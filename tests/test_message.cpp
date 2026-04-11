#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>

using namespace mbedbus;

TEST(Message, CreateMethodCall) {
    auto msg = Message::createMethodCall("com.example.Svc", "/obj", "com.example.If", "Method");
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_METHOD_CALL);
    EXPECT_STREQ(msg.destination(), "com.example.Svc");
    EXPECT_STREQ(msg.path(), "/obj");
    EXPECT_STREQ(msg.interface(), "com.example.If");
    EXPECT_STREQ(msg.member(), "Method");
}

TEST(Message, CreateSignal) {
    auto msg = Message::createSignal("/obj", "com.example.If", "Sig");
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_SIGNAL);
    EXPECT_STREQ(msg.member(), "Sig");
}

TEST(Message, AppendAndReadArgs) {
    auto msg = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    msg.appendArgs(int32_t(42), std::string("hello"));
    auto t = msg.readArgs<int32_t, std::string>();
    EXPECT_EQ(std::get<0>(t), 42);
    EXPECT_EQ(std::get<1>(t), "hello");
}

TEST(Message, ReadSingleArg) {
    auto msg = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    msg.appendArgs(std::string("test"));
    EXPECT_EQ(msg.readArg<std::string>(), "test");
}

TEST(Message, MethodReturn) {
    auto call = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    dbus_message_set_serial(call.raw(), 1); // Required for method return
    auto ret = Message::createMethodReturn(call);
    ASSERT_TRUE(ret);
    EXPECT_EQ(ret.type(), DBUS_MESSAGE_TYPE_METHOD_RETURN);
}

TEST(Message, ErrorMessage) {
    auto call = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    dbus_message_set_serial(call.raw(), 2);
    auto err = Message::createError(call, "com.err.Test", "oops");
    ASSERT_TRUE(err);
    EXPECT_TRUE(err.isError());
    EXPECT_STREQ(err.errorName(), "com.err.Test");
}

TEST(Message, MoveSemantics) {
    auto msg = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    msg.appendArgs(int32_t(7));
    Message msg2(std::move(msg));
    EXPECT_FALSE(msg);
    EXPECT_TRUE(msg2);
    EXPECT_EQ(msg2.readArg<int32_t>(), 7);
}

TEST(Message, CopySemantics) {
    auto msg = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    msg.appendArgs(int32_t(99));
    Message msg2(msg);
    EXPECT_TRUE(msg);
    EXPECT_TRUE(msg2);
    EXPECT_EQ(msg2.readArg<int32_t>(), 99);
}

TEST(Message, BodySignature) {
    auto msg = Message::createMethodCall("com.test.Dest", "/com/test/Path", "com.test.Iface", "Method");
    msg.appendArgs(int32_t(1), std::string("x"));
    EXPECT_STREQ(msg.bodySignature(), "is");
}

TEST(Message, NullMessage) {
    Message msg;
    EXPECT_FALSE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_INVALID);
}
