#include <algorithm>
#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <climits>

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

    // Helper: run server event loop for N iterations
    void serve(std::shared_ptr<mbedbus::Connection>& conn, int iterations = 100) {
        for (int i = 0; i < iterations; ++i)
            conn->processPendingEvents(20);
    }

    // RAII thread joiner — prevents std::terminate if test throws before join()
    struct JoinGuard {
        std::thread& t;
        explicit JoinGuard(std::thread& t) : t(t) {}
        ~JoinGuard() { if (t.joinable()) t.join(); }
    };
};

// ============================================================
// Basic method calls with all types
// ============================================================

TEST_F(IntegrationTest, MethodCallInt32) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T1");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T1")
        .addMethod("Echo", [](int32_t v) -> int32_t { return v; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T1", "/t", "com.mbedbus.T1");
    for (int32_t v : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
        EXPECT_EQ(p->call<int32_t>("Echo", v), v) << "Failed for " << v;
    }
    t.join();
}

TEST_F(IntegrationTest, MethodCallString) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T2");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T2")
        .addMethod("Echo", [](const std::string& s) -> std::string { return s; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T2", "/t", "com.mbedbus.T2");
    for (const char* s : {"", "hello", "Привет", "a b\tc\nd"}) {
        EXPECT_EQ(p->call<std::string>("Echo", std::string(s)), s);
    }
    t.join();
}

TEST_F(IntegrationTest, MethodCallBool) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T3");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T3")
        .addMethod("Not", [](bool v) -> bool { return !v; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T3", "/t", "com.mbedbus.T3");
    EXPECT_EQ(p->call<bool>("Not", true), false);
    EXPECT_EQ(p->call<bool>("Not", false), true);
    t.join();
}

TEST_F(IntegrationTest, MethodCallDouble) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T4");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T4")
        .addMethod("Half", [](double v) -> double { return v / 2.0; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T4", "/t", "com.mbedbus.T4");
    EXPECT_DOUBLE_EQ(p->call<double>("Half", 10.0), 5.0);
    EXPECT_DOUBLE_EQ(p->call<double>("Half", 0.0), 0.0);
    EXPECT_DOUBLE_EQ(p->call<double>("Half", -6.0), -3.0);
    t.join();
}

TEST_F(IntegrationTest, MethodCallVoid) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T5");
    std::atomic<int> counter(0);
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T5")
        .addMethod("Increment", [&]() { ++counter; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T5", "/t", "com.mbedbus.T5");
    p->callVoid("Increment");
    p->callVoid("Increment");
    p->callVoid("Increment");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(counter.load(), 3);
    t.join();
}

// ============================================================
// Container types through D-Bus
// ============================================================

TEST_F(IntegrationTest, MethodCallVector) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T6");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T6")
        .addMethod("Sum", [](std::vector<int32_t> v) -> int32_t {
            int32_t s = 0; for (auto n : v) s += n; return s;
        })
        .addMethod("Reverse", [](std::vector<std::string> v) -> std::vector<std::string> {
            std::reverse(v.begin(), v.end()); return v;
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T6", "/t", "com.mbedbus.T6");
    EXPECT_EQ(p->call<int32_t>("Sum", std::vector<int32_t>{1, 2, 3, 4, 5}), 15);
    EXPECT_EQ(p->call<int32_t>("Sum", std::vector<int32_t>{}), 0);
    auto rev = p->call<std::vector<std::string>>("Reverse",
        std::vector<std::string>{"a", "b", "c"});
    EXPECT_EQ(rev, (std::vector<std::string>{"c", "b", "a"}));
    t.join();
}

TEST_F(IntegrationTest, MethodCallMap) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T7");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T7")
        .addMethod("Echo", [](std::map<std::string, int32_t> m) -> std::map<std::string, int32_t> {
            return m;
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T7", "/t", "com.mbedbus.T7");
    std::map<std::string, int32_t> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_EQ((p->call<std::map<std::string, int32_t>>("Echo", m)), m);
    // Empty map
    std::map<std::string, int32_t> empty;
    EXPECT_EQ((p->call<std::map<std::string, int32_t>>("Echo", empty)), empty);
    t.join();
}

TEST_F(IntegrationTest, NestedContainers) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.T8");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.T8")
        .addMethod("EchoVecVec", [](std::vector<std::vector<int32_t>> v)
            -> std::vector<std::vector<int32_t>> { return v; })
        .addMethod("EchoMapVec", [](std::map<std::string, std::vector<int32_t>> m)
            -> std::map<std::string, std::vector<int32_t>> { return m; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.T8", "/t", "com.mbedbus.T8");
    std::vector<std::vector<int32_t>> vv = {{1, 2}, {}, {3, 4, 5}};
    EXPECT_EQ((p->call<std::vector<std::vector<int32_t>>>("EchoVecVec", vv)), vv);
    std::map<std::string, std::vector<int32_t>> mv = {{"primes", {2, 3, 5}}, {"empty", {}}};
    EXPECT_EQ((p->call<std::map<std::string, std::vector<int32_t>>>("EchoMapVec", mv)), mv);
    t.join();
}

// ============================================================
// Error propagation
// ============================================================

TEST_F(IntegrationTest, ErrorPropagation) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.E1");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.E1")
        .addMethod("Fail", [](int32_t code) -> int32_t {
            throw mbedbus::Error("com.example.Err" + std::to_string(code),
                "Error code " + std::to_string(code));
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.E1", "/t", "com.mbedbus.E1");
    for (int code : {1, 2, 3}) {
        auto result = p->tryCall<int32_t>("Fail", int32_t(code));
        EXPECT_FALSE(result.hasValue());
        EXPECT_EQ(result.error().name(), "com.example.Err" + std::to_string(code));
    }
    t.join();
}

TEST_F(IntegrationTest, StdExceptionConverted) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.E2");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.E2")
        .addMethod("Throw", []() -> int32_t {
            throw std::runtime_error("std error");
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.E2", "/t", "com.mbedbus.E2");
    auto result = p->tryCall<int32_t>("Throw");
    EXPECT_FALSE(result.hasValue());
}

TEST_F(IntegrationTest, UnknownMethodError) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.E3");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.E3")
        .addMethod("Exists", []() -> int32_t { return 1; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.E3", "/t", "com.mbedbus.E3");
    EXPECT_EQ(p->call<int32_t>("Exists"), 1);
    EXPECT_THROW(p->call<int32_t>("DoesNotExist"), mbedbus::Error);
    t.join();
}

// ============================================================
// Properties — all types, errors
// ============================================================

TEST_F(IntegrationTest, PropertyReadOnly) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P1");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P1")
        .addProperty("Name", []() -> std::string { return "test"; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P1", "/t", "com.mbedbus.P1");
    EXPECT_EQ(p->getProperty<std::string>("Name"), "test");
    // Attempting to set a read-only property should fail
    EXPECT_THROW(p->setProperty("Name", std::string("new")), mbedbus::Error);
    t.join();
}

TEST_F(IntegrationTest, PropertyReadWrite) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P2");
    int32_t val = 0;
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P2")
        .addProperty("Value",
            [&]() -> int32_t { return val; },
            [&](int32_t v) { val = v; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 200); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P2", "/t", "com.mbedbus.P2");
    EXPECT_EQ(p->getProperty<int32_t>("Value"), 0);
    p->setProperty("Value", int32_t(42));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(p->getProperty<int32_t>("Value"), 42);
    // Set multiple times
    for (int32_t v = 0; v < 5; ++v) {
        p->setProperty("Value", v);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(p->getProperty<int32_t>("Value"), v);
    }
    t.join();
}

TEST_F(IntegrationTest, PropertyGetterError) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P3");
    bool ready = false;
    int32_t val = 0;
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P3")
        .addProperty("Value",
            [&]() -> int32_t {
                if (!ready) throw mbedbus::Error("com.err.NotReady", "Not ready");
                return val;
            },
            [&](int32_t v) { val = v; ready = true; })
        .addProperty("OK", []() -> std::string { return "ok"; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 200); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P3", "/t", "com.mbedbus.P3");
    // Get on unready property → error
    EXPECT_THROW(p->getProperty<int32_t>("Value"), mbedbus::Error);
    try {
        p->getProperty<int32_t>("Value");
    } catch (const mbedbus::Error& e) {
        EXPECT_EQ(e.name(), "com.err.NotReady");
    }
    // GetAll skips erroring property
    auto all = p->getAllProperties();
    EXPECT_EQ(all.count("OK"), 1u);
    EXPECT_EQ(all.count("Value"), 0u);
    // Set makes it ready
    p->setProperty("Value", int32_t(99));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(p->getProperty<int32_t>("Value"), 99);
    all = p->getAllProperties();
    EXPECT_EQ(all.size(), 2u);
    t.join();
}

TEST_F(IntegrationTest, PropertySetterError) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P4");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P4")
        .addProperty("Positive",
            []() -> int32_t { return 1; },
            [](int32_t v) {
                if (v <= 0) throw mbedbus::Error("com.err.Invalid", "Must be positive");
            });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P4", "/t", "com.mbedbus.P4");
    EXPECT_NO_THROW(p->setProperty("Positive", int32_t(5)));
    EXPECT_THROW(p->setProperty("Positive", int32_t(-1)), mbedbus::Error);
    try {
        p->setProperty("Positive", int32_t(0));
    } catch (const mbedbus::Error& e) {
        EXPECT_EQ(e.name(), "com.err.Invalid");
    }
    t.join();
}

