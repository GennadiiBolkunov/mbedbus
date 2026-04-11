#include <mbedbus/mbedbus.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    conn->requestName("com.example.Emitter");

    auto obj = mbedbus::ServiceObject::create(conn, "/com/example/Emitter");
    obj->addInterface("com.example.Emitter")
        .addSignal<int32_t, std::string>("Notify");
    obj->finalize();

    std::cout << "Emitting signals every second..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        obj->emitSignal("com.example.Emitter", "Notify",
                        int32_t(i), std::string("tick " + std::to_string(i)));
        std::cout << "Emitted signal #" << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        conn->processPendingEvents(0);
    }

    return 0;
}
