#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>
#include <functional>

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
    void serve(std::shared_ptr<mbedbus::Connection>& c, int n = 100) {
        for (int i = 0; i < n; ++i) c->processPendingEvents(20);
    }
    struct JoinGuard { std::thread& t; explicit JoinGuard(std::thread& t) : t(t) {} ~JoinGuard() { if (t.joinable()) t.join(); } };
};

TEST_F(ProxyTest, CallStringMethod) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px1");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px1")
        .addMethod("Concat", [](const std::string& a, const std::string& b) -> std::string {
            return a + b;
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px1", "/t", "com.mbedbus.Px1");
    EXPECT_EQ(p->call<std::string>("Concat", std::string("a"), std::string("b")), "ab");
    EXPECT_EQ(p->call<std::string>("Concat", std::string(""), std::string("")), "");
    t.join();
}

TEST_F(ProxyTest, TryCallError) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px2");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px2")
        .addMethod("Fail", []() -> int32_t {
            throw mbedbus::Error("com.example.Fail", "intentional");
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px2", "/t", "com.mbedbus.Px2");
    auto result = p->tryCall<int32_t>("Fail");
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().name(), "com.example.Fail");
    t.join();
}

TEST_F(ProxyTest, GetSetProperty) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px3");
    int32_t volume = 50;
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px3")
        .addProperty("Volume",
            [&]() -> int32_t { return volume; },
            [&](int32_t v) { volume = v; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 200); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px3", "/t", "com.mbedbus.Px3");
    EXPECT_EQ(p->getProperty<int32_t>("Volume"), 50);
    p->setProperty("Volume", int32_t(75));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(p->getProperty<int32_t>("Volume"), 75);
    EXPECT_EQ(volume, 75);
    t.join();
}

TEST_F(ProxyTest, GetAllProperties) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px4");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px4")
        .addProperty("Name", []() -> std::string { return "test"; })
        .addProperty("Count", []() -> int32_t { return 42; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px4", "/t", "com.mbedbus.Px4");
    auto props = p->getAllProperties();
    EXPECT_EQ(props.size(), 2u);
    EXPECT_EQ(props["Name"].get<std::string>(), "test");
    EXPECT_EQ(props["Count"].get<int32_t>(), 42);
    t.join();
}

TEST_F(ProxyTest, SetTimeout) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px5");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px5")
        .addMethod("Fast", []() -> int32_t { return 1; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px5", "/t", "com.mbedbus.Px5");
    p->setTimeout(5000);
    EXPECT_EQ(p->call<int32_t>("Fast"), 1);
    t.join();
}

TEST_F(ProxyTest, CallVoidMethod) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px6");
    std::atomic<bool> called(false);
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px6")
        .addMethod("DoIt", [&]() { called = true; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px6", "/t", "com.mbedbus.Px6");
    EXPECT_NO_THROW(p->callVoid("DoIt"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(called.load());
    t.join();
}

TEST_F(ProxyTest, MultipleCalls) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px7");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px7")
        .addMethod("Double", [](int32_t v) -> int32_t { return v * 2; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 300); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px7", "/t", "com.mbedbus.Px7");
    for (int32_t i = 0; i < 50; ++i) {
        EXPECT_EQ(p->call<int32_t>("Double", i), i * 2);
    }
    t.join();
}

TEST_F(ProxyTest, AsyncCall) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Px8");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Px8")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 200); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Px8", "/t", "com.mbedbus.Px8");
    std::atomic<bool> got_result(false);
    int32_t async_result = 0;
    p->callAsync<int32_t>("Add",
        [&](mbedbus::Expected<int32_t> r) {
            if (r) async_result = r.value();
            got_result = true;
        },
        int32_t(10), int32_t(20));
    // Process events to receive reply
    for (int i = 0; i < 100 && !got_result; ++i) {
        cli->processPendingEvents(20);
    }
    EXPECT_TRUE(got_result.load());
    EXPECT_EQ(async_result, 30);
    t.join();
}
