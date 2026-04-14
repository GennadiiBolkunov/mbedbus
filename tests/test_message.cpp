#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>

using namespace mbedbus;

static const char* DEST = "com.test.Dest";
static const char* PATH = "/com/test/Path";
static const char* IFACE = "com.test.Iface";
static const char* METHOD = "TestMethod";

// ============================================================
// Factory methods
// ============================================================

TEST(Message, CreateMethodCall) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_METHOD_CALL);
    EXPECT_STREQ(msg.destination(), DEST);
    EXPECT_STREQ(msg.path(), PATH);
    EXPECT_STREQ(msg.interface(), IFACE);
    EXPECT_STREQ(msg.member(), METHOD);
    EXPECT_FALSE(msg.isError());
}

TEST(Message, CreateSignal) {
    auto msg = Message::createSignal(PATH, IFACE, "SigName");
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_SIGNAL);
    EXPECT_STREQ(msg.path(), PATH);
    EXPECT_STREQ(msg.interface(), IFACE);
    EXPECT_STREQ(msg.member(), "SigName");
}

TEST(Message, MethodReturn) {
    auto call = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    dbus_message_set_serial(call.raw(), 1);
    auto ret = Message::createMethodReturn(call);
    ASSERT_TRUE(ret);
    EXPECT_EQ(ret.type(), DBUS_MESSAGE_TYPE_METHOD_RETURN);
    EXPECT_FALSE(ret.isError());
}

TEST(Message, ErrorMessage) {
    auto call = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    dbus_message_set_serial(call.raw(), 2);
    auto err = Message::createError(call, "com.err.Test", "oops");
    ASSERT_TRUE(err);
    EXPECT_TRUE(err.isError());
    EXPECT_STREQ(err.errorName(), "com.err.Test");
    EXPECT_EQ(err.type(), DBUS_MESSAGE_TYPE_ERROR);
}

TEST(Message, NullMessage) {
    Message msg;
    EXPECT_FALSE(msg);
    EXPECT_EQ(msg.type(), DBUS_MESSAGE_TYPE_INVALID);
    EXPECT_EQ(msg.raw(), nullptr);
    EXPECT_EQ(msg.interface(), nullptr);
    EXPECT_EQ(msg.member(), nullptr);
    EXPECT_EQ(msg.path(), nullptr);
    EXPECT_EQ(msg.sender(), nullptr);
    EXPECT_EQ(msg.destination(), nullptr);
    EXPECT_EQ(msg.errorName(), nullptr);
    EXPECT_FALSE(msg.isError());
}

// ============================================================
// Append and read
// ============================================================

TEST(Message, AppendAndReadSingleInt) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(42));
    EXPECT_EQ(msg.readArg<int32_t>(), 42);
}

TEST(Message, AppendAndReadSingleString) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(std::string("hello"));
    EXPECT_EQ(msg.readArg<std::string>(), "hello");
}

TEST(Message, AppendAndReadTwoArgs) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(42), std::string("hello"));
    auto t = msg.readArgs<int32_t, std::string>();
    EXPECT_EQ(std::get<0>(t), 42);
    EXPECT_EQ(std::get<1>(t), "hello");
}

TEST(Message, AppendAndReadFiveArgs) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(true, uint8_t(7), int32_t(-1), std::string("x"), double(3.14));
    auto t = msg.readArgs<bool, uint8_t, int32_t, std::string, double>();
    EXPECT_EQ(std::get<0>(t), true);
    EXPECT_EQ(std::get<1>(t), 7);
    EXPECT_EQ(std::get<2>(t), -1);
    EXPECT_EQ(std::get<3>(t), "x");
    EXPECT_DOUBLE_EQ(std::get<4>(t), 3.14);
}

TEST(Message, AppendVector) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    std::vector<int32_t> v = {1, 2, 3};
    msg.appendArgs(v);
    EXPECT_EQ(msg.readArg<std::vector<int32_t>>(), v);
}

TEST(Message, AppendMap) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    std::map<std::string, int32_t> m = {{"a", 1}, {"b", 2}};
    msg.appendArgs(m);
    auto result = msg.readArg<std::map<std::string, int32_t>>();
    EXPECT_EQ(result, m);
}

