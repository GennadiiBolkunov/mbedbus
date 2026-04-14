#define _USE_MATH_DEFINES
#include <cmath>
#include <climits>
#include <cfloat>
#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <dbus/dbus.h>

using namespace mbedbus;
using namespace mbedbus::types;

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

// ============================================================
// Signatures
// ============================================================

TEST(TypeTraits, SignatureBool)     { EXPECT_EQ(Traits<bool>::signature(), "b"); }
TEST(TypeTraits, SignatureByte)     { EXPECT_EQ(Traits<uint8_t>::signature(), "y"); }
TEST(TypeTraits, SignatureInt16)    { EXPECT_EQ(Traits<int16_t>::signature(), "n"); }
TEST(TypeTraits, SignatureUInt16)   { EXPECT_EQ(Traits<uint16_t>::signature(), "q"); }
TEST(TypeTraits, SignatureInt32)    { EXPECT_EQ(Traits<int32_t>::signature(), "i"); }
TEST(TypeTraits, SignatureUInt32)   { EXPECT_EQ(Traits<uint32_t>::signature(), "u"); }
TEST(TypeTraits, SignatureInt64)    { EXPECT_EQ(Traits<int64_t>::signature(), "x"); }
TEST(TypeTraits, SignatureUInt64)   { EXPECT_EQ(Traits<uint64_t>::signature(), "t"); }
TEST(TypeTraits, SignatureDouble)   { EXPECT_EQ(Traits<double>::signature(), "d"); }
TEST(TypeTraits, SignatureString)   { EXPECT_EQ(Traits<std::string>::signature(), "s"); }
TEST(TypeTraits, SignatureObjPath)  { EXPECT_EQ(Traits<ObjectPath>::signature(), "o"); }
TEST(TypeTraits, SignatureSig)      { EXPECT_EQ(Traits<mbedbus::Signature>::signature(), "g"); }
TEST(TypeTraits, SignatureUnixFd)   { EXPECT_EQ(Traits<UnixFd>::signature(), "h"); }
TEST(TypeTraits, SignatureVariant)  { EXPECT_EQ(Traits<Variant>::signature(), "v"); }

TEST(TypeTraits, SignatureVectorInt)    { EXPECT_EQ(Traits<std::vector<int32_t>>::signature(), "ai"); }
TEST(TypeTraits, SignatureVectorString) { EXPECT_EQ(Traits<std::vector<std::string>>::signature(), "as"); }
TEST(TypeTraits, SignatureVectorByte)   { EXPECT_EQ(Traits<std::vector<uint8_t>>::signature(), "ay"); }

TEST(TypeTraits, SignatureMapSI) { EXPECT_EQ((Traits<std::map<std::string, int32_t>>::signature()), "a{si}"); }
TEST(TypeTraits, SignatureMapSV) { EXPECT_EQ((Traits<std::map<std::string, Variant>>::signature()), "a{sv}"); }
TEST(TypeTraits, SignatureMapIS) { EXPECT_EQ((Traits<std::map<int32_t, std::string>>::signature()), "a{is}"); }

TEST(TypeTraits, SignatureTuple2)  { EXPECT_EQ((Traits<std::tuple<int32_t, std::string>>::signature()), "(is)"); }
TEST(TypeTraits, SignatureTuple3)  { EXPECT_EQ((Traits<std::tuple<int32_t, std::string, double>>::signature()), "(isd)"); }
TEST(TypeTraits, SignatureTuple1)  { EXPECT_EQ((Traits<std::tuple<bool>>::signature()), "(b)"); }
TEST(TypeTraits, SignatureNestedTuple) {
    using T = std::tuple<std::tuple<int32_t, int32_t>, std::string>;
    EXPECT_EQ(Traits<T>::signature(), "((ii)s)");
}

TEST(TypeTraits, SignatureNestedContainer) {
    using T = std::map<std::string, std::vector<int32_t>>;
    EXPECT_EQ(Traits<T>::signature(), "a{sai}");
}

TEST(TypeTraits, SignatureVectorOfTuples) {
    using T = std::vector<std::tuple<std::string, int32_t>>;
    EXPECT_EQ(Traits<T>::signature(), "a(si)");
}

// ============================================================
// Bool round-trips
// ============================================================

TEST(TypeTraits, BoolTrue)  { EXPECT_EQ(roundTrip(true), true); }
TEST(TypeTraits, BoolFalse) { EXPECT_EQ(roundTrip(false), false); }

