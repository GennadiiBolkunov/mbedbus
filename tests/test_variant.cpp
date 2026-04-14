#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>

using namespace mbedbus;

// Helper: serialize variant into dbus, deserialize back
static Variant variantRoundTrip(const Variant& v) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    types::Traits<Variant>::serialize(iter, v);
    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    Variant result = types::Traits<Variant>::deserialize(readIter);
    dbus_message_unref(msg);
    return result;
}

// ============================================================
// Construction and emptiness
// ============================================================

TEST(Variant, DefaultEmpty) {
    Variant v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.signature(), "");
}

TEST(Variant, NonEmpty) {
    Variant v(int32_t(1));
    EXPECT_FALSE(v.empty());
}

// ============================================================
// All basic types: construction, is<>, get<>, tryGet<>, signature
// ============================================================

TEST(Variant, Bool) {
    Variant vt(true), vf(false);
    EXPECT_EQ(vt.signature(), "b");
    EXPECT_TRUE(vt.is<bool>());
    EXPECT_EQ(vt.get<bool>(), true);
    EXPECT_EQ(vf.get<bool>(), false);
}

TEST(Variant, Byte) {
    Variant v(uint8_t(255));
    EXPECT_EQ(v.signature(), "y");
    EXPECT_EQ(v.get<uint8_t>(), 255);
}

TEST(Variant, Int16) {
    Variant v(int16_t(-32768));
    EXPECT_EQ(v.signature(), "n");
    EXPECT_EQ(v.get<int16_t>(), -32768);
}

TEST(Variant, UInt16) {
    Variant v(uint16_t(65535));
    EXPECT_EQ(v.signature(), "q");
    EXPECT_EQ(v.get<uint16_t>(), 65535);
}

TEST(Variant, Int32) {
    Variant v(int32_t(-1));
    EXPECT_EQ(v.signature(), "i");
    EXPECT_EQ(v.get<int32_t>(), -1);
}

TEST(Variant, UInt32) {
    Variant v(uint32_t(0));
    EXPECT_EQ(v.signature(), "u");
    EXPECT_EQ(v.get<uint32_t>(), 0);
}

TEST(Variant, Int64) {
    Variant v(int64_t(INT64_MAX));
    EXPECT_EQ(v.signature(), "x");
    EXPECT_EQ(v.get<int64_t>(), INT64_MAX);
}

TEST(Variant, UInt64) {
    Variant v(uint64_t(UINT64_MAX));
    EXPECT_EQ(v.signature(), "t");
    EXPECT_EQ(v.get<uint64_t>(), UINT64_MAX);
}

TEST(Variant, Double) {
    Variant v(3.14);
    EXPECT_EQ(v.signature(), "d");
    EXPECT_DOUBLE_EQ(v.get<double>(), 3.14);
}

TEST(Variant, String) {
    Variant v(std::string("hello"));
    EXPECT_EQ(v.signature(), "s");
    EXPECT_EQ(v.get<std::string>(), "hello");
}

TEST(Variant, EmptyString) {
    Variant v{std::string{}};
    EXPECT_EQ(v.get<std::string>(), "");
}

TEST(Variant, ObjectPath) {
    ObjectPath p("/com/example/Obj");
    Variant v(p);
    EXPECT_EQ(v.signature(), "o");
    EXPECT_EQ(v.get<ObjectPath>().str(), "/com/example/Obj");
}

// ============================================================
// Error handling
// ============================================================

TEST(Variant, TypeMismatchThrows) {
    Variant v(int32_t(1));
    EXPECT_THROW(v.get<std::string>(), Error);
}

TEST(Variant, EmptyGetThrows) {
    Variant v;
    EXPECT_THROW(v.get<int32_t>(), Error);
}

TEST(Variant, TryGetSuccess) {
    Variant v(int32_t(10));
    auto* p = v.tryGet<int32_t>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 10);
}

TEST(Variant, TryGetWrongType) {
    Variant v(int32_t(10));
    EXPECT_EQ(v.tryGet<std::string>(), nullptr);
}

TEST(Variant, TryGetEmpty) {
    Variant v;
    EXPECT_EQ(v.tryGet<int32_t>(), nullptr);
}

TEST(Variant, IsChecks) {
    Variant v(int32_t(1));
    EXPECT_TRUE(v.is<int32_t>());
    EXPECT_FALSE(v.is<uint32_t>());
    EXPECT_FALSE(v.is<std::string>());
    EXPECT_FALSE(v.is<double>());
    EXPECT_FALSE(v.is<bool>());
}

// ============================================================
// Equality
// ============================================================

