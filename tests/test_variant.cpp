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

// --- Array round-trips through D-Bus variant wire format ---

TEST(Variant, VectorInt32RoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<int32_t> original = {10, 20, 30, -1};
    Variant v(original);
    EXPECT_EQ(v.signature(), "ai");
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<int32_t>>();
    EXPECT_EQ(result, original);
    dbus_message_unref(msg);
}

TEST(Variant, VectorStringRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<std::string> original = {"hello", "world", ""};
    Variant v(original);
    EXPECT_EQ(v.signature(), "as");
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<std::string>>();
    EXPECT_EQ(result, original);
    dbus_message_unref(msg);
}

TEST(Variant, VectorDoubleRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<double> original = {1.5, 2.7, 3.14};
    Variant v(original);
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<double>>();
    ASSERT_EQ(result.size(), original.size());
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_DOUBLE_EQ(result[i], original[i]);
    }
    dbus_message_unref(msg);
}

TEST(Variant, VectorByteRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<uint8_t> original = {0, 1, 255, 128};
    Variant v(original);
    EXPECT_EQ(v.signature(), "ay");
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<uint8_t>>();
    EXPECT_EQ(result, original);
    dbus_message_unref(msg);
}

TEST(Variant, EmptyVectorRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<int32_t> original;
    Variant v(original);
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<int32_t>>();
    EXPECT_TRUE(result.empty());
    dbus_message_unref(msg);
}

TEST(Variant, MapStringVariantRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::map<std::string, Variant> original;
    original["count"] = Variant(int32_t(42));
    original["name"] = Variant(std::string("test"));
    Variant v(original);
    EXPECT_EQ(v.signature(), "a{sv}");
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::map<std::string, Variant>>();
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result["count"].get<int32_t>(), 42);
    EXPECT_EQ(result["name"].get<std::string>(), "test");
    dbus_message_unref(msg);
}

// --- Tuple tests (struct in D-Bus terms) ---

TEST(Variant, TupleHolding) {
    using T = std::tuple<int32_t, std::string>;
    T t(42, "hello");
    Variant v(t);
    EXPECT_EQ(v.signature(), "(is)");
    EXPECT_TRUE(v.is<T>());

    auto& result = v.get<T>();
    EXPECT_EQ(std::get<0>(result), 42);
    EXPECT_EQ(std::get<1>(result), "hello");
}

TEST(Variant, TupleTriple) {
    using T = std::tuple<int32_t, std::string, double>;
    T t(1, "abc", 2.5);
    Variant v(t);
    EXPECT_EQ(v.signature(), "(isd)");

    auto& result = v.get<T>();
    EXPECT_EQ(std::get<0>(result), 1);
    EXPECT_EQ(std::get<1>(result), "abc");
    EXPECT_DOUBLE_EQ(std::get<2>(result), 2.5);
}

TEST(Variant, TupleSingleElement) {
    using T = std::tuple<std::string>;
    T t("only");
    Variant v(t);
    EXPECT_EQ(v.signature(), "(s)");
    EXPECT_EQ(std::get<0>(v.get<T>()), "only");
}

TEST(Variant, TupleNested) {
    using Inner = std::tuple<int32_t, int32_t>;
    using Outer = std::tuple<Inner, std::string>;
    Outer t(Inner(1, 2), "nested");
    Variant v(t);
    EXPECT_EQ(v.signature(), "((ii)s)");
    auto& result = v.get<Outer>();
    EXPECT_EQ(std::get<0>(std::get<0>(result)), 1);
    EXPECT_EQ(std::get<1>(std::get<0>(result)), 2);
    EXPECT_EQ(std::get<1>(result), "nested");
}

TEST(Variant, TupleWithVector) {
    using T = std::tuple<std::string, std::vector<int32_t>>;
    T t("nums", {10, 20, 30});
    Variant v(t);
    EXPECT_EQ(v.signature(), "(sai)");

    auto& result = v.get<T>();
    EXPECT_EQ(std::get<0>(result), "nums");
    EXPECT_EQ(std::get<1>(result), (std::vector<int32_t>{10, 20, 30}));
}

TEST(Variant, ObjectPathInVariant) {
    ObjectPath p("/com/example/Obj");
    Variant v(p);
    EXPECT_EQ(v.signature(), "o");
    EXPECT_EQ(v.get<ObjectPath>().str(), "/com/example/Obj");
}

TEST(Variant, VectorObjectPathRoundTrip) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    std::vector<ObjectPath> original = {ObjectPath("/a"), ObjectPath("/b")};
    Variant v(original);
    EXPECT_EQ(v.signature(), "ao");
    types::Traits<Variant>::serialize(iter, v);

    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant deserialized = types::Traits<Variant>::deserialize(readIter);

    auto result = deserialized.get<std::vector<ObjectPath>>();
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].str(), "/a");
    EXPECT_EQ(result[1].str(), "/b");
    dbus_message_unref(msg);
}