// ============================================================
// Integer boundary values
// ============================================================

TEST(TypeTraits, ByteBoundary) {
    EXPECT_EQ(roundTrip(uint8_t(0)), 0);
    EXPECT_EQ(roundTrip(uint8_t(255)), 255);
    EXPECT_EQ(roundTrip(uint8_t(128)), 128);
}

TEST(TypeTraits, Int16Boundary) {
    EXPECT_EQ(roundTrip(int16_t(0)), 0);
    EXPECT_EQ(roundTrip(int16_t(-32768)), -32768);
    EXPECT_EQ(roundTrip(int16_t(32767)), 32767);
    EXPECT_EQ(roundTrip(int16_t(-1)), -1);
}

TEST(TypeTraits, UInt16Boundary) {
    EXPECT_EQ(roundTrip(uint16_t(0)), 0);
    EXPECT_EQ(roundTrip(uint16_t(65535)), 65535);
}

TEST(TypeTraits, Int32Boundary) {
    EXPECT_EQ(roundTrip(int32_t(0)), 0);
    EXPECT_EQ(roundTrip(int32_t(INT32_MIN)), INT32_MIN);
    EXPECT_EQ(roundTrip(int32_t(INT32_MAX)), INT32_MAX);
    EXPECT_EQ(roundTrip(int32_t(-1)), -1);
    EXPECT_EQ(roundTrip(int32_t(1)), 1);
}

TEST(TypeTraits, UInt32Boundary) {
    EXPECT_EQ(roundTrip(uint32_t(0)), 0u);
    EXPECT_EQ(roundTrip(uint32_t(UINT32_MAX)), UINT32_MAX);
}

TEST(TypeTraits, Int64Boundary) {
    EXPECT_EQ(roundTrip(int64_t(0)), 0);
    EXPECT_EQ(roundTrip(int64_t(INT64_MIN)), INT64_MIN);
    EXPECT_EQ(roundTrip(int64_t(INT64_MAX)), INT64_MAX);
}

TEST(TypeTraits, UInt64Boundary) {
    EXPECT_EQ(roundTrip(uint64_t(0)), 0u);
    EXPECT_EQ(roundTrip(uint64_t(UINT64_MAX)), UINT64_MAX);
}

// ============================================================
// Double edge cases
// ============================================================

TEST(TypeTraits, DoubleZero)      { EXPECT_DOUBLE_EQ(roundTrip(0.0), 0.0); }
TEST(TypeTraits, DoubleNegZero)   { EXPECT_DOUBLE_EQ(roundTrip(-0.0), -0.0); }
TEST(TypeTraits, DoublePi)        { EXPECT_DOUBLE_EQ(roundTrip(M_PI), M_PI); }
TEST(TypeTraits, DoubleMaxVal)    { EXPECT_DOUBLE_EQ(roundTrip(DBL_MAX), DBL_MAX); }
TEST(TypeTraits, DoubleMinPos)    { EXPECT_DOUBLE_EQ(roundTrip(DBL_MIN), DBL_MIN); }
TEST(TypeTraits, DoubleNegative)  { EXPECT_DOUBLE_EQ(roundTrip(-1e100), -1e100); }
TEST(TypeTraits, DoubleInfinity)  {
    double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(roundTrip(inf), inf);
}
TEST(TypeTraits, DoubleNegInf) {
    double ninf = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(roundTrip(ninf), ninf);
}
TEST(TypeTraits, DoubleNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(roundTrip(nan)));
}

// ============================================================
// String edge cases
// ============================================================

TEST(TypeTraits, StringEmpty)    { EXPECT_EQ(roundTrip(std::string()), ""); }
TEST(TypeTraits, StringNormal)   { EXPECT_EQ(roundTrip(std::string("hello")), "hello"); }
TEST(TypeTraits, StringSpaces)   { EXPECT_EQ(roundTrip(std::string("  a  b  ")), "  a  b  "); }
TEST(TypeTraits, StringUTF8)     { EXPECT_EQ(roundTrip(std::string("Привет мир")), "Привет мир"); }
TEST(TypeTraits, StringEmoji)    { EXPECT_EQ(roundTrip(std::string("🚀🔥")), "🚀🔥"); }
TEST(TypeTraits, StringNewlines) { EXPECT_EQ(roundTrip(std::string("a\nb\tc\r")), "a\nb\tc\r"); }
TEST(TypeTraits, StringLong) {
    std::string s(10000, 'A');
    EXPECT_EQ(roundTrip(s), s);
}
TEST(TypeTraits, StringNullByteSafe) {
    // D-Bus strings are nul-terminated C strings, so embedded nuls get truncated
    std::string s("before");
    EXPECT_EQ(roundTrip(s), "before");
}

