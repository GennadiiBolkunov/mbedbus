#include "mbedbus/Proxy.h"
#include "mbedbus/Log.h"

namespace mbedbus {

Proxy::Proxy(std::shared_ptr<Connection> conn,
    const std::string& destination,
    const std::string& objectPath,
    const std::string& interface)
    : conn_(std::move(conn))
    , dest_(destination)
    , path_(objectPath)
    , iface_(interface)
    , timeoutMs_(-1)
{}

Proxy::~Proxy() {
    for (auto& rule : matchRules_) {
        try {
            conn_->removeMatch(rule);
        } catch (...) {
            MBEDBUS_LOG("Failed to remove match rule during Proxy destruction");
        }
    }
}

std::shared_ptr<Proxy> Proxy::create(std::shared_ptr<Connection> conn,
    const std::string& destination,
    const std::string& objectPath,
    const std::string& interface) {
    return std::shared_ptr<Proxy>(new Proxy(std::move(conn), destination,
        objectPath, interface));
}

std::map<std::string, Variant> Proxy::getAllProperties() {
    Message msg = Message::createMethodCall(dest_, path_,
        "org.freedesktop.DBus.Properties", "GetAll");
    msg.appendArgs(iface_);
    Message reply = conn_->sendWithReplyAndBlock(msg, timeoutMs_);
    return reply.readArg<std::map<std::string, Variant>>();
}

void Proxy::onPropertiesChanged(
    std::function<void(const std::string&,
        const std::map<std::string, Variant>&,
        const std::vector<std::string>&)> handler)
{
    std::string matchRule = "type='signal',sender='" + dest_ +
                            "',path='" + path_ +
                            "',interface='org.freedesktop.DBus.Properties'"
                            ",member='PropertiesChanged'";
    conn_->addMatch(matchRule);
    matchRules_.emplace_back(matchRule);

    std::string myPath = path_;
    conn_->addFilter([handler, myPath](const Message& msg) -> bool {
        if (msg.type() != DBUS_MESSAGE_TYPE_SIGNAL) return false;
        const char* mi = msg.interface();
        const char* mm = msg.member();
        const char* mp = msg.path();
        if (!mi || !mm || !mp) return false;
        if (std::string(mi) != "org.freedesktop.DBus.Properties") return false;
        if (std::string(mm) != "PropertiesChanged") return false;
        if (std::string(mp) != myPath) return false;

        try {
            auto args = msg.readArgs<std::string,
                std::map<std::string, Variant>,
                std::vector<std::string>>();
            handler(std::get<0>(args), std::get<1>(args), std::get<2>(args));
        } catch (const std::exception& e) {
            MBEDBUS_LOG("PropertiesChanged handler error: %s", e.what());
        } catch (...) {
            MBEDBUS_LOG("PropertiesChanged handler: unknown error");
        }
        return false;
    });
}

void Proxy::onInterfacesAdded(
    std::function<void(const ObjectPath&,
        const std::map<std::string, std::map<std::string, Variant>>&)> handler)
{
    std::string matchRule = "type='signal',sender='" + dest_ +
                            "',interface='org.freedesktop.DBus.ObjectManager'"
                            ",member='InterfacesAdded'";
    conn_->addMatch(matchRule);
    matchRules_.emplace_back(matchRule);

    conn_->addFilter([handler](const Message& msg) -> bool {
        if (msg.type() != DBUS_MESSAGE_TYPE_SIGNAL) return false;
        const char* mi = msg.interface();
        const char* mm = msg.member();
        if (!mi || !mm) return false;
        if (std::string(mi) != "org.freedesktop.DBus.ObjectManager") return false;
        if (std::string(mm) != "InterfacesAdded") return false;

        try {
            auto args = msg.readArgs<ObjectPath,
                std::map<std::string, std::map<std::string, Variant>>>();
            handler(std::get<0>(args), std::get<1>(args));
        } catch (const std::exception& e) {
            MBEDBUS_LOG("InterfacesAdded handler error: %s", e.what());
        } catch (...) {
            MBEDBUS_LOG("InterfacesAdded handler: unknown error");
        }
        return false;
    });
}

void Proxy::onInterfacesRemoved(
    std::function<void(const ObjectPath&,
        const std::vector<std::string>&)> handler)
{
    std::string matchRule = "type='signal',sender='" + dest_ +
                            "',interface='org.freedesktop.DBus.ObjectManager'"
                            ",member='InterfacesRemoved'";
    conn_->addMatch(matchRule);
    matchRules_.emplace_back(matchRule);

    conn_->addFilter([handler](const Message& msg) -> bool {
        if (msg.type() != DBUS_MESSAGE_TYPE_SIGNAL) return false;
        const char* mi = msg.interface();
        const char* mm = msg.member();
        if (!mi || !mm) return false;
        if (std::string(mi) != "org.freedesktop.DBus.ObjectManager") return false;
        if (std::string(mm) != "InterfacesRemoved") return false;

        try {
            auto args = msg.readArgs<ObjectPath, std::vector<std::string>>();
            handler(std::get<0>(args), std::get<1>(args));
        } catch (const std::exception& e) {
            MBEDBUS_LOG("InterfacesRemoved handler error: %s", e.what());
        } catch (...) {
            MBEDBUS_LOG("InterfacesRemoved handler: unknown error");
        }
        return false;
    });
}

std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>>
Proxy::getManagedObjects() {
    Message msg = Message::createMethodCall(dest_, path_,
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    Message reply = conn_->sendWithReplyAndBlock(msg, timeoutMs_);
    return reply.readArg<
        std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>>>();
}

} // namespace mbedbus
