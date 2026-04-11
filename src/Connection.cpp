#include "mbedbus/Connection.h"
#include "mbedbus/Log.h"

namespace mbedbus {

Connection::Connection()
    : conn_(nullptr)
    , isPrivate_(false)
    , stopRequested_(false)
{}

Connection::~Connection() {
    if (conn_) {
        // Remove our filter
        dbus_connection_remove_filter(conn_, &Connection::filterDispatch, this);

        if (isPrivate_) {
            dbus_connection_close(conn_);
        }
        dbus_connection_unref(conn_);
        conn_ = nullptr;
    }
}

std::shared_ptr<Connection> Connection::create(DBusBusType busType) {
    DBusError err;
    dbus_error_init(&err);

    auto self = std::shared_ptr<Connection>(new Connection());
    self->conn_ = dbus_bus_get(busType, &err);
    if (dbus_error_is_set(&err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to connect to bus: " + msg);
    }
    if (!self->conn_) {
        throw Error("org.freedesktop.DBus.Error.Failed", "Failed to connect to bus");
    }

    // Don't exit the process if the connection is lost
    dbus_connection_set_exit_on_disconnect(self->conn_, FALSE);

    // Install our global filter
    if (!dbus_connection_add_filter(self->conn_, &Connection::filterDispatch,
                                     self.get(), nullptr)) {
        throw Error("com.mbedbus.Error", "Failed to add connection filter");
    }

    MBEDBUS_LOG("Connected to bus, unique name: %s",
                dbus_bus_get_unique_name(self->conn_));
    return self;
}

std::shared_ptr<Connection> Connection::createSystemBus() {
    return create(DBUS_BUS_SYSTEM);
}

std::shared_ptr<Connection> Connection::createSessionBus() {
    return create(DBUS_BUS_SESSION);
}

std::shared_ptr<Connection> Connection::createPrivate(const std::string& address) {
    DBusError err;
    dbus_error_init(&err);

    auto self = std::shared_ptr<Connection>(new Connection());
    self->isPrivate_ = true;
    self->conn_ = dbus_connection_open_private(address.c_str(), &err);
    if (dbus_error_is_set(&err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to open private connection: " + msg);
    }
    if (!self->conn_) {
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to open private connection");
    }

    // Register on the bus
    if (!dbus_bus_register(self->conn_, &err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to register on bus: " + msg);
    }

    dbus_connection_set_exit_on_disconnect(self->conn_, FALSE);

    if (!dbus_connection_add_filter(self->conn_, &Connection::filterDispatch,
                                     self.get(), nullptr)) {
        throw Error("com.mbedbus.Error", "Failed to add connection filter");
    }

    return self;
}

void Connection::requestName(const std::string& name, unsigned int flags) {
    DBusError err;
    dbus_error_init(&err);

    int ret = dbus_bus_request_name(conn_, name.c_str(), flags, &err);
    if (dbus_error_is_set(&err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to request name '" + name + "': " + msg);
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER &&
        ret != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to become primary owner of '" + name + "'");
    }

    MBEDBUS_LOG("Acquired name: %s", name.c_str());
}

void Connection::releaseName(const std::string& name) {
    DBusError err;
    dbus_error_init(&err);
    dbus_bus_release_name(conn_, name.c_str(), &err);
    if (dbus_error_is_set(&err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("org.freedesktop.DBus.Error.Failed",
                     "Failed to release name: " + msg);
    }
}

std::string Connection::uniqueName() const {
    const char* name = dbus_bus_get_unique_name(conn_);
    return name ? name : "";
}

uint32_t Connection::send(const Message& msg) {
    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn_, msg.raw(), &serial)) {
        throw Error("com.mbedbus.Error", "Failed to send message (out of memory)");
    }
    dbus_connection_flush(conn_);
    return serial;
}

Message Connection::sendWithReplyAndBlock(const Message& msg, int timeoutMs) {
    DBusError err;
    dbus_error_init(&err);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg.raw(), timeoutMs, &err);

    if (dbus_error_is_set(&err)) {
        std::string name = err.name ? err.name : "unknown";
        std::string message = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error(name, message);
    }

    if (!reply) {
        throw Error("com.mbedbus.Error", "No reply received");
    }

    return Message(reply);
}

void Connection::sendWithReplyAsync(const Message& msg,
                                     std::function<void(Message)> callback,
                                     int timeoutMs) {
    DBusPendingCall* pending = nullptr;
    if (!dbus_connection_send_with_reply(conn_, msg.raw(), &pending, timeoutMs)) {
        throw Error("com.mbedbus.Error", "Failed to send async message");
    }
    if (!pending) {
        throw Error("com.mbedbus.Error", "Connection disconnected");
    }

    // Store the callback on the heap
    auto* cb = new std::function<void(Message)>(std::move(callback));

    dbus_pending_call_set_notify(pending,
        [](DBusPendingCall* p, void* data) {
            auto* callback = static_cast<std::function<void(Message)>*>(data);
            DBusMessage* reply = dbus_pending_call_steal_reply(p);
            if (reply) {
                (*callback)(Message(reply));
            }
            delete callback;
            dbus_pending_call_unref(p);
        },
        cb, nullptr);

    dbus_connection_flush(conn_);
}

void Connection::addMatch(const std::string& rule) {
    DBusError err;
    dbus_error_init(&err);
    dbus_bus_add_match(conn_, rule.c_str(), &err);
    if (dbus_error_is_set(&err)) {
        std::string msg = err.message ? err.message : "Unknown error";
        dbus_error_free(&err);
        throw Error("com.mbedbus.Error", "Failed to add match: " + msg);
    }
    dbus_connection_flush(conn_);
}

void Connection::removeMatch(const std::string& rule) {
    DBusError err;
    dbus_error_init(&err);
    dbus_bus_remove_match(conn_, rule.c_str(), &err);
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        // Ignore errors on removal
    }
}

void Connection::addFilter(FilterFunction filter) {
    std::lock_guard<std::mutex> lock(filterMutex_);
    filters_.push_back(std::move(filter));
}

void Connection::registerObjectPath(const std::string& path,
                                     const DBusObjectPathVTable& vtable,
                                     void* userData) {
    if (!dbus_connection_register_object_path(conn_, path.c_str(),
                                               &vtable, userData)) {
        throw Error("com.mbedbus.Error",
                     "Failed to register object path: " + path);
    }
    MBEDBUS_LOG("Registered object path: %s", path.c_str());
}

void Connection::unregisterObjectPath(const std::string& path) {
    dbus_connection_unregister_object_path(conn_, path.c_str());
}

void Connection::enterEventLoop() {
    stopRequested_ = false;
    MBEDBUS_LOG("Entering event loop");
    while (!stopRequested_.load()) {
        if (!dbus_connection_read_write_dispatch(conn_, 100)) {
            break; // disconnected
        }
    }
    MBEDBUS_LOG("Exiting event loop");
}

bool Connection::processPendingEvents(int timeoutMs) {
    if (!dbus_connection_read_write(conn_, timeoutMs)) {
        return false; // disconnected
    }
    while (dbus_connection_dispatch(conn_) == DBUS_DISPATCH_DATA_REMAINS) {
        // keep dispatching
    }
    return true;
}

void Connection::requestStop() {
    stopRequested_ = true;
}

int Connection::getUnixFd() const {
    int fd = -1;
    if (!dbus_connection_get_unix_fd(conn_, &fd)) {
        throw Error("com.mbedbus.Error", "Failed to get Unix FD");
    }
    return fd;
}

void Connection::flush() {
    dbus_connection_flush(conn_);
}

DBusHandlerResult Connection::filterDispatch(DBusConnection* /*conn*/,
                                              DBusMessage* msg, void* data) {
    auto* self = static_cast<Connection*>(data);
    Message wrapped(msg);
    dbus_message_ref(msg); // wrapped will unref, but we don't own it here

    std::lock_guard<std::mutex> lock(self->filterMutex_);
    for (auto& filter : self->filters_) {
        if (filter(wrapped)) {
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

} // namespace mbedbus
