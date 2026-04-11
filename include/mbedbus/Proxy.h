/// @file Proxy.h
/// @brief Client-side proxy for calling D-Bus methods, accessing properties, and subscribing to signals.

#ifndef MBEDBUS_PROXY_H
#define MBEDBUS_PROXY_H

#include <dbus/dbus.h>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

#include "Types.h"
#include "Error.h"
#include "Message.h"
#include "Connection.h"
#include "Variant.h"
#include "TypeTraits.h"

namespace mbedbus {

/// @brief Client-side proxy for a remote D-Bus object.
class Proxy : public std::enable_shared_from_this<Proxy> {
public:
    /// @brief Create a proxy for a remote object.
    /// @param conn The D-Bus connection to use.
    /// @param destination The bus name of the remote service.
    /// @param objectPath The object path.
    /// @param interface The default interface for method calls.
    static std::shared_ptr<Proxy> create(std::shared_ptr<Connection> conn,
                                          const std::string& destination,
                                          const std::string& objectPath,
                                          const std::string& interface);

    ~Proxy();

    Proxy(const Proxy&) = delete;
    Proxy& operator=(const Proxy&) = delete;

    // ---- Synchronous method calls ----

    /// @brief Call a method and return the result. Throws Error on failure.
    template<typename R, typename... Args>
    R call(const std::string& method, const Args&... args) {
        Message msg = Message::createMethodCall(dest_, path_, iface_, method);
        msg.appendArgs(args...);
        Message reply = conn_->sendWithReplyAndBlock(msg, timeoutMs_);
        return reply.readArg<R>();
    }

    /// @brief Call a void method. Throws Error on failure.
    template<typename... Args>
    void callVoid(const std::string& method, const Args&... args) {
        Message msg = Message::createMethodCall(dest_, path_, iface_, method);
        msg.appendArgs(args...);
        conn_->sendWithReplyAndBlock(msg, timeoutMs_);
    }

    /// @brief Call a method, returning Expected<R> instead of throwing.
    template<typename R, typename... Args>
    Expected<R> tryCall(const std::string& method, const Args&... args) {
        try {
            return Expected<R>(call<R>(method, args...));
        } catch (const Error& e) {
            return Expected<R>(e);
        }
    }

    // ---- Async method calls ----

    /// @brief Call a method asynchronously, invoking callback with the result.
    template<typename R, typename... Args>
    void callAsync(const std::string& method,
                   std::function<void(Expected<R>)> callback,
                   const Args&... args) {
        Message msg = Message::createMethodCall(dest_, path_, iface_, method);
        msg.appendArgs(args...);
        conn_->sendWithReplyAsync(msg,
            [callback](Message reply) {
                if (reply.isError()) {
                    const char* ename = reply.errorName();
                    DBusError err;
                    dbus_error_init(&err);
                    dbus_set_error(&err, ename ? ename : "unknown",
                                   "%s", reply.path() ? reply.path() : "");
                    // Read error message from body
                    std::string errMsg;
                    try { errMsg = reply.readArg<std::string>(); }
                    catch (...) { errMsg = ename ? ename : "Unknown error"; }
                    dbus_error_free(&err);
                    callback(Expected<R>(Error(ename ? ename : "unknown", errMsg)));
                } else {
                    try {
                        R val = reply.readArg<R>();
                        callback(Expected<R>(std::move(val)));
                    } catch (const Error& e) {
                        callback(Expected<R>(e));
                    }
                }
            },
            timeoutMs_);
    }

    // ---- Properties ----

    /// @brief Get a property value.
    template<typename T>
    T getProperty(const std::string& propertyName) {
        Message msg = Message::createMethodCall(dest_, path_,
            "org.freedesktop.DBus.Properties", "Get");
        msg.appendArgs(iface_, propertyName);
        Message reply = conn_->sendWithReplyAndBlock(msg, timeoutMs_);
        Variant v = reply.readArg<Variant>();
        return v.get<T>();
    }

