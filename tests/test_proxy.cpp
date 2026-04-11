#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

class ProxyTest : public ::testing::Test {
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
};

TEST_F(ProxyTest, CallMethod) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.ProxyTest");

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.ProxyTest")
        .addMethod("Concat", [](const std::string& a, const std::string& b) -> std::string {
            return a + b;
        });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.ProxyTest",
                                         "/test", "com.mbedbus.ProxyTest");
    auto r = proxy->call<std::string>("Concat", std::string("hello "), std::string("world"));
    EXPECT_EQ(r, "hello world");
    srv.join();
}

TEST_F(ProxyTest, TryCallError) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.ProxyErr");

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.ProxyErr")
        .addMethod("Fail", []() -> int32_t {
            throw mbedbus::Error("com.example.Fail", "intentional");
        });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.ProxyErr",
                                         "/test", "com.mbedbus.ProxyErr");
    auto result = proxy->tryCall<int32_t>("Fail");
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().name(), "com.example.Fail");
    srv.join();
}

TEST_F(ProxyTest, GetSetProperty) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.PropTest");

    int32_t volume = 50;
    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.PropTest")
        .addProperty("Volume",
            [&]() -> int32_t { return volume; },
            [&](int32_t v) { volume = v; });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 100; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.PropTest",
                                         "/test", "com.mbedbus.PropTest");
    auto v = proxy->getProperty<int32_t>("Volume");
    EXPECT_EQ(v, 50);

    proxy->setProperty("Volume", int32_t(75));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    v = proxy->getProperty<int32_t>("Volume");
    EXPECT_EQ(v, 75);
    EXPECT_EQ(volume, 75);

    srv.join();
}

TEST_F(ProxyTest, GetAllProperties) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.AllProps");

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.AllProps")
        .addProperty("Name", []() -> std::string { return "test"; })
        .addProperty("Count", []() -> int32_t { return 42; });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.AllProps",
                                         "/test", "com.mbedbus.AllProps");
    auto props = proxy->getAllProperties();
    EXPECT_EQ(props.size(), 2u);
    EXPECT_EQ(props["Name"].get<std::string>(), "test");
    EXPECT_EQ(props["Count"].get<int32_t>(), 42);

    srv.join();
}
