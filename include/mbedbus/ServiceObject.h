/// @file ServiceObject.h
/// @brief Server-side D-Bus object that exports interfaces with methods, properties, and signals.

#ifndef MBEDBUS_SERVICEOBJECT_H
#define MBEDBUS_SERVICEOBJECT_H

#include <dbus/dbus.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <mutex>

#include "Types.h"
#include "Error.h"
#include "Message.h"
#include "Connection.h"
#include "InterfaceBuilder.h"
#include "Variant.h"

namespace mbedbus {

/// @brief A server-side D-Bus object registered at a specific path.
/// Supports methods, properties, signals, and standard D-Bus interfaces.
class ServiceObject : public std::enable_shared_from_this<ServiceObject> {
public:
    /// @brief Create a new ServiceObject on the given connection and path.
    static std::shared_ptr<ServiceObject> create(std::shared_ptr<Connection> conn, const std::string& path);

    ~ServiceObject();

    ServiceObject(const ServiceObject&) = delete;
    ServiceObject& operator=(const ServiceObject&) = delete;

    /// @brief Begin defining an interface. Returns a fluent InterfaceBuilder.
    InterfaceBuilder addInterface(const std::string& name);

    /// @brief Finalize the object: register it on the bus and enable message handling.
    void finalize();

    /// @brief Unregister the object from the bus. Safe to call multiple times.
    /// After this call the destructor will not attempt to unregister again.
    void unregister();

    /// @brief Emit a signal on the given interface.
    template<typename... Args>
    void emitSignal(const std::string& iface, const std::string& signalName,
        const Args&... args) {
        Message sig = Message::createSignal(path_, iface, signalName);
        sig.appendArgs(args...);
        conn_->send(sig);
    }

    /// @brief Emit PropertiesChanged signal for the given interface.
    void emitPropertiesChanged(const std::string& iface,
        const std::map<std::string, Variant>& changed,
        const std::vector<std::string>& invalidated);

    /// @brief Get the object path.
    const std::string& path() const { return path_; }

    /// @brief Get all registered interfaces (for ObjectManager / introspection).
    const std::map<std::string, InterfaceData>& interfaces() const { return interfaces_; }

    /// @brief Get all properties for an interface as a map.
    std::map<std::string, Variant> getAllProperties(const std::string& iface) const;

    /// @brief Generate introspection XML for this object.
    std::string introspect() const;

    /// @brief Set child path names for introspection output.
    void setChildNodes(const std::vector<std::string>& children);

private:
    ServiceObject(std::shared_ptr<Connection> conn, const std::string& path);

    Message handleMessage(const Message& msg);
    Message handleMethodCall(const Message& msg);
    Message handlePropertiesGet(const Message& msg);
    Message handlePropertiesSet(const Message& msg);
    Message handlePropertiesGetAll(const Message& msg);
    Message handleIntrospect(const Message& msg);
    Message handlePeerPing(const Message& msg);
    Message handlePeerGetMachineId(const Message& msg);

    static DBusHandlerResult messageHandler(DBusConnection* conn, DBusMessage* msg, void* data);
    static void unregisterHandler(DBusConnection* conn, void* data);

    std::shared_ptr<Connection> conn_;
    std::string path_;
    std::map<std::string, InterfaceData> interfaces_;
    mutable std::mutex mutex_;
    bool finalized_;
    std::vector<std::string> childNodes_;
};

} // namespace mbedbus

#endif // MBEDBUS_SERVICEOBJECT_H
