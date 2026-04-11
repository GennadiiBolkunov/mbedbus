#include <gtest/gtest.h>
#include <mbedbus/Error.h>
#include <string>

using namespace mbedbus;

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

TEST(Expected, ValueThrowsOnError) {
    Expected<int> e(Error("e", "msg"));
    EXPECT_THROW(e.value(), Error);
}

TEST(Expected, ErrorThrowsOnValue) {
    Expected<int> e(42);
    EXPECT_THROW(e.error(), std::logic_error);
}

TEST(Expected, ValueOr) {
    Expected<int> good(10);
    Expected<int> bad(Error("e", "m"));
    EXPECT_EQ(good.valueOr(99), 10);
    EXPECT_EQ(bad.valueOr(99), 99);
}

TEST(Expected, MoveConstruction) {
    Expected<std::string> e(std::string("hello"));
    Expected<std::string> e2(std::move(e));
    EXPECT_EQ(e2.value(), "hello");
}

TEST(Expected, CopyConstruction) {
    Expected<int> e(42);
    Expected<int> e2(e);
    EXPECT_EQ(e2.value(), 42);
    EXPECT_EQ(e.value(), 42);
}

TEST(Expected, Map) {
    Expected<int> e(10);
    auto e2 = e.map([](int v) { return v * 2; });
    EXPECT_EQ(e2.value(), 20);

    Expected<int> err(Error("e", "m"));
    auto e3 = err.map([](int v) { return v * 2; });
    EXPECT_FALSE(e3.hasValue());
}

TEST(Expected, VoidSpecialization) {
    Expected<void> ok;
    EXPECT_TRUE(ok.hasValue());

    Expected<void> fail(Error("e", "m"));
    EXPECT_FALSE(fail.hasValue());
    EXPECT_EQ(fail.error().name(), "e");
}

TEST(Error, InheritsFromRuntimeError) {
    Error e("name", "msg");
    const std::runtime_error& re = e;
    EXPECT_STREQ(re.what(), "msg");
}
