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
};

TEST_F(ObjectManagerTest, AddAndRemoveChildren) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    conn->requestName("com.mbedbus.OmTest");

    auto mgr = mbedbus::ObjectManager::create(conn, "/com/mbedbus/devices");

    auto dev1 = mgr->addChild("/com/mbedbus/devices/1");
    dev1->addInterface("com.mbedbus.Device")
        .addProperty("Name", []() -> std::string { return "Sensor 1"; });
    dev1->finalize();

    auto dev2 = mgr->addChild("/com/mbedbus/devices/2");
    dev2->addInterface("com.mbedbus.Device")
        .addProperty("Name", []() -> std::string { return "Sensor 2"; });
    dev2->finalize();

    auto managed = mgr->getManagedObjects();
    EXPECT_EQ(managed.size(), 2u);

    mgr->removeChild("/com/mbedbus/devices/1");
    managed = mgr->getManagedObjects();
    EXPECT_EQ(managed.size(), 1u);

    auto it = managed.find(mbedbus::ObjectPath("/com/mbedbus/devices/2"));
    ASSERT_NE(it, managed.end());
}

TEST_F(ObjectManagerTest, GetManagedObjectsViaProxy) {
    auto srvConn = mbedbus::Connection::createPrivate(address_);
    srvConn->requestName("com.mbedbus.OmProxy");

    auto mgr = mbedbus::ObjectManager::create(srvConn, "/devices");
    auto dev = mgr->addChild("/devices/sensor");
    dev->addInterface("com.mbedbus.Sensor")
        .addProperty("Value", []() -> int32_t { return 42; });
    dev->finalize();

    std::thread srv([&] {
        for (int i = 0; i < 50; ++i) srvConn->processPendingEvents(20);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto cliConn = mbedbus::Connection::createPrivate(address_);
    auto proxy = mbedbus::Proxy::create(cliConn, "com.mbedbus.OmProxy",
                                         "/devices",
                                         "org.freedesktop.DBus.ObjectManager");
    auto managed = proxy->getManagedObjects();
    EXPECT_GE(managed.size(), 1u);

    srv.join();
}
