/// @file skeleton_server.cpp
/// @brief Example: build a D-Bus service from XML contract using Skeleton.

#include <mbedbus/mbedbus.h>
#include <iostream>

static const char* CONTRACT_XML = R"(
<node name="/com/example/Calculator">
  <interface name="com.example.Calculator">
    <method name="Add">
      <arg name="a" type="i" direction="in"/>
      <arg name="b" type="i" direction="in"/>
      <arg name="sum" type="i" direction="out"/>
    </method>
    <method name="Divide">
      <arg name="a" type="d" direction="in"/>
      <arg name="b" type="d" direction="in"/>
      <arg name="result" type="d" direction="out"/>
    </method>
    <method name="Reset"/>
    <property name="Version" type="s" access="read"/>
    <property name="Volume" type="i" access="readwrite"/>
    <signal name="Alert">
      <arg name="code" type="i"/>
      <arg name="message" type="s"/>
    </signal>
  </interface>
</node>
)";

int main() {
    auto conn = mbedbus::Connection::createSessionBus();
    conn->requestName("com.example.Calculator");

    // ---- Create skeleton from XML ----
    auto skel = mbedbus::Skeleton::fromXml(conn, CONTRACT_XML);

    // ---- Developers bind handlers ----
    // Signatures are checked against XML at bind time.
    // If you change int32_t to string here, you get an immediate exception.

    skel->bindMethod<int32_t, int32_t, int32_t>(
        "com.example.Calculator", "Add",
        [](int32_t a, int32_t b) -> int32_t {
            return a + b;
        });

    skel->bindMethod<double, double, double>(
        "com.example.Calculator", "Divide",
        [](double a, double b) -> double {
            if (b == 0.0)
                throw mbedbus::Error("com.example.DivisionByZero", "Cannot divide by zero");
            return a / b;
        });

    skel->bindVoidMethod(
        "com.example.Calculator", "Reset",
        []() { std::cout << "Reset called" << std::endl; });

    skel->bindPropertyGetter<std::string>(
        "com.example.Calculator", "Version",
        []() -> std::string { return "2.0"; });

    int32_t volume = 50;

    skel->bindPropertyGetter<int32_t>(
        "com.example.Calculator", "Volume",
        [&]() -> int32_t { return volume; });

    skel->bindPropertySetter<int32_t>(
        "com.example.Calculator", "Volume",
        [&](int32_t v) {
            if (v < 0 || v > 100)
                throw mbedbus::Error("com.example.InvalidValue", "Volume must be 0..100");
            volume = v;
            std::cout << "Volume set to " << v << std::endl;
        });

    // ---- Finalize: register on bus ----
    // Strict mode throws if any method or readable property is unbound.
    skel->finalize(mbedbus::SkeletonMode::Strict);

    std::cout << "Calculator service running (from XML skeleton)..." << std::endl;
    conn->enterEventLoop();
    return 0;
}
