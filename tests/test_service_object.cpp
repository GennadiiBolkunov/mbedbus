#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>

class ServiceObjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        FILE* pipe = popen("dbus-daemon --session --fork --print-address=1 --print-pid=1", "r");
        if (!pipe) GTEST_SKIP() << "Cannot start dbus-daemon";
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
};

TEST_F(ServiceObjectTest, CreateAndFinalize) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    conn->requestName("com.mbedbus.SvcTest");

    auto obj = mbedbus::ServiceObject::create(conn, "/com/mbedbus/SvcTest");
    obj->addInterface("com.mbedbus.SvcTest")
        .addMethod("Echo", [](std::string s) -> std::string { return s; });
    ASSERT_NO_THROW(obj->finalize());
}

TEST_F(ServiceObjectTest, IntrospectionXML) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/com/mbedbus/Intro");
    obj->addInterface("com.mbedbus.Intro")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; })
        .addProperty("Version", []() -> std::string { return "1.0"; })
        .addSignal<int32_t>("Notify");
    obj->finalize();

    std::string xml = obj->introspect();
    EXPECT_NE(xml.find("com.mbedbus.Intro"), std::string::npos);
    EXPECT_NE(xml.find("Add"), std::string::npos);
    EXPECT_NE(xml.find("Version"), std::string::npos);
    EXPECT_NE(xml.find("Notify"), std::string::npos);
    EXPECT_NE(xml.find("org.freedesktop.DBus.Introspectable"), std::string::npos);
    EXPECT_NE(xml.find("org.freedesktop.DBus.Properties"), std::string::npos);
}

TEST_F(ServiceObjectTest, MethodCallRoundTrip) {
    auto serverConn = mbedbus::Connection::createPrivate(address_);
    serverConn->requestName("com.mbedbus.Calc");

    auto obj = mbedbus::ServiceObject::create(serverConn, "/calc");
    obj->addInterface("com.mbedbus.Calc")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; });
    obj->finalize();

    // Process events in a thread
    std::thread srvThread([&] {
        for (int i = 0; i < 50; ++i) {
            serverConn->processPendingEvents(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto clientConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(clientConn, "com.mbedbus.Calc",
                                         "/calc", "com.mbedbus.Calc");
    auto result = proxy->call<int32_t>("Add", int32_t(3), int32_t(4));
    EXPECT_EQ(result, 7);

    srvThread.join();
}
