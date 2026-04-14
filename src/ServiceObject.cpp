#include "mbedbus/ServiceObject.h"
#include "mbedbus/Log.h"
#include <fstream>
#include <sstream>

namespace mbedbus {

ServiceObject::ServiceObject(std::shared_ptr<Connection> conn, const std::string& path)
    : conn_(std::move(conn))
    , path_(path)
    , finalized_(false)
{}

ServiceObject::~ServiceObject() {
    unregister();
}

std::shared_ptr<ServiceObject> ServiceObject::create(std::shared_ptr<Connection> conn,
    const std::string& path) {
    return std::shared_ptr<ServiceObject>(new ServiceObject(std::move(conn), path));
}

InterfaceBuilder ServiceObject::addInterface(const std::string& name) {
    auto& data = interfaces_[name];
    data.name = name;
    return InterfaceBuilder(data, this);
}

void ServiceObject::finalize() {
    if (finalized_) return;

    static DBusObjectPathVTable vtable;
    vtable.message_function = &ServiceObject::messageHandler;
    vtable.unregister_function = &ServiceObject::unregisterHandler;

    conn_->registerObjectPath(path_, vtable, this);
    finalized_ = true;
    MBEDBUS_LOG("Finalized object: %s", path_.c_str());
}

void ServiceObject::unregister() {
    if (!finalized_ || !conn_) return;
    finalized_ = false;
    try {
        conn_->unregisterObjectPath(path_);
    } catch (...) {
        MBEDBUS_LOG("Failed to unregister path: %s", path_.c_str());
    }
}

void ServiceObject::emitPropertiesChanged(
    const std::string& iface,
    const std::map<std::string, Variant>& changed,
    const std::vector<std::string>& invalidated)
{
    Message sig = Message::createSignal(path_,
        "org.freedesktop.DBus.Properties", "PropertiesChanged");

    DBusMessageIter iter;
    dbus_message_iter_init_append(sig.raw(), &iter);
    types::Traits<std::string>::serialize(iter, iface);
    types::Traits<std::map<std::string, Variant>>::serialize(iter, changed);
    types::Traits<std::vector<std::string>>::serialize(iter, invalidated);

    conn_->send(sig);
}

std::map<std::string, Variant> ServiceObject::getAllProperties(const std::string& iface) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, Variant> result;
    auto it = interfaces_.find(iface);
    if (it == interfaces_.end()) return result;
    for (auto& prop : it->second.properties) {
        if (prop.readable && prop.getter) {
            try {
                result[prop.name] = prop.getter();
            } catch (const Error& e) {
                // Property getter signalled an error — skip this property
                MBEDBUS_LOG("Property '%s' getter threw: %s", prop.name.c_str(), e.message().c_str());
            } catch (const std::exception& e) {
                MBEDBUS_LOG("Property '%s' getter threw: %s", prop.name.c_str(), e.what());
            }
        }
    }
    return result;
}

void ServiceObject::setChildNodes(const std::vector<std::string>& children) {
    std::lock_guard<std::mutex> lock(mutex_);
    childNodes_ = children;
}

Message ServiceObject::handleMessage(const Message& msg) {
    const char* iface = msg.interface();
    const char* member = msg.member();

    if (!member) {
        return Message::createError(msg, "org.freedesktop.DBus.Error.Failed",
            "No member specified");
    }

    // Standard interfaces
    if (iface) {
        std::string ifaceStr(iface);
        if (ifaceStr == "org.freedesktop.DBus.Introspectable") {
            if (std::string(member) == "Introspect") return handleIntrospect(msg);
        }
        else if (ifaceStr == "org.freedesktop.DBus.Peer") {
            if (std::string(member) == "Ping") return handlePeerPing(msg);
            if (std::string(member) == "GetMachineId") return handlePeerGetMachineId(msg);
        }
        else if (ifaceStr == "org.freedesktop.DBus.Properties") {
            std::string m(member);
            if (m == "Get") return handlePropertiesGet(msg);
            if (m == "Set") return handlePropertiesSet(msg);
            if (m == "GetAll") return handlePropertiesGetAll(msg);
        }
    }

    return handleMethodCall(msg);
}

Message ServiceObject::handleMethodCall(const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* iface = msg.interface();
    const char* member = msg.member();

    if (!member) {
        return Message::createError(msg, "org.freedesktop.DBus.Error.UnknownMethod",
            "No method specified");
    }

    std::string memberStr(member);

    // If interface specified, look only there
    if (iface) {
        auto it = interfaces_.find(iface);
        if (it != interfaces_.end()) {
            for (auto& method : it->second.methods) {
                if (method.name == memberStr) {
                    try {
                        return method.handler(msg);
                    } catch (const Error& e) {
                        return Message::createError(msg, e.name(), e.message());
                    } catch (const std::exception& e) {
                        return Message::createError(msg,
                            "org.freedesktop.DBus.Error.Failed", e.what());
                    }
                }
            }
        }
    } else {
        // Search all interfaces
        for (auto& kv : interfaces_) {
            for (auto& method : kv.second.methods) {
                if (method.name == memberStr) {
                    try {
                        return method.handler(msg);
                    } catch (const Error& e) {
                        return Message::createError(msg, e.name(), e.message());
                    } catch (const std::exception& e) {
                        return Message::createError(msg,
                            "org.freedesktop.DBus.Error.Failed", e.what());
                    }
                }
            }
        }
    }

    return Message::createError(msg, "org.freedesktop.DBus.Error.UnknownMethod",
        std::string("Unknown method: ") + memberStr);
}

