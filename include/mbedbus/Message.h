/// @file Message.h
/// @brief RAII wrapper around DBusMessage*.

#ifndef MBEDBUS_MESSAGE_H
#define MBEDBUS_MESSAGE_H

#include <dbus/dbus.h>
#include <string>
#include <tuple>
#include <utility>

#include "Types.h"
#include "Error.h"
#include "TypeTraits.h"
#include "Variant.h"

namespace mbedbus {

/// @brief RAII wrapper around DBusMessage*. Move-only.
class Message {
public:
    /// @brief Construct an empty (null) message.
    Message() : msg_(nullptr) {}

    /// @brief Take ownership of a raw DBusMessage*. The message will be unref'd in destructor.
    explicit Message(DBusMessage* msg) : msg_(msg) {}

    ~Message() { if (msg_) dbus_message_unref(msg_); }

    Message(const Message& o) : msg_(o.msg_) { if (msg_) dbus_message_ref(msg_); }

    Message& operator=(const Message& o) {
        if (this != &o) {
            if (msg_) dbus_message_unref(msg_);
            msg_ = o.msg_;
            if (msg_) dbus_message_ref(msg_);
        }
        return *this;
    }

    Message(Message&& o) noexcept : msg_(o.msg_) { o.msg_ = nullptr; }

    Message& operator=(Message&& o) noexcept {
        if (this != &o) {
            if (msg_) dbus_message_unref(msg_);
            msg_ = o.msg_;
            o.msg_ = nullptr;
        }
        return *this;
    }

    /// @brief Check if message is valid (non-null).
    explicit operator bool() const { return msg_ != nullptr; }

    /// @brief Get the raw pointer (borrowed).
    DBusMessage* raw() const { return msg_; }

    /// @brief Release ownership and return the raw pointer.
    DBusMessage* release() { auto* m = msg_; msg_ = nullptr; return m; }

    /// @brief Get message type (DBUS_MESSAGE_TYPE_*).
    int type() const { return msg_ ? dbus_message_get_type(msg_) : DBUS_MESSAGE_TYPE_INVALID; }

    /// @brief Get interface name.
    const char* interface() const { return msg_ ? dbus_message_get_interface(msg_) : nullptr; }

    /// @brief Get member (method/signal) name.
    const char* member() const { return msg_ ? dbus_message_get_member(msg_) : nullptr; }

    /// @brief Get object path.
    const char* path() const { return msg_ ? dbus_message_get_path(msg_) : nullptr; }

    /// @brief Get sender.
    const char* sender() const { return msg_ ? dbus_message_get_sender(msg_) : nullptr; }

    /// @brief Get destination.
    const char* destination() const { return msg_ ? dbus_message_get_destination(msg_) : nullptr; }

    /// @brief Get error name (for error messages).
    const char* errorName() const { return msg_ ? dbus_message_get_error_name(msg_) : nullptr; }

    /// @brief Check if this is an error message.
    bool isError() const { return type() == DBUS_MESSAGE_TYPE_ERROR; }

    /// @brief Get the D-Bus signature of the message body.
    const char* bodySignature() const { return msg_ ? dbus_message_get_signature(msg_) : ""; }

    // ---- Argument access ----

    /// @brief Read all arguments as a tuple.
    template<typename... Args>
    std::tuple<Args...> readArgs() const {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(msg_, &iter)) {
            throw Error("org.freedesktop.DBus.Error.InvalidArgs", "No arguments in message");
        }
        return types::deserializeArgs<Args...>(iter);
    }

    /// @brief Read a single argument.
    template<typename T>
    T readArg() const {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(msg_, &iter)) {
            throw Error("org.freedesktop.DBus.Error.InvalidArgs", "No arguments in message");
        }
        return types::Traits<T>::deserialize(iter);
    }

    /// @brief Append arguments to the message.
    template<typename... Args>
    void appendArgs(const Args&... args) {
        DBusMessageIter iter;
        dbus_message_iter_init_append(msg_, &iter);
        types::serializeArgs(iter, args...);
    }

    /// @brief Set the "no reply expected" flag.
    void setNoReply(bool noReply) {
        dbus_message_set_no_reply(msg_, noReply ? TRUE : FALSE);
    }

    // ---- Factory methods ----

    /// @brief Create a method call message.
    static Message createMethodCall(const std::string& dest, const std::string& path,
                                    const std::string& iface, const std::string& method) {
        DBusMessage* m = dbus_message_new_method_call(
            dest.c_str(), path.c_str(), iface.c_str(), method.c_str());
        if (!m) throw Error("com.mbedbus.Error", "Failed to create method call message");
        return Message(m);
    }

    /// @brief Create a signal message.
    static Message createSignal(const std::string& path, const std::string& iface,
                                const std::string& member) {
        DBusMessage* m = dbus_message_new_signal(path.c_str(), iface.c_str(), member.c_str());
        if (!m) throw Error("com.mbedbus.Error", "Failed to create signal message");
        return Message(m);
    }

    /// @brief Create a method return message.
    static Message createMethodReturn(const Message& call) {
        DBusMessage* m = dbus_message_new_method_return(call.raw());
        if (!m) throw Error("com.mbedbus.Error", "Failed to create method return");
        return Message(m);
    }

    /// @brief Create an error reply message.
    static Message createError(const Message& call, const std::string& name,
                               const std::string& message) {
        DBusMessage* m = dbus_message_new_error(call.raw(), name.c_str(), message.c_str());
        if (!m) throw Error("com.mbedbus.Error", "Failed to create error message");
        return Message(m);
    }

private:
    DBusMessage* msg_;
};

} // namespace mbedbus

#endif // MBEDBUS_MESSAGE_H
