#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>

static const char* CALC_XML = R"(
<node name="/com/example/Calculator">
  <interface name="com.example.Calculator">
    <method name="Add">
      <arg name="a" type="i" direction="in"/>
      <arg name="b" type="i" direction="in"/>
      <arg name="sum" type="i" direction="out"/>
    </method>
    <method name="Concat">
      <arg name="a" type="s" direction="in"/>
      <arg name="b" type="s" direction="in"/>
      <arg name="result" type="s" direction="out"/>
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

// ============================================================
// Unit tests (no D-Bus daemon needed)
// ============================================================

TEST(Skeleton, ParsesXml) {
    // We need a connection just to create a Skeleton, but for unit tests
    // we can't create one. So unit tests are limited to testing parse errors.
    // Actual integration tests are below.
}

TEST(Skeleton, SignatureMismatchDetected) {
    // This test verifies that the template signature validation works
    // by checking the XML parsing part. The actual bindMethod call
    // requires a connection, so it's tested in integration below.
    auto node = mbedbus::IntrospectionParser::parse(CALC_XML);
    auto* m = node.findMethod("com.example.Calculator", "Add");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->inputSignature(), "ii");
    EXPECT_EQ(m->outputSignature(), "i");

    // If someone tried to bind with wrong types, the signatures wouldn't match:
    std::string wrongIn = mbedbus::types::buildSignature<std::string, std::string>();
    EXPECT_NE(wrongIn, m->inputSignature());
}

// ============================================================
// Integration tests (with D-Bus daemon)
// ============================================================

class SkeletonTest : public ::testing::Test {
protected:
    void SetUp() override {
        FILE* pipe = popen("dbus-daemon --session --fork --print-address=1 --print-pid=1", "r");
        if (!pipe) GTEST_SKIP();
        char buf[512];
        if (fgets(buf, sizeof(buf), pipe)) {
            address_ = buf;
            while (!address_.empty() && (address_.back() == '\n' || address_.back() == '\r'))
                address_.pop_back();
        }
        if (fgets(buf, sizeof(buf), pipe)) pid_ = std::atoi(buf);
        pclose(pipe);
        if (address_.empty() || pid_ <= 0) GTEST_SKIP();
        setenv("DBUS_SESSION_BUS_ADDRESS", address_.c_str(), 1);
    }
    void TearDown() override { if (pid_ > 0) kill(pid_, SIGTERM); }
    std::string address_;
    pid_t pid_ = 0;
    struct JoinGuard {
        std::thread& t;
        explicit JoinGuard(std::thread& t) : t(t) {}
        ~JoinGuard() { if (t.joinable()) t.join(); }
    };
};

TEST_F(SkeletonTest, FullRoundTrip) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Skel1");

    int32_t volume = 50;

    auto skel = mbedbus::Skeleton::fromXml(srv, CALC_XML);

    skel->bindMethod<int32_t, int32_t, int32_t>(
        "com.example.Calculator", "Add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    skel->bindMethod<std::string, std::string, std::string>(
        "com.example.Calculator", "Concat",
        [](const std::string& a, const std::string& b) -> std::string { return a + b; });

    skel->bindVoidMethod(
        "com.example.Calculator", "Reset",
        []() {});

    skel->bindPropertyGetter<std::string>(
        "com.example.Calculator", "Version",
        []() -> std::string { return "2.0"; });

    skel->bindPropertyGetter<int32_t>(
        "com.example.Calculator", "Volume",
        [&]() -> int32_t { return volume; });

    skel->bindPropertySetter<int32_t>(
        "com.example.Calculator", "Volume",
        [&](int32_t v) { volume = v; });

    skel->finalize();

    std::thread t([&] {
        for (int i = 0; i < 200; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.Skel1",
        "/com/example/Calculator",
        "com.example.Calculator");

    // Methods
    EXPECT_EQ(proxy->call<int32_t>("Add", int32_t(10), int32_t(20)), 30);
    EXPECT_EQ(proxy->call<std::string>("Concat", std::string("a"), std::string("b")), "ab");
    EXPECT_NO_THROW(proxy->callVoid("Reset"));

    // Properties
    EXPECT_EQ(proxy->getProperty<std::string>("Version"), "2.0");
    EXPECT_EQ(proxy->getProperty<int32_t>("Volume"), 50);
    proxy->setProperty("Volume", int32_t(75));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(proxy->getProperty<int32_t>("Volume"), 75);
}

TEST_F(SkeletonTest, SignatureMismatchThrows) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);

    // Add expects (int32_t, int32_t) -> int32_t, binding with wrong types should throw
    EXPECT_THROW(
        (skel->bindMethod<std::string, int32_t, int32_t>(
            "com.example.Calculator", "Add",
            [](int32_t, int32_t) -> std::string { return ""; })),
        mbedbus::Error);

    EXPECT_THROW(
        (skel->bindMethod<int32_t, std::string, std::string>(
            "com.example.Calculator", "Add",
            [](const std::string&, const std::string&) -> int32_t { return 0; })),
        mbedbus::Error);
}