TEST_F(IntegrationTest, PropertyUnknown) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P5");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P5")
        .addProperty("X", []() -> int32_t { return 1; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P5", "/t", "com.mbedbus.P5");
    EXPECT_EQ(p->getProperty<int32_t>("X"), 1);
    EXPECT_THROW(p->getProperty<int32_t>("NonExistent"), mbedbus::Error);
    t.join();
}

TEST_F(IntegrationTest, GetAllProperties) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.P6");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.P6")
        .addProperty("A", []() -> int32_t { return 1; })
        .addProperty("B", []() -> std::string { return "two"; })
        .addProperty("C", []() -> bool { return true; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.P6", "/t", "com.mbedbus.P6");
    auto all = p->getAllProperties();
    EXPECT_EQ(all.size(), 3u);
    EXPECT_EQ(all["A"].get<int32_t>(), 1);
    EXPECT_EQ(all["B"].get<std::string>(), "two");
    EXPECT_EQ(all["C"].get<bool>(), true);
    t.join();
}

// ============================================================
// Multiple interfaces on one object
// ============================================================

TEST_F(IntegrationTest, MultipleInterfaces) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.MI");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Math")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; });
    obj->addInterface("com.mbedbus.Text")
        .addMethod("Len", [](const std::string& s) -> int32_t { return (int32_t)s.size(); });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto pm = mbedbus::Proxy::create(cli, "com.mbedbus.MI", "/t", "com.mbedbus.Math");
    auto pt = mbedbus::Proxy::create(cli, "com.mbedbus.MI", "/t", "com.mbedbus.Text");
    EXPECT_EQ(pm->call<int32_t>("Add", int32_t(3), int32_t(4)), 7);
    EXPECT_EQ(pt->call<int32_t>("Len", std::string("hello")), 5);
    t.join();
}

