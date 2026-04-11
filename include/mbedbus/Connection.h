/// @file Connection.h
/// @brief RAII wrapper around DBusConnection* with event loop support.

#ifndef MBEDBUS_CONNECTION_H
#define MBEDBUS_CONNECTION_H

#include <dbus/dbus.h>
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <mutex>

#include "Types.h"
#include "Error.h"
#include "Message.h"

namespace mbedbus {

/// @brief RAII wrapper around DBusConnection*. Move-only.
class Connection : public std::enable_shared_from_this<Connection> {
public:
    /// @brief Create a shared system bus connection.
    static std::shared_ptr<Connection> createSystemBus();

    /// @brief Create a shared session bus connection.
    static std::shared_ptr<Connection> createSessionBus();

    /// @brief Create a private connection to the given address.
    static std::shared_ptr<Connection> createPrivate(const std::string& address);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    /// @brief Request a well-known bus name.
    /// @param name The name to claim (e.g. "com.example.MyService").
    /// @param flags DBUS_NAME_FLAG_* flags (default: DBUS_NAME_FLAG_DO_NOT_QUEUE).
    void requestName(const std::string& name,
                     unsigned int flags = DBUS_NAME_FLAG_DO_NOT_QUEUE);

    /// @brief Release a previously claimed bus name.
    void releaseName(const std::string& name);

    /// @brief Get the unique name of this connection.
    std::string uniqueName() const;

    /// @brief Send a message on the bus (fire-and-forget).
    /// @return The serial number assigned to the message.
    uint32_t send(const Message& msg);

    /// @brief Send a message and wait for a reply (blocking).
    /// @param msg The method call message to send.
    /// @param timeoutMs Timeout in milliseconds (-1 for default).
    /// @return The reply message.
    Message sendWithReplyAndBlock(const Message& msg, int timeoutMs = -1);

    /// @brief Send a message and register an async reply callback.
    void sendWithReplyAsync(const Message& msg,
                            std::function<void(Message)> callback,
                            int timeoutMs = -1);

    /// @brief Add a match rule for signal filtering.
    void addMatch(const std::string& rule);

    /// @brief Remove a match rule.
    void removeMatch(const std::string& rule);

    /// @brief Add a message filter function. Returns an opaque handle.
    using FilterFunction = std::function<bool(const Message&)>;
    void addFilter(FilterFunction filter);

    /// @brief Register an object path with a vtable.
    void registerObjectPath(const std::string& path,
                            const DBusObjectPathVTable& vtable,
                            void* userData);

    /// @brief Unregister an object path.
    void unregisterObjectPath(const std::string& path);

    /// @brief Enter a blocking event loop. Call requestStop() to exit.
    void enterEventLoop();

    /// @brief Process pending D-Bus events. Returns true if still connected.
    /// @param timeoutMs Timeout in milliseconds for dbus_connection_read_write.
    bool processPendingEvents(int timeoutMs = 0);

    /// @brief Request the event loop to stop.
    void requestStop();

    /// @brief Get the Unix file descriptor for poll/epoll integration.
    int getUnixFd() const;

    /// @brief Flush pending outgoing messages.
    void flush();

    /// @brief Get the raw DBusConnection* (borrowed).
    DBusConnection* raw() const { return conn_; }

private:
    Connection();
    static std::shared_ptr<Connection> create(DBusBusType busType);

    DBusConnection* conn_;
    bool isPrivate_;
    std::atomic<bool> stopRequested_;
    std::mutex filterMutex_;
    std::vector<FilterFunction> filters_;

    static DBusHandlerResult filterDispatch(DBusConnection* conn,
                                            DBusMessage* msg, void* data);
};

} // namespace mbedbus

#endif // MBEDBUS_CONNECTION_H
