#include <mbedbus/mbedbus.h>
#include <iostream>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    auto proxy = mbedbus::Proxy::create(conn, "com.example.Calculator",
                                         "/com/example/Calculator",
                                         "com.example.Calculator");

    auto sum = proxy->call<int32_t>("Add", int32_t(10), int32_t(20));
    std::cout << "10 + 20 = " << sum << std::endl;

    auto vol = proxy->getProperty<int32_t>("Volume");
    std::cout << "Volume = " << vol << std::endl;

    proxy->setProperty("Volume", int32_t(75));
    std::cout << "Volume set to 75" << std::endl;

    auto result = proxy->tryCall<double>("Divide", 1.0, 0.0);
    if (!result) {
        std::cout << "Error: " << result.error().message() << std::endl;
    }

    return 0;
}