// ============================================================
// tryCall
// ============================================================

TEST_F(IntegrationTest, TryCallSuccess) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.TC1");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.TC1")
        .addMethod("Inc", [](int32_t v) -> int32_t { return v + 1; });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.TC1", "/t", "com.mbedbus.TC1");
    auto r = p->tryCall<int32_t>("Inc", int32_t(9));
    ASSERT_TRUE(r.hasValue());
    EXPECT_EQ(r.value(), 10);
    t.join();
}

TEST_F(IntegrationTest, TryCallFailure) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.TC2");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.TC2")
        .addMethod("Fail", []() -> int32_t {
            throw mbedbus::Error("com.err.X", "fail");
        });
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.TC2", "/t", "com.mbedbus.TC2");
    auto r = p->tryCall<int32_t>("Fail");
    ASSERT_FALSE(r.hasValue());
    EXPECT_EQ(r.error().name(), "com.err.X");
    t.join();
}

// ============================================================
// Stress: many sequential calls
// ============================================================

TEST_F(IntegrationTest, ManySequentialCalls) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Stress");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Stress")
        .addMethod("Inc", [](int32_t v) -> int32_t { return v + 1; });
    obj->finalize();
    std::thread t([&]{ serve(srv, 500); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Stress", "/t", "com.mbedbus.Stress");
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(p->call<int32_t>("Inc", int32_t(i)), i + 1);
    }
    t.join();
}

// ============================================================
// ServiceObject::unregister idempotency
// ============================================================

TEST_F(IntegrationTest, UnregisterIdempotent) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/unreg");
    obj->addInterface("com.test.U").addMethod("Ping", [](){});
    obj->finalize();
    obj->unregister();
    obj->unregister(); // second call should be no-op
    // Destructor will also call unregister — should not crash
}

// ============================================================
// Peer interface
// ============================================================

TEST_F(IntegrationTest, PeerPing) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.Peer");
    auto obj = mbedbus::ServiceObject::create(srv, "/t");
    obj->addInterface("com.mbedbus.Peer").addMethod("Noop", [](){});
    obj->finalize();
    std::thread t([&]{ serve(srv); });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto p = mbedbus::Proxy::create(cli, "com.mbedbus.Peer", "/t",
        "org.freedesktop.DBus.Peer");
    EXPECT_NO_THROW(p->callVoid("Ping"));
    t.join();
}