TEST_F(SkeletonTest, UnknownMethodThrows) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);

    EXPECT_THROW(
        (skel->bindMethod<int32_t>("com.example.Calculator", "NonExistent",
            []() -> int32_t { return 0; })),
        mbedbus::Error);
}

TEST_F(SkeletonTest, UnknownInterfaceThrows) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);

    EXPECT_THROW(
        (skel->bindMethod<int32_t>("com.wrong.Interface", "Add",
            []() -> int32_t { return 0; })),
        mbedbus::Error);
}

TEST_F(SkeletonTest, PropertyTypeMismatchThrows) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);

    // Volume is type "i" (int32_t), binding as string should throw
    EXPECT_THROW(
        skel->bindPropertyGetter<std::string>(
            "com.example.Calculator", "Volume",
            []() -> std::string { return ""; }),
        mbedbus::Error);
}

TEST_F(SkeletonTest, StrictModeThrowsOnMissing) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);

    // Only bind one method, leave the rest
    skel->bindMethod<int32_t, int32_t, int32_t>(
        "com.example.Calculator", "Add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    EXPECT_THROW(skel->finalize(mbedbus::SkeletonMode::Strict), mbedbus::Error);
}

TEST_F(SkeletonTest, LenientModeSucceeds) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Skel2");

    auto skel = mbedbus::Skeleton::fromXml(srv, CALC_XML);

    // Only bind Add, leave everything else unbound
    skel->bindMethod<int32_t, int32_t, int32_t>(
        "com.example.Calculator", "Add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    EXPECT_NO_THROW(skel->finalize(mbedbus::SkeletonMode::Lenient));

    std::thread t([&] {
        for (int i = 0; i < 100; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.Skel2",
        "/com/example/Calculator",
        "com.example.Calculator");

    // Bound method works
    EXPECT_EQ(proxy->call<int32_t>("Add", int32_t(1), int32_t(2)), 3);

    // Unbound method returns NotSupported error
    auto result = proxy->tryCall<std::string>("Concat", std::string("a"), std::string("b"));
    EXPECT_FALSE(result.hasValue());

    // Unbound property getter returns error
    EXPECT_THROW(proxy->getProperty<std::string>("Version"), mbedbus::Error);
}

TEST_F(SkeletonTest, RawMethodBinding) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Skel3");

    auto skel = mbedbus::Skeleton::fromXml(srv, CALC_XML);

    skel->bindRawMethod("com.example.Calculator", "Add",
        [](const mbedbus::Message& call) -> mbedbus::Message {
            auto args = call.readArgs<int32_t, int32_t>();
            auto reply = mbedbus::Message::createMethodReturn(call);
            reply.appendArgs(std::get<0>(args) * std::get<1>(args)); // multiply instead of add
            return reply;
        });

    skel->finalize(mbedbus::SkeletonMode::Lenient);

    std::thread t([&] {
        for (int i = 0; i < 100; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.Skel3",
        "/com/example/Calculator",
        "com.example.Calculator");

    // Raw binding multiplies instead of adding
    EXPECT_EQ(proxy->call<int32_t>("Add", int32_t(3), int32_t(4)), 12);
}

TEST_F(SkeletonTest, DoubleFinalize) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);
    skel->finalize(mbedbus::SkeletonMode::Lenient);
    EXPECT_THROW(skel->finalize(), mbedbus::Error);
}

TEST_F(SkeletonTest, EmptyXmlThrows) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    EXPECT_THROW(mbedbus::Skeleton::fromXml(conn, "<node/>"), mbedbus::Error);
}

TEST_F(SkeletonTest, PathFromXml) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto skel = mbedbus::Skeleton::fromXml(conn, CALC_XML);
    EXPECT_EQ(skel->path(), "/com/example/Calculator");
}

TEST_F(SkeletonTest, IntrospectionWorks) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Skel4");

    auto skel = mbedbus::Skeleton::fromXml(srv, CALC_XML);
    skel->finalize(mbedbus::SkeletonMode::Lenient);

    std::thread t([&] {
        for (int i = 0; i < 50; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.Skel4",
        "/com/example/Calculator",
        "org.freedesktop.DBus.Introspectable");
    auto xml = proxy->call<std::string>("Introspect");
    EXPECT_NE(xml.find("com.example.Calculator"), std::string::npos);
    EXPECT_NE(xml.find("Add"), std::string::npos);
    EXPECT_NE(xml.find("Volume"), std::string::npos);
    EXPECT_NE(xml.find("Alert"), std::string::npos);
}