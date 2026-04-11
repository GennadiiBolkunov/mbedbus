# mbedbus

**Minimalist, lightweight C++14 D-Bus wrapper for embedded systems.**

mbedbus wraps libdbus-1 with modern C++ idioms: RAII resource management, type-safe serialization via templates, and a fluent builder API for exporting D-Bus services — all without Boost, GLib, or libsystemd.

## Features

- **C++14**, no Boost — targets GCC 4.9+ and Clang 3.5+
- **RAII everywhere** — `Connection`, `Message`, smart pointers
- **Compile-time type safety** — D-Bus signatures deduced from C++ types
- **Fluent builder API** — define methods, properties, signals with lambdas
- **ObjectManager** — automatic `InterfacesAdded`/`InterfacesRemoved` signals
- **Standard interfaces** — `Properties`, `Introspectable`, `Peer`, `ObjectManager`
- **Dual error handling** — exceptions (`Error`) or `Expected<T>`
- **Flexible event loop** — blocking, manual dispatch, or poll/epoll integration

## Quick Start

```cpp
#include <mbedbus/mbedbus.h>

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    conn->requestName("com.example.Calculator");

    auto obj = mbedbus::ServiceObject::create(conn, "/com/example/Calculator");
    obj->addInterface("com.example.Calculator")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; });
    obj->finalize();

    conn->enterEventLoop();
}
```

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Dependencies

- libdbus-1 (development headers)
- CMake 3.5+
- C++14 compiler

### Options

| Option | Default | Description |
|---|---|---|
| `MBEDBUS_BUILD_TESTS` | ON | Build unit and integration tests |
| `MBEDBUS_BUILD_EXAMPLES` | ON | Build example programs |
| `MBEDBUS_ENABLE_LOGGING` | OFF | Enable debug logging to stderr |

## License

MIT
