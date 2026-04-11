#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>

using namespace mbedbus;

TEST(Variant, Empty) {
    Variant v;
    EXPECT_TRUE(v.empty());
}

TEST(Variant, Int32) {
    Variant v(int32_t(42));
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(v.signature(), "i");
    EXPECT_TRUE(v.is<int32_t>());
    EXPECT_EQ(v.get<int32_t>(), 42);
}

TEST(Variant, String) {
    Variant v(std::string("hello"));
    EXPECT_EQ(v.signature(), "s");
    EXPECT_EQ(v.get<std::string>(), "hello");
}

TEST(Variant, Double) {
    Variant v(3.14);
    EXPECT_EQ(v.signature(), "d");
    EXPECT_DOUBLE_EQ(v.get<double>(), 3.14);
}

TEST(Variant, Bool) {
    Variant v(true);
    EXPECT_EQ(v.signature(), "b");
    EXPECT_EQ(v.get<bool>(), true);
}

TEST(Variant, TypeMismatchThrows) {
    Variant v(int32_t(1));
    EXPECT_THROW(v.get<std::string>(), Error);
}

TEST(Variant, EmptyGetThrows) {
    Variant v;
    EXPECT_THROW(v.get<int32_t>(), Error);
}

TEST(Variant, TryGet) {
    Variant v(int32_t(10));
    EXPECT_NE(v.tryGet<int32_t>(), nullptr);
    EXPECT_EQ(*v.tryGet<int32_t>(), 10);
    EXPECT_EQ(v.tryGet<std::string>(), nullptr);
}

TEST(Variant, Equality) {
    EXPECT_EQ(Variant(int32_t(1)), Variant(int32_t(1)));
    EXPECT_NE(Variant(int32_t(1)), Variant(int32_t(2)));
    EXPECT_NE(Variant(int32_t(1)), Variant(std::string("1")));
    Variant a, b;
    EXPECT_EQ(a, b);
}

TEST(Variant, RoundTripViaDbus) {
    // Serialize as variant, deserialize back
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    Variant original(int32_t(42));
    types::Traits<Variant>::serialize(iter, original);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    EXPECT_EQ(deserialized.get<int32_t>(), 42);
    dbus_message_unref(msg);
}

TEST(Variant, RoundTripString) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    Variant original(std::string("test"));
    types::Traits<Variant>::serialize(iter, original);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    EXPECT_EQ(deserialized.get<std::string>(), "test");
    dbus_message_unref(msg);
}

TEST(Variant, MapRoundTrip) {
    std::map<std::string, Variant> m;
    m["key1"] = Variant(int32_t(1));
    m["key2"] = Variant(std::string("val"));
    Variant v(m);
    EXPECT_EQ(v.signature(), "a{sv}");
}
