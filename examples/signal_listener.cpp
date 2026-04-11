#include <mbedbus/mbedbus.h>
#include <iostream>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    auto proxy = mbedbus::Proxy::create(conn, "com.example.Emitter",
                                         "/com/example/Emitter",
                                         "com.example.Emitter");

    proxy->onSignal<int32_t, std::string>("Notify",
        std::function<void(int32_t, std::string)>(
            [](int32_t code, const std::string& msg) {
                std::cout << "Signal received: code=" << code << " msg=" << msg << std::endl;
            }));

    std::cout << "Listening for signals... (Ctrl+C to stop)" << std::endl;
    conn->enterEventLoop();
    return 0;
}