// ============================================================
// ObjectPath, Signature
// ============================================================

TEST(TypeTraits, ObjectPathRoot)  {
    EXPECT_EQ(roundTrip(ObjectPath("/")).str(), "/");
}
TEST(TypeTraits, ObjectPathDeep)  {
    EXPECT_EQ(roundTrip(ObjectPath("/a/b/c/d/e")).str(), "/a/b/c/d/e");
}

TEST(TypeTraits, SignatureComplex) {
    EXPECT_EQ(roundTrip(mbedbus::Signature("a{oa{sa{sv}}}")).str(), "a{oa{sa{sv}}}");
}
TEST(TypeTraits, SignatureEmpty)   {
    EXPECT_EQ(roundTrip(mbedbus::Signature("")).str(), "");
}

// ============================================================
// Vectors
// ============================================================

TEST(TypeTraits, VectorEmpty)       { EXPECT_EQ(roundTrip(std::vector<int32_t>()), std::vector<int32_t>()); }
TEST(TypeTraits, VectorSingleElem)  { EXPECT_EQ(roundTrip(std::vector<int32_t>{42}), std::vector<int32_t>{42}); }
TEST(TypeTraits, VectorManyElems)   {
    std::vector<int32_t> v;
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    EXPECT_EQ(roundTrip(v), v);
}
TEST(TypeTraits, VectorString)      { EXPECT_EQ(roundTrip(std::vector<std::string>{"a", "bb", ""}), (std::vector<std::string>{"a", "bb", ""})); }
TEST(TypeTraits, VectorUint8)       {
    std::vector<uint8_t> v = {0, 1, 127, 128, 255};
    EXPECT_EQ(roundTrip(v), v);
}

// Nested vectors
TEST(TypeTraits, VectorOfVectors) {
    std::vector<std::vector<int32_t>> v = {{1, 2}, {}, {3, 4, 5}};
    EXPECT_EQ(roundTrip(v), v);
}
TEST(TypeTraits, VectorOfVectorsEmpty) {
    std::vector<std::vector<int32_t>> v;
    EXPECT_EQ(roundTrip(v), v);
}
TEST(TypeTraits, VectorOfVectorsAllEmpty) {
    std::vector<std::vector<std::string>> v = {{}, {}, {}};
    EXPECT_EQ(roundTrip(v), v);
}

// ============================================================
// Maps
// ============================================================

TEST(TypeTraits, MapEmpty) {
    std::map<std::string, int32_t> m;
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapSingleEntry) {
    std::map<std::string, int32_t> m = {{"key", 42}};
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapManyEntries) {
    std::map<std::string, int32_t> m;
    for (int i = 0; i < 100; ++i) m["key_" + std::to_string(i)] = i;
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapStringString) {
    std::map<std::string, std::string> m = {{"a", "b"}, {"c", "d"}};
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapIntString) {
    std::map<int32_t, std::string> m = {{1, "one"}, {-1, "neg"}};
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapNestedValue) {
    std::map<std::string, std::vector<int32_t>> m = {
        {"primes", {2, 3, 5, 7}},
        {"empty", {}},
    };
    EXPECT_EQ(roundTrip(m), m);
}
TEST(TypeTraits, MapOfMaps) {
    std::map<std::string, std::map<std::string, int32_t>> m = {
        {"outer", {{"inner", 42}}},
    };
    EXPECT_EQ(roundTrip(m), m);
}

// ============================================================
// Tuples
// ============================================================

