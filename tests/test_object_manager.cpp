#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>

class ObjectManagerTest : public ::testing::Test {
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

TEST_F(ObjectManagerTest, CreateEmpty) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/devices");
    EXPECT_EQ(mgr->rootPath(), "/devices");
    auto managed = mgr->getManagedObjects();
    EXPECT_TRUE(managed.empty());
}

TEST_F(ObjectManagerTest, AddOneChild) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    auto dev = mgr->addChild("/d/1");
    dev->addInterface("com.Dev").addProperty("Name", []() -> std::string { return "A"; });
    dev->finalize();
    auto managed = mgr->getManagedObjects();
    ASSERT_EQ(managed.size(), 1u);
    auto it = managed.find(mbedbus::ObjectPath("/d/1"));
    ASSERT_NE(it, managed.end());
    EXPECT_EQ(it->second.count("com.Dev"), 1u);
}

TEST_F(ObjectManagerTest, AddMultipleChildren) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    for (int i = 0; i < 5; ++i) {
        std::string path = "/d/" + std::to_string(i);
        auto child = mgr->addChild(path);
        child->addInterface("com.Dev")
            .addProperty("Index", [i]() -> int32_t { return i; });
        child->finalize();
    }
    auto managed = mgr->getManagedObjects();
    EXPECT_EQ(managed.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        auto it = managed.find(mbedbus::ObjectPath("/d/" + std::to_string(i)));
        ASSERT_NE(it, managed.end());
    }
}

TEST_F(ObjectManagerTest, RemoveChild) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    auto dev1 = mgr->addChild("/d/1");
    dev1->addInterface("com.Dev").addProperty("N", []() -> std::string { return "1"; });
    dev1->finalize();
    auto dev2 = mgr->addChild("/d/2");
    dev2->addInterface("com.Dev").addProperty("N", []() -> std::string { return "2"; });
    dev2->finalize();
    EXPECT_EQ(mgr->getManagedObjects().size(), 2u);
    mgr->removeChild("/d/1");
    auto managed = mgr->getManagedObjects();
    EXPECT_EQ(managed.size(), 1u);
    EXPECT_NE(managed.find(mbedbus::ObjectPath("/d/2")), managed.end());
    EXPECT_EQ(managed.find(mbedbus::ObjectPath("/d/1")), managed.end());
}

TEST_F(ObjectManagerTest, RemoveNonExistent) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    EXPECT_NO_THROW(mgr->removeChild("/d/nonexistent"));
    EXPECT_TRUE(mgr->getManagedObjects().empty());
}

TEST_F(ObjectManagerTest, RemoveAllChildren) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    for (int i = 0; i < 3; ++i) {
        auto c = mgr->addChild("/d/" + std::to_string(i));
        c->addInterface("com.Dev").addMethod("M", [](){});
        c->finalize();
    }
    EXPECT_EQ(mgr->getManagedObjects().size(), 3u);
    mgr->removeChild("/d/0");
    mgr->removeChild("/d/1");
    mgr->removeChild("/d/2");
    EXPECT_TRUE(mgr->getManagedObjects().empty());
}

TEST_F(ObjectManagerTest, ChildWithMultipleInterfaces) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    auto dev = mgr->addChild("/d/1");
    dev->addInterface("com.A").addProperty("X", []() -> int32_t { return 1; });
    dev->addInterface("com.B").addProperty("Y", []() -> std::string { return "z"; });
    dev->finalize();
    auto managed = mgr->getManagedObjects();
    auto& ifaces = managed[mbedbus::ObjectPath("/d/1")];
    EXPECT_EQ(ifaces.size(), 2u);
    EXPECT_EQ(ifaces.count("com.A"), 1u);
    EXPECT_EQ(ifaces.count("com.B"), 1u);
}

TEST_F(ObjectManagerTest, PropertiesInManagedObjects) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    auto dev = mgr->addChild("/d/1");
    dev->addInterface("com.Dev")
        .addProperty("Name", []() -> std::string { return "Sensor"; })
        .addProperty("Value", []() -> int32_t { return 42; });
    dev->finalize();
    auto managed = mgr->getManagedObjects();
    auto& props = managed[mbedbus::ObjectPath("/d/1")]["com.Dev"];
    EXPECT_EQ(props["Name"].get<std::string>(), "Sensor");
    EXPECT_EQ(props["Value"].get<int32_t>(), 42);
}

TEST_F(ObjectManagerTest, RemoveDoesNotDoubleUnregister) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto mgr = mbedbus::ObjectManager::create(conn, "/d");
    auto dev = mgr->addChild("/d/1");
    dev->addInterface("com.Dev").addMethod("M", [](){});
    dev->finalize();
    mgr->removeChild("/d/1");
    // dev still exists as shared_ptr — destructor should not crash
}

TEST_F(ObjectManagerTest, GetManagedObjectsViaProxy) {
    auto srv = mbedbus::Connection::createPrivate(address_);
    srv->requestName("com.mbedbus.OmPx");
    auto mgr = mbedbus::ObjectManager::create(srv, "/devices");
    auto dev = mgr->addChild("/devices/s1");
    dev->addInterface("com.Sensor").addProperty("Val", []() -> int32_t { return 99; });
    dev->finalize();
    std::thread t([&] {
        for (int i = 0; i < 50; ++i) srv->processPendingEvents(20);
    });
    JoinGuard jg(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto cli = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cli, "com.mbedbus.OmPx", "/devices",
        "org.freedesktop.DBus.ObjectManager");
    auto managed = proxy->getManagedObjects();
    EXPECT_GE(managed.size(), 1u);
    t.join();
}
