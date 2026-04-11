/// @file ObjectManager.h
/// @brief Implements org.freedesktop.DBus.ObjectManager for managing child objects.

#ifndef MBEDBUS_OBJECTMANAGER_H
#define MBEDBUS_OBJECTMANAGER_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Types.h"
#include "Connection.h"
#include "ServiceObject.h"
#include "Variant.h"

namespace mbedbus {

/// @brief Manages a tree of child ServiceObjects and implements
/// org.freedesktop.DBus.ObjectManager on the root path.
class ObjectManager : public std::enable_shared_from_this<ObjectManager> {
public:
    /// @brief Create an ObjectManager at the given root path.
    static std::shared_ptr<ObjectManager> create(std::shared_ptr<Connection> conn,
                                                  const std::string& rootPath);

    ~ObjectManager();

    ObjectManager(const ObjectManager&) = delete;
    ObjectManager& operator=(const ObjectManager&) = delete;

    /// @brief Add a child object. Returns a ServiceObject to configure.
    std::shared_ptr<ServiceObject> addChild(const std::string& path);

    /// @brief Remove a child object. Emits InterfacesRemoved signal.
    void removeChild(const std::string& path);

    /// @brief Get the root path.
    const std::string& rootPath() const { return rootPath_; }

    /// @brief Get all managed objects (for GetManagedObjects).
    /// Returns map<path, map<iface, map<prop_name, variant>>>.
    std::map<ObjectPath, std::map<std::string, std::map<std::string, Variant>>>
    getManagedObjects() const;

private:
    ObjectManager(std::shared_ptr<Connection> conn, const std::string& rootPath);

    void emitInterfacesAdded(const std::string& path,
        const std::map<std::string, std::map<std::string, Variant>>& ifacesAndProps);
    void emitInterfacesRemoved(const std::string& path,
        const std::vector<std::string>& interfaces);

    void updateChildNodes();

    std::shared_ptr<Connection> conn_;
    std::string rootPath_;
    std::shared_ptr<ServiceObject> rootObject_;
    std::map<std::string, std::shared_ptr<ServiceObject>> children_;
    mutable std::mutex mutex_;
};

} // namespace mbedbus

#endif // MBEDBUS_OBJECTMANAGER_H
