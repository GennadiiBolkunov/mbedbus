#include <mbedbus/mbedbus.h>
#include <iostream>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    conn->requestName("com.example.Calculator");

    int32_t volume = 50;

    auto obj = mbedbus::ServiceObject::create(conn, "/com/example/Calculator");
    obj->addInterface("com.example.Calculator")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; })
        .addMethod("Divide", [](double a, double b) -> double {
            if (b == 0.0) throw mbedbus::Error("com.example.DivisionByZero", "Cannot divide by zero");
            return a / b;
        })
        .addProperty("Volume",
            [&]() -> int32_t { return volume; },
            [&](int32_t v) { volume = v; })
        .addSignal<int32_t, std::string>("Alert");

    obj->finalize();

    std::cout << "Calculator service running..." << std::endl;
    conn->enterEventLoop();
    return 0;
}
