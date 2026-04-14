#include <gtest/gtest.h>
#include <mbedbus/Error.h>
#include <string>
#include <vector>
#include <memory>

using namespace mbedbus;

// ============================================================
// Error class
// ============================================================

TEST(Error, TwoArgConstructor) {
    Error e("com.example.Fail", "something broke");
    EXPECT_EQ(e.name(), "com.example.Fail");
    EXPECT_EQ(e.message(), "something broke");
    EXPECT_STREQ(e.what(), "something broke");
}

TEST(Error, SingleArgConstructor) {
    Error e("oops");
    EXPECT_EQ(e.name(), "com.mbedbus.Error");
    EXPECT_EQ(e.message(), "oops");
}

TEST(Error, EmptyStrings) {
    Error e("", "");
    EXPECT_EQ(e.name(), "");
    EXPECT_EQ(e.message(), "");
    EXPECT_STREQ(e.what(), "");
}

TEST(Error, InheritsFromRuntimeError) {
    Error e("name", "msg");
    const std::runtime_error& re = e;
    EXPECT_STREQ(re.what(), "msg");
}

TEST(Error, CatchAsStdException) {
    try {
        throw Error("e.Name", "payload");
    } catch (const std::exception& ex) {
        EXPECT_STREQ(ex.what(), "payload");
    }
}

TEST(Error, CopyPreservesFields) {
    Error e1("a.b", "text");
    Error e2(e1);
    EXPECT_EQ(e2.name(), "a.b");
    EXPECT_EQ(e2.message(), "text");
}

TEST(Error, LongStrings) {
    std::string longName(1000, 'x');
    std::string longMsg(5000, 'y');
    Error e(longName, longMsg);
    EXPECT_EQ(e.name().size(), 1000u);
    EXPECT_EQ(e.message().size(), 5000u);
}

// ============================================================
// Expected<T> — construction
// ============================================================

TEST(Expected, ValueConstruction) {
    Expected<int> e(42);
    ASSERT_TRUE(e.hasValue());
    ASSERT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.value(), 42);
}

TEST(Expected, ErrorConstruction) {
    Expected<int> e(Error("test.Error", "something failed"));
    ASSERT_FALSE(e.hasValue());
    EXPECT_EQ(e.error().name(), "test.Error");
    EXPECT_EQ(e.error().message(), "something failed");
}

TEST(Expected, ValueFromLiteral) {
    Expected<int> e = 100;
    EXPECT_TRUE(e.hasValue());
    EXPECT_EQ(e.value(), 100);
}

TEST(Expected, ErrorFromTemporary) {
    Expected<int> e = Error("x", "y");
    EXPECT_FALSE(e.hasValue());
}

TEST(Expected, StringValue) {
    Expected<std::string> e(std::string("hello"));
    EXPECT_EQ(e.value(), "hello");
}

TEST(Expected, VectorValue) {
    std::vector<int> v = {1, 2, 3};
    Expected<std::vector<int>> e(v);
    EXPECT_EQ(e.value().size(), 3u);
    EXPECT_EQ(e.value()[2], 3);
}

// ============================================================
// Expected<T> — access and throwing
// ============================================================

TEST(Expected, ValueThrowsOnError) {
    Expected<int> e(Error("e", "msg"));
    EXPECT_THROW(e.value(), Error);
}

TEST(Expected, ConstValueThrowsOnError) {
    const Expected<int> e(Error("e", "msg"));
    EXPECT_THROW(e.value(), Error);
}

TEST(Expected, ErrorThrowsOnValue) {
    Expected<int> e(42);
    EXPECT_THROW(e.error(), std::logic_error);
}

TEST(Expected, ValueOrWithValue) {
    Expected<int> e(10);
    EXPECT_EQ(e.valueOr(99), 10);
}

TEST(Expected, ValueOrWithError) {
    Expected<int> e(Error("e", "m"));
    EXPECT_EQ(e.valueOr(99), 99);
}

TEST(Expected, ValueOrString) {
    Expected<std::string> e(Error("e", "m"));
    EXPECT_EQ(e.valueOr("default"), "default");
}

TEST(Expected, MutableValueAccess) {
    Expected<int> e(10);
    e.value() = 20;
    EXPECT_EQ(e.value(), 20);
}

// ============================================================
// Expected<T> — copy and move
// ============================================================

TEST(Expected, CopyValue) {
    Expected<int> e1(42);
    Expected<int> e2(e1);
    EXPECT_EQ(e1.value(), 42);
    EXPECT_EQ(e2.value(), 42);
}

TEST(Expected, CopyError) {
    Expected<int> e1(Error("n", "m"));
    Expected<int> e2(e1);
    EXPECT_EQ(e1.error().name(), "n");
    EXPECT_EQ(e2.error().name(), "n");
}

