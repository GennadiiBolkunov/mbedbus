#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

class IntegrationTest : public ::testing::Test {
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

TEST_F(IntegrationTest, FullServerClientCycle) {
    // --- Server setup ---
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.Integration");

    int32_t volume = 50;

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.Integration")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; })
        .addMethod("Concat", [](const std::string& a, const std::string& b) -> std::string {
            return a + b;
        })
        .addMethod("Echo", [](bool v) -> bool { return v; })
        .addMethod("Sum", [](std::vector<int32_t> nums) -> int32_t {
            int32_t s = 0;
            for (auto n : nums) s += n;
            return s;
        })
        .addProperty("Version", []() -> std::string { return "1.0.0"; })
        .addProperty("Volume",
            [&]() -> int32_t { return volume; },
            [&](int32_t v) { volume = v; })
        .addSignal<int32_t, std::string>("Alert");
    obj->finalize();

    std::thread srvThread([&] {
        for (int i = 0; i < 200; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Client ---
    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.Integration",
                                         "/test", "com.mbedbus.Integration");

    // Method calls
    EXPECT_EQ(proxy->call<int32_t>("Add", int32_t(10), int32_t(20)), 30);
    EXPECT_EQ(proxy->call<std::string>("Concat", std::string("a"), std::string("b")), "ab");
    EXPECT_EQ(proxy->call<bool>("Echo", true), true);
    EXPECT_EQ(proxy->call<bool>("Echo", false), false);

    // Vector argument
    std::vector<int32_t> nums = {1, 2, 3, 4, 5};
    EXPECT_EQ(proxy->call<int32_t>("Sum", nums), 15);

    // Properties
    EXPECT_EQ(proxy->getProperty<std::string>("Version"), "1.0.0");
    EXPECT_EQ(proxy->getProperty<int32_t>("Volume"), 50);
    proxy->setProperty("Volume", int32_t(80));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(proxy->getProperty<int32_t>("Volume"), 80);

    // GetAll
    auto props = proxy->getAllProperties();
    EXPECT_GE(props.size(), 2u);

    // tryCall
    auto good = proxy->tryCall<int32_t>("Add", int32_t(1), int32_t(2));
    ASSERT_TRUE(good.hasValue());
    EXPECT_EQ(good.value(), 3);

    srvThread.join();
}

TEST_F(IntegrationTest, ErrorPropagation) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.ErrProp");

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.ErrProp")
        .addMethod("Divide", [](double a, double b) -> double {
            if (b == 0.0) throw mbedbus::Error("com.example.DivByZero", "Division by zero");
            return a / b;
        });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.ErrProp",
                                         "/test", "com.mbedbus.ErrProp");

    EXPECT_DOUBLE_EQ(proxy->call<double>("Divide", 10.0, 2.0), 5.0);

    auto result = proxy->tryCall<double>("Divide", 1.0, 0.0);
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().name(), "com.example.DivByZero");

    srv.join();
}

TEST_F(IntegrationTest, VoidMethod) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.Void");

    std::atomic<bool> called(false);
    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.Void")
        .addMethod("Ping", [&]() { called = true; });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.Void",
                                         "/test", "com.mbedbus.Void");
    proxy->callVoid("Ping");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(called.load());

    srv.join();
}

TEST_F(IntegrationTest, NestedContainers) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.Nested");

    auto obj = mbedbus::ServiceObject::create(srvConn, "/test");
    obj->addInterface("com.mbedbus.Nested")
        .addMethod("EchoMap",
            [](std::map<std::string, int32_t> m) -> std::map<std::string, int32_t> {
                return m;
            })
        .addMethod("EchoVecVec",
            [](std::vector<std::vector<int32_t>> v) -> std::vector<std::vector<int32_t>> {
                return v;
            });
    obj->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.Nested",
                                         "/test", "com.mbedbus.Nested");

    std::map<std::string, int32_t> testMap = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto resultMap = proxy->call<std::map<std::string, int32_t>>("EchoMap", testMap);
    EXPECT_EQ(resultMap, testMap);

    std::vector<std::vector<int32_t>> testVec = {{1, 2}, {3, 4, 5}};
    auto resultVec = proxy->call<std::vector<std::vector<int32_t>>>("EchoVecVec", testVec);
    EXPECT_EQ(resultVec, testVec);

    srv.join();
}
