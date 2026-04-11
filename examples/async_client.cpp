#include <mbedbus/mbedbus.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    auto proxy = mbedbus::Proxy::create(conn, "com.example.Calculator",
                                         "/com/example/Calculator",
                                         "com.example.Calculator");

    proxy->callAsync<int32_t>("Add",
        [](mbedbus::Expected<int32_t> result) {
            if (result) {
                std::cout << "Async result: " << result.value() << std::endl;
            } else {
                std::cerr << "Async error: " << result.error().message() << std::endl;
            }
        },
        int32_t(100), int32_t(200));

    // Process events to receive the reply
    for (int i = 0; i < 50; ++i) {
        conn->processPendingEvents(20);
    }

    return 0;
}
