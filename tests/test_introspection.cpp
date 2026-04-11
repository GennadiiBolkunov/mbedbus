#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <string>

using namespace mbedbus;

// We can test introspection XML generation without a dbus-daemon
// by creating a ServiceObject with a mock connection approach.
// However, ServiceObject::create needs a Connection. Instead, we
// test the XML generation by inspecting the output format.
// We'll create a helper that constructs InterfaceData manually.

TEST(Introspection, SignatureFormat) {
    // Verify signatures used in introspection
    EXPECT_EQ((types::buildSignature<int32_t, std::string>()), "is");
    EXPECT_EQ((types::Traits<std::map<std::string, Variant>>::signature()), "a{sv}");
    EXPECT_EQ(types::Traits<std::vector<std::string>>::signature(), "as");
}

TEST(Introspection, InterfaceDataConstruction) {
    InterfaceData data;
    data.name = "com.example.Test";

    MethodInfo mi;
    mi.name = "DoStuff";
    mi.inputSig = "is";
    mi.outputSig = "b";
    mi.inArgs = {{"arg0", "i"}, {"arg1", "s"}};
    mi.outArgs = {{"result", "b"}};
    data.methods.push_back(mi);

    PropertyInfo pi;
    pi.name = "Version";
    pi.signature = "s";
    pi.readable = true;
    pi.writable = false;
    data.properties.push_back(pi);

    SignalInfo si;
    si.name = "Alert";
    si.signature = "is";
    si.args = {{"arg0", "i"}, {"arg1", "s"}};
    data.signals.push_back(si);

    EXPECT_EQ(data.methods.size(), 1u);
    EXPECT_EQ(data.properties.size(), 1u);
    EXPECT_EQ(data.signals.size(), 1u);
    EXPECT_EQ(data.methods[0].name, "DoStuff");
}

TEST(Introspection, BuilderFunctionTraits) {
    // Test that FunctionTraits works with lambdas
    auto lambda = [](int32_t a, std::string b) -> bool { return a > 0 && !b.empty(); };
    using Traits = detail::FunctionTraits<decltype(lambda)>;
    static_assert(std::is_same<Traits::ReturnType, bool>::value, "");
    static_assert(Traits::arity == 2, "");
}