TEST(Variant, EqualSameType)    { EXPECT_EQ(Variant(int32_t(1)), Variant(int32_t(1))); }
TEST(Variant, NotEqualSameType) { EXPECT_NE(Variant(int32_t(1)), Variant(int32_t(2))); }
TEST(Variant, NotEqualDiffType) { EXPECT_NE(Variant(int32_t(1)), Variant(std::string("1"))); }
TEST(Variant, BothEmpty)        { EXPECT_EQ(Variant(), Variant()); }
TEST(Variant, EmptyVsNonEmpty)  { EXPECT_NE(Variant(), Variant(int32_t(0))); }
TEST(Variant, StringEquality)   { EXPECT_EQ(Variant(std::string("x")), Variant(std::string("x"))); }
TEST(Variant, BoolEquality)     { EXPECT_EQ(Variant(true), Variant(true)); }
TEST(Variant, BoolInequality)   { EXPECT_NE(Variant(true), Variant(false)); }

// ============================================================
// D-Bus round-trip: all basic types
// ============================================================

TEST(Variant, RoundTripBool)    { EXPECT_EQ(variantRoundTrip(Variant(true)).get<bool>(), true); }
TEST(Variant, RoundTripBoolF)   { EXPECT_EQ(variantRoundTrip(Variant(false)).get<bool>(), false); }
TEST(Variant, RoundTripByte)    { EXPECT_EQ(variantRoundTrip(Variant(uint8_t(42))).get<uint8_t>(), 42); }
TEST(Variant, RoundTripInt16)   { EXPECT_EQ(variantRoundTrip(Variant(int16_t(-100))).get<int16_t>(), -100); }
TEST(Variant, RoundTripUInt16)  { EXPECT_EQ(variantRoundTrip(Variant(uint16_t(1000))).get<uint16_t>(), 1000); }
TEST(Variant, RoundTripInt32)   { EXPECT_EQ(variantRoundTrip(Variant(int32_t(42))).get<int32_t>(), 42); }
TEST(Variant, RoundTripUInt32)  { EXPECT_EQ(variantRoundTrip(Variant(uint32_t(42))).get<uint32_t>(), 42); }
TEST(Variant, RoundTripInt64)   { EXPECT_EQ(variantRoundTrip(Variant(int64_t(-1))).get<int64_t>(), -1); }
TEST(Variant, RoundTripUInt64)  { EXPECT_EQ(variantRoundTrip(Variant(uint64_t(999))).get<uint64_t>(), 999); }
TEST(Variant, RoundTripDouble)  { EXPECT_DOUBLE_EQ(variantRoundTrip(Variant(2.718)).get<double>(), 2.718); }
TEST(Variant, RoundTripString)  { EXPECT_EQ(variantRoundTrip(Variant(std::string("test"))).get<std::string>(), "test"); }
TEST(Variant, RoundTripObjPath) {
    EXPECT_EQ(variantRoundTrip(Variant(ObjectPath("/a/b"))).get<ObjectPath>().str(), "/a/b");
}
TEST(Variant, RoundTripSignature) {
    EXPECT_EQ(variantRoundTrip(Variant(mbedbus::Signature("ai"))).get<mbedbus::Signature>().str(), "ai");
}

// ============================================================
// Array round-trips through variant wire format
// ============================================================

TEST(Variant, VectorInt32RoundTrip) {
    std::vector<int32_t> orig = {10, 20, 30, -1};
    Variant v(orig);
    EXPECT_EQ(v.signature(), "ai");
    auto result = variantRoundTrip(v).get<std::vector<int32_t>>();
    EXPECT_EQ(result, orig);
}

TEST(Variant, VectorStringRoundTrip) {
    std::vector<std::string> orig = {"hello", "world", ""};
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<std::string>>();
    EXPECT_EQ(result, orig);
}

TEST(Variant, VectorDoubleRoundTrip) {
    std::vector<double> orig = {1.5, 2.7, 3.14};
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<double>>();
    ASSERT_EQ(result.size(), orig.size());
    for (size_t i = 0; i < result.size(); ++i)
        EXPECT_DOUBLE_EQ(result[i], orig[i]);
}

TEST(Variant, VectorByteRoundTrip) {
    std::vector<uint8_t> orig = {0, 1, 255, 128};
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<uint8_t>>();
    EXPECT_EQ(result, orig);
}

TEST(Variant, EmptyVectorRoundTrip) {
    std::vector<int32_t> orig;
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<int32_t>>();
    EXPECT_TRUE(result.empty());
}

TEST(Variant, VectorObjectPathRoundTrip) {
    std::vector<ObjectPath> orig = {ObjectPath("/a"), ObjectPath("/b/c")};
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<ObjectPath>>();
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].str(), "/a");
    EXPECT_EQ(result[1].str(), "/b/c");
}

