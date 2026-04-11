#include <mbedbus/mbedbus.h>
#include <iostream>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    auto proxy = mbedbus::Proxy::create(conn, "com.example.Calculator",
                                         "/com/example/Calculator",
                                         "com.example.Calculator");

    proxy->onPropertiesChanged(
        [](const std::string& iface,
           const std::map<std::string, mbedbus::Variant>& changed,
           const std::vector<std::string>& invalidated) {
            std::cout << "PropertiesChanged on " << iface << ":" << std::endl;
            for (auto& kv : changed) {
                std::cout << "  " << kv.first << " changed";
                if (kv.second.is<int32_t>()) {
                    std::cout << " = " << kv.second.get<int32_t>();
                } else if (kv.second.is<std::string>()) {
                    std::cout << " = " << kv.second.get<std::string>();
                }
                std::cout << std::endl;
            }
            for (auto& name : invalidated) {
                std::cout << "  " << name << " invalidated" << std::endl;
            }
        });

    proxy->onInterfacesAdded(
        [](const mbedbus::ObjectPath& path,
           const std::map<std::string, std::map<std::string, mbedbus::Variant>>& ifaces) {
            std::cout << "InterfacesAdded at " << path.str() << std::endl;
            for (auto& kv : ifaces) {
                std::cout << "  interface: " << kv.first << std::endl;
            }
        });

    proxy->onInterfacesRemoved(
        [](const mbedbus::ObjectPath& path,
           const std::vector<std::string>& ifaces) {
            std::cout << "InterfacesRemoved at " << path.str() << std::endl;
            for (auto& name : ifaces) {
                std::cout << "  interface: " << name << std::endl;
            }
        });

    std::cout << "Monitoring properties and object changes... (Ctrl+C to stop)" << std::endl;
    conn->enterEventLoop();
    return 0;
}
