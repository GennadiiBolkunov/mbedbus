#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <dbus/dbus.h>

using namespace mbedbus;
using namespace mbedbus::types;

// Helper: create a scratch message, serialize value, deserialize back
template<typename T>
T roundTrip(const T& value) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    Traits<T>::serialize(iter, value);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    T result = Traits<T>::deserialize(readIter);
    dbus_message_unref(msg);
    return result;
}

TEST(TypeTraits, Bool) {
    EXPECT_EQ(Traits<bool>::signature(), "b");
    EXPECT_EQ(roundTrip(true), true);
    EXPECT_EQ(roundTrip(false), false);
}

TEST(TypeTraits, Byte) {
    EXPECT_EQ(Traits<uint8_t>::signature(), "y");
    EXPECT_EQ(roundTrip(uint8_t(255)), 255);
}

TEST(TypeTraits, Int16) {
    EXPECT_EQ(Traits<int16_t>::signature(), "n");
    EXPECT_EQ(roundTrip(int16_t(-1234)), -1234);
}

TEST(TypeTraits, UInt16) {
    EXPECT_EQ(Traits<uint16_t>::signature(), "q");
    EXPECT_EQ(roundTrip(uint16_t(65000)), 65000);
}

TEST(TypeTraits, Int32) {
    EXPECT_EQ(Traits<int32_t>::signature(), "i");
    EXPECT_EQ(roundTrip(int32_t(-100000)), -100000);
}

TEST(TypeTraits, UInt32) {
    EXPECT_EQ(Traits<uint32_t>::signature(), "u");
    EXPECT_EQ(roundTrip(uint32_t(3000000000u)), 3000000000u);
}

TEST(TypeTraits, Int64) {
    EXPECT_EQ(Traits<int64_t>::signature(), "x");
    EXPECT_EQ(roundTrip(int64_t(-9000000000LL)), -9000000000LL);
}

TEST(TypeTraits, UInt64) {
    EXPECT_EQ(Traits<uint64_t>::signature(), "t");
    EXPECT_EQ(roundTrip(uint64_t(18000000000000000000ULL)), 18000000000000000000ULL);
}

TEST(TypeTraits, Double) {
    EXPECT_EQ(Traits<double>::signature(), "d");
    EXPECT_DOUBLE_EQ(roundTrip(3.14159), 3.14159);
}

TEST(TypeTraits, String) {
    EXPECT_EQ(Traits<std::string>::signature(), "s");
    EXPECT_EQ(roundTrip(std::string("hello world")), "hello world");
    EXPECT_EQ(roundTrip(std::string("")), "");
}

TEST(TypeTraits, ObjectPath) {
    EXPECT_EQ(Traits<mbedbus::ObjectPath>::signature(), "o");
    auto result = roundTrip(mbedbus::ObjectPath("/com/example"));
    EXPECT_EQ(result.str(), "/com/example");
}

TEST(TypeTraits, Signature) {
    EXPECT_EQ(Traits<mbedbus::Signature>::signature(), "g");
    auto result = roundTrip(mbedbus::Signature("a{sv}"));
    EXPECT_EQ(result.str(), "a{sv}");
}

TEST(TypeTraits, VectorInt) {
    EXPECT_EQ(Traits<std::vector<int32_t>>::signature(), "ai");
    std::vector<int32_t> v = {1, 2, 3, 4, 5};
    EXPECT_EQ(roundTrip(v), v);
}

TEST(TypeTraits, VectorString) {
    EXPECT_EQ(Traits<std::vector<std::string>>::signature(), "as");
    std::vector<std::string> v = {"a", "bb", "ccc"};
    EXPECT_EQ(roundTrip(v), v);
}

TEST(TypeTraits, EmptyVector) {
    std::vector<int32_t> v;
    EXPECT_EQ(roundTrip(v), v);
}

TEST(TypeTraits, MapStringInt) {
    EXPECT_EQ((Traits<std::map<std::string, int32_t>>::signature()), "a{si}");
    std::map<std::string, int32_t> m = {{"a", 1}, {"b", 2}};
    EXPECT_EQ(roundTrip(m), m);
}

TEST(TypeTraits, MapStringString) {
    std::map<std::string, std::string> m = {{"key", "val"}};
    EXPECT_EQ(roundTrip(m), m);
}

TEST(TypeTraits, Tuple) {
    using T = std::tuple<int32_t, std::string, double>;
    EXPECT_EQ(Traits<T>::signature(), "(isd)");
    T t(42, "hello", 3.14);
    T result = roundTrip(t);
    EXPECT_EQ(std::get<0>(result), 42);
    EXPECT_EQ(std::get<1>(result), "hello");
    EXPECT_DOUBLE_EQ(std::get<2>(result), 3.14);
}

TEST(TypeTraits, NestedVector) {
    std::vector<std::vector<int32_t>> v = {{1, 2}, {3, 4, 5}};
    EXPECT_EQ(roundTrip(v), v);
}

TEST(TypeTraits, NestedMap) {
    std::map<std::string, std::vector<int32_t>> m = {
        {"nums", {1, 2, 3}},
    };
    EXPECT_EQ(roundTrip(m), m);
}

TEST(TypeTraits, BuildSignature) {
    EXPECT_EQ((buildSignature<int32_t, int32_t, std::string>()), "iis");
    EXPECT_EQ((buildSignature<>()), "");
    EXPECT_EQ((buildSignature<double>()), "d");
    EXPECT_EQ((buildSignature<std::vector<std::string>, bool>()), "asb");
}

TEST(TypeTraits, MultipleArgs) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    serializeArgs(iter, int32_t(10), std::string("hello"), double(2.5));

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    auto t = deserializeArgs<int32_t, std::string, double>(readIter);
    EXPECT_EQ(std::get<0>(t), 10);
    EXPECT_EQ(std::get<1>(t), "hello");
    EXPECT_DOUBLE_EQ(std::get<2>(t), 2.5);

    dbus_message_unref(msg);
}