TEST(Variant, VectorUInt32RoundTrip) {
    std::vector<uint32_t> orig = {0, 1, UINT32_MAX};
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<uint32_t>>();
    EXPECT_EQ(result, orig);
}

TEST(Variant, LargeVector) {
    std::vector<int32_t> orig;
    for (int i = 0; i < 1000; ++i) orig.push_back(i);
    auto result = variantRoundTrip(Variant(orig)).get<std::vector<int32_t>>();
    EXPECT_EQ(result, orig);
}

// ============================================================
// Dict (map) round-trips
// ============================================================

TEST(Variant, MapStringVariantRoundTrip) {
    std::map<std::string, Variant> orig;
    orig["count"] = Variant(int32_t(42));
    orig["name"] = Variant(std::string("test"));
    Variant v(orig);
    EXPECT_EQ(v.signature(), "a{sv}");
    auto result = variantRoundTrip(v).get<std::map<std::string, Variant>>();
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result["count"].get<int32_t>(), 42);
    EXPECT_EQ(result["name"].get<std::string>(), "test");
}

TEST(Variant, EmptyMapRoundTrip) {
    std::map<std::string, Variant> orig;
    auto result = variantRoundTrip(Variant(orig)).get<std::map<std::string, Variant>>();
    EXPECT_TRUE(result.empty());
}

TEST(Variant, MapSignatureCorrect) {
    std::map<std::string, Variant> m;
    Variant v(m);
    EXPECT_EQ(v.signature(), "a{sv}");
}

// ============================================================
// Tuples inside Variant
// ============================================================

TEST(Variant, TupleHolding) {
    using T = std::tuple<int32_t, std::string>;
    T t(42, "hello");
    Variant v(t);
    EXPECT_EQ(v.signature(), "(is)");
    EXPECT_TRUE(v.is<T>());
    EXPECT_EQ(std::get<0>(v.get<T>()), 42);
    EXPECT_EQ(std::get<1>(v.get<T>()), "hello");
}

TEST(Variant, TupleTriple) {
    using T = std::tuple<int32_t, std::string, double>;
    T t(1, "abc", 2.5);
    Variant v(t);
    EXPECT_EQ(v.signature(), "(isd)");
    auto& r = v.get<T>();
    EXPECT_EQ(std::get<0>(r), 1);
    EXPECT_DOUBLE_EQ(std::get<2>(r), 2.5);
}

TEST(Variant, TupleSingle) {
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
    auto& r = v.get<Outer>();
    EXPECT_EQ(std::get<0>(std::get<0>(r)), 1);
    EXPECT_EQ(std::get<1>(r), "nested");
}

TEST(Variant, TupleWithVector) {
    using T = std::tuple<std::string, std::vector<int32_t>>;
    T t("nums", {10, 20, 30});
    Variant v(t);
    EXPECT_EQ(v.signature(), "(sai)");
    auto& r = v.get<T>();
    EXPECT_EQ(std::get<0>(r), "nums");
    EXPECT_EQ(std::get<1>(r), (std::vector<int32_t>{10, 20, 30}));
}

TEST(Variant, TupleMismatchThrows) {
    using T1 = std::tuple<int32_t, std::string>;
    using T2 = std::tuple<std::string, int32_t>;
    Variant v(T1(1, "x"));
    EXPECT_THROW(v.get<T2>(), Error);
}

// ============================================================
// Cyclic: construct all basic types in a loop
// ============================================================

TEST(Variant, CyclicInt32Values) {
    for (int32_t val : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
        Variant v(val);
        EXPECT_EQ(v.get<int32_t>(), val);
        auto rt = variantRoundTrip(v);
        EXPECT_EQ(rt.get<int32_t>(), val);
    }
}

TEST(Variant, CyclicStrings) {
    for (const char* s : {"", "a", "hello", "Привет"}) {
        Variant v{std::string(s)};
        EXPECT_EQ(v.get<std::string>(), s);
        auto rt = variantRoundTrip(v);
        EXPECT_EQ(rt.get<std::string>(), s);
    }
}

// ============================================================
// Copy semantics of Variant
// ============================================================

TEST(Variant, CopyConstruction) {
    Variant v1(int32_t(42));
    Variant v2(v1);
    EXPECT_EQ(v1.get<int32_t>(), 42);
    EXPECT_EQ(v2.get<int32_t>(), 42);
}

TEST(Variant, CopyAssignment) {
    Variant v1(int32_t(42));
    Variant v2(std::string("x"));
    v2 = v1;
    EXPECT_EQ(v2.get<int32_t>(), 42);
}

TEST(Variant, SerializeEmptyThrows) {
    Variant v;
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    EXPECT_THROW(types::Traits<Variant>::serialize(iter, v), Error);
    dbus_message_unref(msg);
}