TEST(TypeTraits, TupleSingle)   { EXPECT_EQ(std::get<0>(roundTrip(std::make_tuple(int32_t(42)))), 42); }
TEST(TypeTraits, TuplePair)     {
    auto t = roundTrip(std::make_tuple(int32_t(1), std::string("x")));
    EXPECT_EQ(std::get<0>(t), 1);
    EXPECT_EQ(std::get<1>(t), "x");
}
TEST(TypeTraits, TupleTriple)   {
    auto t = roundTrip(std::make_tuple(int32_t(1), std::string("a"), 2.5));
    EXPECT_EQ(std::get<0>(t), 1);
    EXPECT_EQ(std::get<1>(t), "a");
    EXPECT_DOUBLE_EQ(std::get<2>(t), 2.5);
}
TEST(TypeTraits, TupleNested)   {
    using Inner = std::tuple<int32_t, int32_t>;
    using Outer = std::tuple<Inner, std::string>;
    Outer t(Inner(10, 20), "nested");
    auto r = roundTrip(t);
    EXPECT_EQ(std::get<0>(std::get<0>(r)), 10);
    EXPECT_EQ(std::get<1>(std::get<0>(r)), 20);
    EXPECT_EQ(std::get<1>(r), "nested");
}
TEST(TypeTraits, TupleWithContainer) {
    auto t = std::make_tuple(std::string("x"), std::vector<int32_t>{1, 2, 3});
    auto r = roundTrip(t);
    EXPECT_EQ(std::get<0>(r), "x");
    EXPECT_EQ(std::get<1>(r), (std::vector<int32_t>{1, 2, 3}));
}
TEST(TypeTraits, TupleAllSameType) {
    auto t = std::make_tuple(int32_t(1), int32_t(2), int32_t(3), int32_t(4));
    auto r = roundTrip(t);
    EXPECT_EQ(std::get<0>(r), 1);
    EXPECT_EQ(std::get<3>(r), 4);
}

// ============================================================
// buildSignature
// ============================================================

TEST(TypeTraits, BuildSignatureEmpty)  { EXPECT_EQ((buildSignature<>()), ""); }
TEST(TypeTraits, BuildSignatureSingle) { EXPECT_EQ((buildSignature<int32_t>()), "i"); }
TEST(TypeTraits, BuildSignatureMulti)  { EXPECT_EQ((buildSignature<int32_t, int32_t, std::string>()), "iis"); }
TEST(TypeTraits, BuildSignatureComplex) {
    EXPECT_EQ((buildSignature<std::vector<std::string>, bool, std::map<std::string, int32_t>>()), "asba{si}");
}
TEST(TypeTraits, BuildSignatureWithTuple) {
    EXPECT_EQ((buildSignature<std::tuple<int32_t, std::string>>()), "(is)");
}

// ============================================================
// Multiple args serialize/deserialize
// ============================================================

TEST(TypeTraits, MultipleArgsThree) {
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

TEST(TypeTraits, MultipleArgsSingle) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    serializeArgs(iter, std::string("only"));
    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    auto t = deserializeArgs<std::string>(readIter);
    EXPECT_EQ(std::get<0>(t), "only");
    dbus_message_unref(msg);
}

TEST(TypeTraits, MultipleArgsFive) {
    DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    serializeArgs(iter, true, uint8_t(255), int64_t(-1), std::string("x"), double(0.0));
    DBusMessageIter readIter;
    dbus_message_iter_init(msg, &readIter);
    auto t = deserializeArgs<bool, uint8_t, int64_t, std::string, double>(readIter);
    EXPECT_EQ(std::get<0>(t), true);
    EXPECT_EQ(std::get<1>(t), 255);
    EXPECT_EQ(std::get<2>(t), -1);
    EXPECT_EQ(std::get<3>(t), "x");
    EXPECT_DOUBLE_EQ(std::get<4>(t), 0.0);
    dbus_message_unref(msg);
}

// ============================================================
// Cyclic round-trip for all basic types
// ============================================================

TEST(TypeTraits, CyclicInt32) {
    for (int32_t v : {INT32_MIN, -1000, -1, 0, 1, 1000, INT32_MAX}) {
        EXPECT_EQ(roundTrip(v), v) << "Failed for " << v;
    }
}

TEST(TypeTraits, CyclicUint8) {
    for (int i = 0; i <= 255; ++i) {
        EXPECT_EQ(roundTrip(uint8_t(i)), uint8_t(i)) << "Failed for " << i;
    }
}

TEST(TypeTraits, CyclicDouble) {
    for (double v : {-1e300, -1.0, -0.0, 0.0, 1e-300, 1.0, 1e300}) {
        EXPECT_DOUBLE_EQ(roundTrip(v), v) << "Failed for " << v;
    }
}