TEST(Expected, MoveValue) {
    Expected<std::string> e1(std::string("hello"));
    Expected<std::string> e2(std::move(e1));
    EXPECT_EQ(e2.value(), "hello");
}

TEST(Expected, MoveError) {
    Expected<int> e1(Error("n", "m"));
    Expected<int> e2(std::move(e1));
    EXPECT_EQ(e2.error().name(), "n");
}

TEST(Expected, CopyAssignValueToValue) {
    Expected<int> e1(10);
    Expected<int> e2(20);
    e2 = e1;
    EXPECT_EQ(e2.value(), 10);
}

TEST(Expected, CopyAssignErrorToValue) {
    Expected<int> e1(Error("n", "m"));
    Expected<int> e2(42);
    e2 = e1;
    EXPECT_FALSE(e2.hasValue());
    EXPECT_EQ(e2.error().name(), "n");
}

TEST(Expected, CopyAssignValueToError) {
    Expected<int> e1(42);
    Expected<int> e2(Error("n", "m"));
    e2 = e1;
    EXPECT_TRUE(e2.hasValue());
    EXPECT_EQ(e2.value(), 42);
}

TEST(Expected, CopyAssignErrorToError) {
    Expected<int> e1(Error("a", "1"));
    Expected<int> e2(Error("b", "2"));
    e2 = e1;
    EXPECT_EQ(e2.error().name(), "a");
}

TEST(Expected, MoveAssignValueToError) {
    Expected<std::string> e1(std::string("hi"));
    Expected<std::string> e2(Error("e", "m"));
    e2 = std::move(e1);
    EXPECT_TRUE(e2.hasValue());
    EXPECT_EQ(e2.value(), "hi");
}

TEST(Expected, SelfAssignment) {
    Expected<int> e(42);
    e = e;
    EXPECT_EQ(e.value(), 42);
}

// ============================================================
// Expected<T> — map
// ============================================================

TEST(Expected, MapValue) {
    Expected<int> e(10);
    auto e2 = e.map([](int v) { return v * 2; });
    EXPECT_EQ(e2.value(), 20);
}

TEST(Expected, MapError) {
    Expected<int> e(Error("e", "m"));
    auto e2 = e.map([](int v) { return v * 2; });
    EXPECT_FALSE(e2.hasValue());
    EXPECT_EQ(e2.error().name(), "e");
}

TEST(Expected, MapChangesType) {
    Expected<int> e(42);
    auto e2 = e.map([](int v) { return std::to_string(v); });
    EXPECT_EQ(e2.value(), "42");
}

TEST(Expected, MapChaining) {
    Expected<int> e(5);
    auto result = e.map([](int v) { return v + 1; })
        .map([](int v) { return v * 10; })
        .map([](int v) { return std::to_string(v); });
    EXPECT_EQ(result.value(), "60");
}

TEST(Expected, MapChainingWithError) {
    Expected<int> e(Error("e", "m"));
    auto result = e.map([](int v) { return v + 1; })
        .map([](int v) { return v * 10; });
    EXPECT_FALSE(result.hasValue());
}

// ============================================================
// Expected<void>
// ============================================================

TEST(Expected, VoidSuccess) {
    Expected<void> ok;
    EXPECT_TRUE(ok.hasValue());
    EXPECT_TRUE(static_cast<bool>(ok));
}

TEST(Expected, VoidError) {
    Expected<void> fail(Error("e", "m"));
    EXPECT_FALSE(fail.hasValue());
    EXPECT_EQ(fail.error().name(), "e");
}

TEST(Expected, VoidErrorThrowsOnSuccess) {
    Expected<void> ok;
    EXPECT_THROW(ok.error(), std::logic_error);
}

// ============================================================
// Expected<T> with complex types
// ============================================================

TEST(Expected, WithSharedPtr) {
    auto ptr = std::make_shared<int>(42);
    Expected<std::shared_ptr<int>> e(ptr);
    EXPECT_EQ(*e.value(), 42);
    EXPECT_EQ(ptr.use_count(), 2);
}

TEST(Expected, WithPairType) {
    Expected<std::pair<int, std::string>> e(std::make_pair(1, std::string("x")));
    EXPECT_EQ(e.value().first, 1);
    EXPECT_EQ(e.value().second, "x");
}

// ============================================================
// Stress: many constructions
// ============================================================

TEST(Expected, LoopConstructDestruct) {
    for (int i = 0; i < 10000; ++i) {
        if (i % 2 == 0) {
            Expected<std::string> e(std::to_string(i));
            EXPECT_EQ(e.value(), std::to_string(i));
        } else {
            Expected<std::string> e(Error("e", std::to_string(i)));
            EXPECT_EQ(e.error().message(), std::to_string(i));
        }
    }
}
