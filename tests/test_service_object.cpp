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
    struct JoinGuard { std::thread& t; explicit JoinGuard(std::thread& t) : t(t) {} ~JoinGuard() { if (t.joinable()) t.join(); } };
};

TEST_F(ServiceObjectTest, CreateAndFinalize) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc1");
    obj->addInterface("com.svc.Test").addMethod("M", [](){});
    ASSERT_NO_THROW(obj->finalize());
}

TEST_F(ServiceObjectTest, DoubleFinalize) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc2");
    obj->addInterface("com.svc.Test").addMethod("M", [](){});
    obj->finalize();
    EXPECT_NO_THROW(obj->finalize()); // idempotent
}

TEST_F(ServiceObjectTest, Path) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/my/path");
    EXPECT_EQ(obj->path(), "/my/path");
}

TEST_F(ServiceObjectTest, InterfacesAccess) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc3");
    obj->addInterface("com.A").addMethod("M", [](){});
    obj->addInterface("com.B").addProperty("P", []() -> int32_t { return 0; });
    auto& ifaces = obj->interfaces();
    EXPECT_EQ(ifaces.size(), 2u);
    EXPECT_NE(ifaces.find("com.A"), ifaces.end());
    EXPECT_NE(ifaces.find("com.B"), ifaces.end());
}

TEST_F(ServiceObjectTest, GetAllPropertiesEmpty) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc4");
    obj->addInterface("com.A").addMethod("M", [](){});
    auto props = obj->getAllProperties("com.A");
    EXPECT_TRUE(props.empty());
}

TEST_F(ServiceObjectTest, GetAllPropertiesMultiple) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc5");
    obj->addInterface("com.A")
        .addProperty("X", []() -> int32_t { return 1; })
        .addProperty("Y", []() -> std::string { return "two"; });
    auto props = obj->getAllProperties("com.A");
    EXPECT_EQ(props.size(), 2u);
    EXPECT_EQ(props["X"].get<int32_t>(), 1);
    EXPECT_EQ(props["Y"].get<std::string>(), "two");
}

TEST_F(ServiceObjectTest, GetAllPropertiesUnknownInterface) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc6");
    auto props = obj->getAllProperties("com.Unknown");
    EXPECT_TRUE(props.empty());
}

TEST_F(ServiceObjectTest, IntrospectionXML) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/svc7");
    obj->addInterface("com.test.Intro")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; })
        .addProperty("Version", []() -> std::string { return "1.0"; })
        .addSignal<int32_t>("Notify");
    obj->finalize();
    std::string xml = obj->introspect();
    EXPECT_NE(xml.find("com.test.Intro"), std::string::npos);
    EXPECT_NE(xml.find("Add"), std::string::npos);
    EXPECT_NE(xml.find("Version"), std::string::npos);
    EXPECT_NE(xml.find("Notify"), std::string::npos);
    EXPECT_NE(xml.find("org.freedesktop.DBus.Introspectable"), std::string::npos);
    EXPECT_NE(xml.find("org.freedesktop.DBus.Properties"), std::string::npos);
    EXPECT_NE(xml.find("org.freedesktop.DBus.Peer"), std::string::npos);
}

TEST_F(ServiceObjectTest, MethodCallRoundTrip) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.SvcRT");
    auto obj = mbedbus::ServiceObject::create(srv, "/calc");
    obj->addInterface("com.mbedbus.SvcRT")
        .addMethod("Add", [](int32_t a, int32_t b) -> int32_t { return a + b; });
    obj->finalize();
    std::thread srvThread([&] {
        for (int i = 0; i < 50; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(srvThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.SvcRT", "/calc", "com.mbedbus.SvcRT");
    EXPECT_EQ(proxy->call<int32_t>("Add", int32_t(3), int32_t(4)), 7);
    srvThread.join();
}

TEST_F(ServiceObjectTest, UnregisterBeforeDestroy) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/unreg");
    obj->addInterface("com.A").addMethod("M", [](){});
    obj->finalize();
    obj->unregister();
    // Destructor should not try to unregister again
}

TEST_F(ServiceObjectTest, SetChildNodes) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto obj = mbedbus::ServiceObject::create(conn, "/parent");
    obj->addInterface("com.A").addMethod("M", [](){});
    obj->setChildNodes({"child1", "child2"});
    obj->finalize();
    std::string xml = obj->introspect();
    EXPECT_NE(xml.find("child1"), std::string::npos);
    EXPECT_NE(xml.find("child2"), std::string::npos);
}
