#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <chrono>
#include <csignal>

class DBusTestFixture : public ::testing::Test {
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

TEST_F(DBusTestFixture, ConnectPrivate) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    ASSERT_TRUE(conn);
    EXPECT_FALSE(conn->uniqueName().empty());
    EXPECT_NE(conn->raw(), nullptr);
}

TEST_F(DBusTestFixture, TwoPrivateConnections) {
    auto c1 = mbedbus::Connection::createPrivate(address_);
    auto c2 = mbedbus::Connection::createPrivate(address_);
    EXPECT_NE(c1->uniqueName(), c2->uniqueName());
}

TEST_F(DBusTestFixture, RequestName) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    ASSERT_NO_THROW(conn->requestName("com.mbedbus.ConnTest1"));
}

TEST_F(DBusTestFixture, RequestNameTwiceFails) {
    auto c1 = mbedbus::Connection::createPrivate(address_);
    c1->requestName("com.mbedbus.ConnTestUniq");
    auto c2 = mbedbus::Connection::createPrivate(address_);
    EXPECT_THROW(c2->requestName("com.mbedbus.ConnTestUniq"), mbedbus::Error);
}

TEST_F(DBusTestFixture, RequestAndReleaseName) {
    auto c1 = mbedbus::Connection::createPrivate(address_);
    c1->requestName("com.mbedbus.ConnRelease");
    c1->releaseName("com.mbedbus.ConnRelease");
    auto c2 = mbedbus::Connection::createPrivate(address_);
    EXPECT_NO_THROW(c2->requestName("com.mbedbus.ConnRelease"));
}

TEST_F(DBusTestFixture, ProcessPendingEvents) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    EXPECT_TRUE(conn->processPendingEvents(10));
}

TEST_F(DBusTestFixture, ProcessPendingEventsZeroTimeout) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    EXPECT_TRUE(conn->processPendingEvents(0));
}

TEST_F(DBusTestFixture, EventLoopStopFromThread) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        conn->requestStop();
    });
    conn->enterEventLoop();
    t.join();
}

TEST_F(DBusTestFixture, GetUnixFd) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    int fd = conn->getUnixFd();
    EXPECT_GE(fd, 0);
}

TEST_F(DBusTestFixture, Flush) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    EXPECT_NO_THROW(conn->flush());
}

TEST_F(DBusTestFixture, SendSignal) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    auto msg = mbedbus::Message::createSignal("/test", "com.test.If", "TestSig");
    msg.appendArgs(int32_t(42));
    uint32_t serial = conn->send(msg);
    EXPECT_GT(serial, 0u);
}

TEST_F(DBusTestFixture, UniqueNameFormat) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    std::string name = conn->uniqueName();
    EXPECT_EQ(name[0], ':');
}

TEST_F(DBusTestFixture, AddAndRemoveMatch) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    std::string rule = "type='signal',interface='com.test.If'";
    EXPECT_NO_THROW(conn->addMatch(rule));
    EXPECT_NO_THROW(conn->removeMatch(rule));
}
