#include <mbedbus/mbedbus.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    conn->requestName("com.example.DeviceManager");

    auto manager = mbedbus::ObjectManager::create(conn, "/com/example/devices");

    auto dev1 = manager->addChild("/com/example/devices/1");
    dev1->addInterface("com.example.Device")
        .addProperty("Name", []() -> std::string { return "Sensor 1"; })
        .addProperty("Type", []() -> std::string { return "temperature"; })
        .addMethod("Reset", []() { std::cout << "Device 1 reset" << std::endl; });
    dev1->finalize();

    auto dev2 = manager->addChild("/com/example/devices/2");
    dev2->addInterface("com.example.Device")
        .addProperty("Name", []() -> std::string { return "Sensor 2"; })
        .addProperty("Type", []() -> std::string { return "humidity"; });
    dev2->finalize();

    std::cout << "DeviceManager running with 2 devices..." << std::endl;
    std::cout << "Removing device 1 in 5 seconds..." << std::endl;

    std::thread remover([&] {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        manager->removeChild("/com/example/devices/1");
        std::cout << "Device 1 removed." << std::endl;
    });

    conn->enterEventLoop();
    remover.join();
    return 0;
}
