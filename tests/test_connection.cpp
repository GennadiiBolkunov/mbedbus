#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>

// Helper: launch a private session bus for testing
class DBusTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Start a private dbus-daemon
        FILE* pipe = popen("dbus-daemon --session --fork --print-address=1 --print-pid=1", "r");
        if (!pipe) GTEST_SKIP() << "Cannot start dbus-daemon";

        char buf[512];
        // First line: address
        if (fgets(buf, sizeof(buf), pipe)) {
            address_ = buf;
            while (!address_.empty() && (address_.back() == '\n' || address_.back() == '\r'))
                address_.pop_back();
        }
        // Second line: pid
        if (fgets(buf, sizeof(buf), pipe)) {
            pid_ = std::atoi(buf);
        }
        pclose(pipe);

        if (address_.empty() || pid_ <= 0) {
            GTEST_SKIP() << "Failed to start dbus-daemon";
        }

        // Set the environment for libdbus
        setenv("DBUS_SESSION_BUS_ADDRESS", address_.c_str(), 1);
    }

    void TearDown() override {
        if (pid_ > 0) {
            kill(pid_, SIGTERM);
            pid_ = 0;
        }
    }

    std::string address_;
    pid_t pid_ = 0;
};

TEST_F(DBusTestFixture, ConnectSessionBus) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    ASSERT_TRUE(conn);
    EXPECT_FALSE(conn->uniqueName().empty());
}

TEST_F(DBusTestFixture, RequestName) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    ASSERT_NO_THROW(conn->requestName("com.mbedbus.Test"));
}

TEST_F(DBusTestFixture, RequestNameTwiceFails) {
    auto conn1 = mbedbus::Connection::createPrivate(address_);
    conn1->requestName("com.mbedbus.Unique");

    auto conn2 = mbedbus::Connection::createPrivate(address_);
    EXPECT_THROW(conn2->requestName("com.mbedbus.Unique"), mbedbus::Error);
}

TEST_F(DBusTestFixture, ProcessPendingEvents) {
    auto conn = mbedbus::Connection::createPrivate(address_);
    EXPECT_TRUE(conn->processPendingEvents(10));
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