Message ServiceObject::handlePropertiesGet(const Message& msg) {
    auto args = msg.readArgs<std::string, std::string>();
    const std::string& ifaceName = std::get<0>(args);
    const std::string& propName = std::get<1>(args);

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = interfaces_.find(ifaceName);
    if (it == interfaces_.end()) {
        return Message::createError(msg,
            "org.freedesktop.DBus.Error.UnknownInterface",
            "Unknown interface: " + ifaceName);
    }

    for (auto& prop : it->second.properties) {
        if (prop.name == propName) {
            if (!prop.readable || !prop.getter) {
                return Message::createError(msg,
                    "org.freedesktop.DBus.Error.Failed",
                    "Property is not readable: " + propName);
            }
            try {
                Variant v = prop.getter();
                Message reply = Message::createMethodReturn(msg);
                reply.appendArgs(v);
                return reply;
            } catch (const Error& e) {
                return Message::createError(msg, e.name(), e.message());
            } catch (const std::exception& e) {
                return Message::createError(msg,
                    "org.freedesktop.DBus.Error.Failed", e.what());
            }
        }
    }

    return Message::createError(msg,
        "org.freedesktop.DBus.Error.UnknownProperty",
        "Unknown property: " + propName);
}

Message ServiceObject::handlePropertiesSet(const Message& msg) {
    DBusMessageIter iter;
    dbus_message_iter_init(msg.raw(), &iter);

    std::string ifaceName = types::Traits<std::string>::deserialize(iter);
    dbus_message_iter_next(&iter);
    std::string propName = types::Traits<std::string>::deserialize(iter);
    dbus_message_iter_next(&iter);
    Variant value = types::Traits<Variant>::deserialize(iter);

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = interfaces_.find(ifaceName);
    if (it == interfaces_.end()) {
        return Message::createError(msg,
            "org.freedesktop.DBus.Error.UnknownInterface",
            "Unknown interface: " + ifaceName);
    }

    for (auto& prop : it->second.properties) {
        if (prop.name == propName) {
            if (!prop.writable || !prop.setter) {
                return Message::createError(msg,
                    "org.freedesktop.DBus.Error.PropertyReadOnly",
                    "Property is read-only: " + propName);
            }
            try {
                prop.setter(value);
            } catch (const Error& e) {
                return Message::createError(msg, e.name(), e.message());
            } catch (const std::exception& e) {
                return Message::createError(msg,
                    "org.freedesktop.DBus.Error.Failed", e.what());
            }
            return Message::createMethodReturn(msg);
        }
    }

    return Message::createError(msg,
        "org.freedesktop.DBus.Error.UnknownProperty",
        "Unknown property: " + propName);
}

Message ServiceObject::handlePropertiesGetAll(const Message& msg) {
    std::string ifaceName = msg.readArg<std::string>();

    // getAllProperties already catches exceptions per-property
    auto props = getAllProperties(ifaceName);

    Message reply = Message::createMethodReturn(msg);
    reply.appendArgs(props);
    return reply;
}

Message ServiceObject::handleIntrospect(const Message& msg) {
    std::string xml = introspect();
    Message reply = Message::createMethodReturn(msg);
    reply.appendArgs(xml);
    return reply;
}

Message ServiceObject::handlePeerPing(const Message& msg) {
    return Message::createMethodReturn(msg);
}

Message ServiceObject::handlePeerGetMachineId(const Message& msg) {
    std::string machineId;
    std::ifstream f("/etc/machine-id");
    if (f.is_open()) {
        std::getline(f, machineId);
        while (!machineId.empty() &&
               (machineId.back() == '\n' || machineId.back() == '\r' ||
                machineId.back() == ' ')) {
            machineId.pop_back();
        }
    }
    if (machineId.empty()) {
        machineId = "0";
    }
    Message reply = Message::createMethodReturn(msg);
    reply.appendArgs(machineId);
    return reply;
}

DBusHandlerResult ServiceObject::messageHandler(DBusConnection* /*conn*/,
    DBusMessage* rawMsg,
    void* data) {
    auto* self = static_cast<ServiceObject*>(data);

    if (dbus_message_get_type(rawMsg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    Message msg(rawMsg);
    dbus_message_ref(rawMsg); // msg will unref

    // Top-level safety barrier: no C++ exception must escape into libdbus C code.
    // We use raw dbus calls in catch blocks because Message::createError itself
    // could throw if out of memory.
    try {
        Message reply = self->handleMessage(msg);
        if (reply) {
            dbus_connection_send(self->conn_->raw(), reply.raw(), nullptr);
            dbus_connection_flush(self->conn_->raw());
        }
    } catch (const Error& e) {
        DBusMessage* errReply = dbus_message_new_error(
            rawMsg, e.name().c_str(), e.message().c_str());
        if (errReply) {
            dbus_connection_send(self->conn_->raw(), errReply, nullptr);
            dbus_connection_flush(self->conn_->raw());
            dbus_message_unref(errReply);
        }
    } catch (const std::exception& e) {
        DBusMessage* errReply = dbus_message_new_error(
            rawMsg, "org.freedesktop.DBus.Error.Failed", e.what());
        if (errReply) {
            dbus_connection_send(self->conn_->raw(), errReply, nullptr);
            dbus_connection_flush(self->conn_->raw());
            dbus_message_unref(errReply);
        }
    } catch (...) {
        DBusMessage* errReply = dbus_message_new_error(
            rawMsg, "org.freedesktop.DBus.Error.Failed", "Unknown internal error");
        if (errReply) {
            dbus_connection_send(self->conn_->raw(), errReply, nullptr);
            dbus_connection_flush(self->conn_->raw());
            dbus_message_unref(errReply);
        }
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

void ServiceObject::unregisterHandler(DBusConnection* /*conn*/, void* /*data*/) {
    MBEDBUS_LOG("Object path unregistered");
}

} // namespace mbedbus