    /// @brief Set a property value.
    template<typename T>
    void setProperty(const std::string& propertyName, const T& value) {
        Message msg = Message::createMethodCall(dest_, path_,
            "org.freedesktop.DBus.Properties", "Set");
        DBusMessageIter iter;
        dbus_message_iter_init_append(msg.raw(), &iter);
        types::Traits<std::string>::serialize(iter, iface_);
        types::Traits<std::string>::serialize(iter, propertyName);
        Variant v(value);
        types::Traits<Variant>::serialize(iter, v);
        conn_->sendWithReplyAndBlock(msg, timeoutMs_);
    }

    /// @brief Get all properties for the default interface.
    std::map<std::string, Variant> getAllProperties();

    // ---- Signals ----

    /// @brief Subscribe to a signal on the default interface.
    template<typename... Args>
    void onSignal(const std::string& signalName,
                  std::function<void(Args...)> handler) {
        std::string matchRule = "type='signal',sender='" + dest_ +
            "',path='" + path_ + "',interface='" + iface_ +
            "',member='" + signalName + "'";
        conn_->addMatch(matchRule);
        matchRules_.push_back(matchRule);

        auto wrappedHandler = [handler](const Message& msg) -> bool {
            DBusMessageIter iter;
            if (!dbus_message_iter_init(msg.raw(), &iter)) return false;
            try {
                auto args = types::deserializeArgs<Args...>(iter);
                applyHandler(handler, std::move(args),
                    std::index_sequence_for<Args...>{});
                return true;
            } catch (...) {
                return false;
            }
        };

        std::string sigName = signalName;
        std::string myIface = iface_;
        std::string myPath = path_;
        conn_->addFilter([wrappedHandler, sigName, myIface, myPath](const Message& msg) -> bool {
            if (msg.type() != DBUS_MESSAGE_TYPE_SIGNAL) return false;
            const char* mi = msg.interface();
            const char* mm = msg.member();
            const char* mp = msg.path();
            if (!mi || !mm || !mp) return false;
            if (myIface != mi || sigName != mm || myPath != mp) return false;
            wrappedHandler(msg);
            return false; // Don't consume; let other handlers see it
        });
    }

    /// @brief Subscribe to PropertiesChanged signal.
    void onPropertiesChanged(
        std::function<void(const std::string& iface,
                           const std::map<std::string, Variant>& changed,
                           const std::vector<std::string>& invalidated)> handler);

    /// @brief Subscribe to InterfacesAdded signal (ObjectManager).
    void onInterfacesAdded(
        std::function<void(const ObjectPath& path,
                           const std::map<std::string, std::map<std::string, Variant>>& ifaces)> handler);

    /// @brief Subscribe to InterfacesRemoved signal (ObjectManager).
    void onInterfacesRemoved(
        std::function<void(const ObjectPath& path,
                           const std::vector<std::string>& ifaces)> handler);

    /// @brief Call GetManagedObjects on the remote ObjectManager.
    std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>>
    getManagedObjects();

    /// @brief Set the default timeout for calls (in ms).
    void setTimeout(int timeoutMs) { timeoutMs_ = timeoutMs; }

private:
    Proxy(std::shared_ptr<Connection> conn,
          const std::string& destination,
          const std::string& objectPath,
          const std::string& interface);

    template<typename... Args, std::size_t... I>
    static void applyHandler(const std::function<void(Args...)>& handler,
                             std::tuple<Args...>&& args,
                             std::index_sequence<I...>) {
        handler(std::get<I>(std::move(args))...);
    }

    std::shared_ptr<Connection> conn_;
    std::string dest_;
    std::string path_;
    std::string iface_;
    int timeoutMs_;
    std::vector<std::string> matchRules_;
};

} // namespace mbedbus

#endif // MBEDBUS_PROXY_H