TEST(Message, AppendTuple) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    auto t = std::make_tuple(int32_t(1), std::string("x"));
    msg.appendArgs(t);
    auto result = msg.readArg<std::tuple<int32_t, std::string>>();
    EXPECT_EQ(std::get<0>(result), 1);
    EXPECT_EQ(std::get<1>(result), "x");
}

TEST(Message, AppendVariant) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    Variant v(int32_t(42));
    msg.appendArgs(v);
    auto result = msg.readArg<Variant>();
    EXPECT_EQ(result.get<int32_t>(), 42);
}

TEST(Message, ReadNoArgsThrows) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    // No args appended
    EXPECT_THROW(msg.readArg<int32_t>(), Error);
}

TEST(Message, AppendEmptyStringAndRead) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(std::string());
    EXPECT_EQ(msg.readArg<std::string>(), "");
}

// ============================================================
// Body signature
// ============================================================

TEST(Message, BodySignatureEmpty) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    EXPECT_STREQ(msg.bodySignature(), "");
}

TEST(Message, BodySignatureInt) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(1));
    EXPECT_STREQ(msg.bodySignature(), "i");
}

TEST(Message, BodySignatureMultiple) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(1), std::string("x"), double(2.0));
    EXPECT_STREQ(msg.bodySignature(), "isd");
}

TEST(Message, BodySignatureVector) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(std::vector<int32_t>{1, 2});
    EXPECT_STREQ(msg.bodySignature(), "ai");
}

// ============================================================
// Move and copy semantics
// ============================================================

TEST(Message, MoveConstructor) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(7));
    Message msg2(std::move(msg));
    EXPECT_FALSE(msg);
    EXPECT_TRUE(msg2);
    EXPECT_EQ(msg2.readArg<int32_t>(), 7);
}

TEST(Message, MoveAssignment) {
    auto msg1 = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg1.appendArgs(int32_t(1));
    auto msg2 = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg2.appendArgs(int32_t(2));
    msg2 = std::move(msg1);
    EXPECT_FALSE(msg1);
    EXPECT_EQ(msg2.readArg<int32_t>(), 1);
}

TEST(Message, CopyConstructor) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(99));
    Message msg2(msg);
    EXPECT_TRUE(msg);
    EXPECT_TRUE(msg2);
    EXPECT_EQ(msg.readArg<int32_t>(), 99);
    EXPECT_EQ(msg2.readArg<int32_t>(), 99);
}

TEST(Message, CopyAssignment) {
    auto msg1 = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg1.appendArgs(int32_t(10));
    auto msg2 = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg2 = msg1;
    EXPECT_EQ(msg1.readArg<int32_t>(), 10);
    EXPECT_EQ(msg2.readArg<int32_t>(), 10);
}

TEST(Message, SelfMoveAssignment) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.appendArgs(int32_t(5));
    msg = std::move(msg);
    // Should still be valid (self-move)
}

TEST(Message, Release) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    DBusMessage* raw = msg.release();
    EXPECT_FALSE(msg);
    EXPECT_NE(raw, nullptr);
    dbus_message_unref(raw);
}

// ============================================================
// Method return with args
// ============================================================

TEST(Message, MethodReturnWithArgs) {
    auto call = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    dbus_message_set_serial(call.raw(), 10);
    auto ret = Message::createMethodReturn(call);
    ret.appendArgs(int32_t(42), std::string("result"));
    auto t = ret.readArgs<int32_t, std::string>();
    EXPECT_EQ(std::get<0>(t), 42);
    EXPECT_EQ(std::get<1>(t), "result");
}

// ============================================================
// SetNoReply
// ============================================================

TEST(Message, SetNoReply) {
    auto msg = Message::createMethodCall(DEST, PATH, IFACE, METHOD);
    msg.setNoReply(true);
    EXPECT_TRUE(dbus_message_get_no_reply(msg.raw()));
    msg.setNoReply(false);
    EXPECT_FALSE(dbus_message_get_no_reply(msg.raw()));
}
