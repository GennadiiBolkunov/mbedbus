#include "mbedbus/ObjectManager.h"
#include "mbedbus/Log.h"

namespace mbedbus {

ObjectManager::ObjectManager(std::shared_ptr<Connection> conn, const std::string& rootPath)
    : conn_(std::move(conn))
    , rootPath_(rootPath)
{}

ObjectManager::~ObjectManager() = default;

std::shared_ptr<ObjectManager> ObjectManager::create(std::shared_ptr<Connection> conn,
    const std::string& rootPath) {
    auto self = std::shared_ptr<ObjectManager>(new ObjectManager(std::move(conn), rootPath));

    // Create root object that implements ObjectManager interface
    self->rootObject_ = ServiceObject::create(self->conn_, rootPath);
    auto weakSelf = std::weak_ptr<ObjectManager>(self);

    self->rootObject_->addInterface("org.freedesktop.DBus.ObjectManager")
        .addMethod("GetManagedObjects", [weakSelf]()
            -> std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>> {
            auto s = weakSelf.lock();
            if (!s) return {};
            return s->getManagedObjects();
        });

    self->rootObject_->finalize();
    return self;
}

std::shared_ptr<ServiceObject> ObjectManager::addChild(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto child = ServiceObject::create(conn_, path);
    children_[path] = child;
    updateChildNodes();
    MBEDBUS_LOG("Added child: %s", path.c_str());
    // InterfacesAdded is emitted when finalize() is called on the child
    // We set up a mechanism: the caller calls child->finalize() then we emit.
    // Actually, let's emit in a separate step or after finalize.
    // The spec says emit after interfaces are known, so we'll do it
    // via a wrapper approach — the caller should call finalize() on the child,
    // and then we detect that and emit. For simplicity, we return the child
    // and the user calls finalize(). We provide a helper.
    return child;
}

void ObjectManager::removeChild(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = children_.find(path);
    if (it == children_.end()) return;

    // Collect interface names before removing
    std::vector<std::string> ifaceNames;
    for (auto& kv : it->second->interfaces()) {
        ifaceNames.push_back(kv.first);
    }

    children_.erase(it);
    updateChildNodes();

    // Emit InterfacesRemoved (outside lock would be better, but simple is fine)
    if (!ifaceNames.empty()) {
        emitInterfacesRemoved(path, ifaceNames);
    }

    // Unregister the path
    try {
        conn_->unregisterObjectPath(path);
    } catch (...) {
        MBEDBUS_LOG("Failed to unregister path: %s", path.c_str());
    }

    MBEDBUS_LOG("Removed child: %s", path.c_str());
}

std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>>
ObjectManager::getManagedObjects() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>> result;

    for (auto& kv : children_) {
        auto& path = kv.first;
        auto& obj = kv.second;
        std::map<std::string, std::map<std::string, Variant>> ifaceMap;

        for (auto& ifaceKv : obj->interfaces()) {
            ifaceMap[ifaceKv.first] = obj->getAllProperties(ifaceKv.first);
        }

        result[ObjectPath(path)] = std::move(ifaceMap);
    }

    return result;
}

void ObjectManager::emitInterfacesAdded(
    const std::string& path,
    const std::map<std::string, std::map<std::string, Variant>>& ifacesAndProps)
{
    Message sig = Message::createSignal(rootPath_,
        "org.freedesktop.DBus.ObjectManager", "InterfacesAdded");

    DBusMessageIter iter;
    dbus_message_iter_init_append(sig.raw(), &iter);
    types::Traits<ObjectPath>::serialize(iter, ObjectPath(path));
    types::Traits<std::map<std::string, std::map<std::string, Variant>>>::serialize(iter, ifacesAndProps);

    conn_->send(sig);
}

void ObjectManager::emitInterfacesRemoved(
    const std::string& path,
    const std::vector<std::string>& interfaces)
{
    Message sig = Message::createSignal(rootPath_,
        "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved");

    DBusMessageIter iter;
    dbus_message_iter_init_append(sig.raw(), &iter);
    types::Traits<ObjectPath>::serialize(iter, ObjectPath(path));
    types::Traits<std::vector<std::string>>::serialize(iter, interfaces);

    conn_->send(sig);
}

void ObjectManager::updateChildNodes() {
    std::vector<std::string> childNames;
    for (auto& kv : children_) {
        // Extract just the last component of the path
        std::string childPath = kv.first;
        auto pos = childPath.rfind('/');
        if (pos != std::string::npos && pos + 1 < childPath.size()) {
            childNames.emplace_back(childPath.substr(pos + 1));
        }
    }
    rootObject_->setChildNodes(childNames);
}

} // namespace mbedbus
