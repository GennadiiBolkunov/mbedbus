#include <gtest/gtest.h>
#include <mbedbus/mbedbus.h>
#include <string>

using namespace mbedbus;

// ============================================================
// XML generation tests (no D-Bus connection)
// ============================================================

TEST(Introspection, SignatureFormat) {
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
}

TEST(Introspection, BuilderFunctionTraits) {
    auto lambda = [](int32_t a, std::string b) -> bool { return a > 0 && !b.empty(); };
    using Traits = detail::FunctionTraits<decltype(lambda)>;
    static_assert(std::is_same<Traits::ReturnType, bool>::value, "");
    static_assert(Traits::arity == 2, "");
}

// ============================================================
// IntrospectionParser: basic parsing
// ============================================================

TEST(Parser, EmptyString) {
    auto node = IntrospectionParser::parse("");
    EXPECT_TRUE(node.interfaces.empty());
    EXPECT_TRUE(node.childNodes.empty());
    EXPECT_TRUE(node.name.empty());
}

TEST(Parser, MinimalNode) {
    auto node = IntrospectionParser::parse("<node/>");
    EXPECT_TRUE(node.interfaces.empty());
}

TEST(Parser, NodeWithName) {
    auto node = IntrospectionParser::parse(R"(<node name="/com/example"/>)");
    EXPECT_EQ(node.name, "/com/example");
}

TEST(Parser, SimpleMethod) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.test.A">
    <method name="Ping"/>
  </interface>
</node>
)");
    ASSERT_EQ(node.interfaces.size(), 1u);
    ASSERT_EQ(node.interfaces[0].methods.size(), 1u);
    EXPECT_EQ(node.interfaces[0].methods[0].name, "Ping");
    EXPECT_EQ(node.interfaces[0].methods[0].inputSignature(), "");
    EXPECT_EQ(node.interfaces[0].methods[0].outputSignature(), "");
}

TEST(Parser, MethodWithArgs) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.test.A">
    <method name="Add">
      <arg name="a" type="i" direction="in"/>
      <arg name="b" type="i" direction="in"/>
      <arg name="result" type="i" direction="out"/>
    </method>
  </interface>
</node>
)");
    auto& m = node.interfaces[0].methods[0];
    EXPECT_EQ(m.name, "Add");
    EXPECT_EQ(m.inputSignature(), "ii");
    EXPECT_EQ(m.outputSignature(), "i");
    EXPECT_EQ(m.inArgs.size(), 2u);
    EXPECT_EQ(m.outArgs.size(), 1u);
    EXPECT_EQ(m.inArgs[0].name, "a");
    EXPECT_EQ(m.inArgs[1].name, "b");
    EXPECT_EQ(m.outArgs[0].name, "result");
}

TEST(Parser, ArgDefaultDirectionIsIn) {
    // D-Bus spec: direction defaults to "in" for methods
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.test.A">
    <method name="Foo">
      <arg type="s"/>
    </method>
  </interface>
</node>
)");
    auto& m = node.interfaces[0].methods[0];
    EXPECT_EQ(m.inputSignature(), "s");
    EXPECT_EQ(m.outputSignature(), "");
}

TEST(Parser, ArgWithoutName) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.test.A">
    <method name="Foo">
      <arg type="i" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
  </interface>
</node>
)");
    auto& m = node.interfaces[0].methods[0];
    EXPECT_EQ(m.inArgs[0].name, "");
    EXPECT_EQ(m.outArgs[0].name, "");
    EXPECT_EQ(m.inputSignature(), "i");
    EXPECT_EQ(m.outputSignature(), "s");
}

// ============================================================
// Properties
// ============================================================

TEST(Parser, PropertyRead) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.test.A">
    <property name="Version" type="s" access="read"/>
  </interface>
</node>
)");
    ASSERT_EQ(node.interfaces[0].properties.size(), 1u);
    auto& p = node.interfaces[0].properties[0];
    EXPECT_EQ(p.name, "Version");
    EXPECT_EQ(p.signature, "s");
    EXPECT_EQ(p.access, PropertyAccess::Read);
}

TEST(Parser, PropertyReadWrite) {
    auto node = IntrospectionParser::parse(R"(
<node><interface name="a"><property name="X" type="i" access="readwrite"/></interface></node>
)");
    EXPECT_EQ(node.interfaces[0].properties[0].access, PropertyAccess::ReadWrite);
}

TEST(Parser, PropertyWrite) {
    auto node = IntrospectionParser::parse(R"(
<node><interface name="a"><property name="X" type="s" access="write"/></interface></node>
)");
    EXPECT_EQ(node.interfaces[0].properties[0].access, PropertyAccess::Write);
}

// ============================================================
// Signals
// ============================================================

TEST(Parser, SignalNoArgs) {
    auto node = IntrospectionParser::parse(R"(
<node><interface name="a"><signal name="Ping"/></interface></node>
)");
    ASSERT_EQ(node.interfaces[0].signals.size(), 1u);
    EXPECT_EQ(node.interfaces[0].signals[0].name, "Ping");
    EXPECT_EQ(node.interfaces[0].signals[0].signature(), "");
}

TEST(Parser, SignalWithArgs) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="a">
    <signal name="Changed">
      <arg type="s"/>
      <arg type="a{sv}"/>
      <arg type="as"/>
    </signal>
  </interface>
</node>
)");
    auto& s = node.interfaces[0].signals[0];
    EXPECT_EQ(s.signature(), "sa{sv}as");
    EXPECT_EQ(s.args.size(), 3u);
}

// ============================================================
// Multiple interfaces
// ============================================================

TEST(Parser, MultipleInterfaces) {
    auto node = IntrospectionParser::parse(R"(
<node name="/obj">
  <interface name="com.A"><method name="Foo"/></interface>
  <interface name="com.B"><method name="Bar"/></interface>
  <interface name="com.C"><method name="Baz"/></interface>
</node>
)");
    ASSERT_EQ(node.interfaces.size(), 3u);
    EXPECT_EQ(node.interfaces[0].name, "com.A");
    EXPECT_EQ(node.interfaces[1].name, "com.B");
    EXPECT_EQ(node.interfaces[2].name, "com.C");
}

// ============================================================
// Child nodes
// ============================================================

TEST(Parser, ChildNodes) {
    auto node = IntrospectionParser::parse(R"(
<node name="/devices">
  <interface name="org.freedesktop.DBus.ObjectManager"/>
  <node name="1"/>
  <node name="2"/>
  <node name="sensor_3"/>
</node>
)");
    EXPECT_EQ(node.name, "/devices");
    ASSERT_EQ(node.childNodes.size(), 3u);
    EXPECT_EQ(node.childNodes[0], "1");
    EXPECT_EQ(node.childNodes[1], "2");
    EXPECT_EQ(node.childNodes[2], "sensor_3");
}

TEST(Parser, NoChildNodes) {
    auto node = IntrospectionParser::parse("<node name='/a'><interface name='b'/></node>");
    EXPECT_TRUE(node.childNodes.empty());
}

// ============================================================
// Complex signatures
// ============================================================

TEST(Parser, ComplexSignatures) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.Complex">
    <method name="GetAll"><arg type="a{sa{sv}}" direction="out"/></method>
    <property name="Map" type="a{sv}" access="read"/>
  </interface>
</node>
)");
    EXPECT_EQ(node.interfaces[0].methods[0].outputSignature(), "a{sa{sv}}");
    EXPECT_EQ(node.interfaces[0].properties[0].signature, "a{sv}");
}

// ============================================================
// Lookup helpers
// ============================================================

TEST(Parser, FindInterface) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.A"><method name="M"/></interface>
  <interface name="com.B"><property name="P" type="i" access="read"/></interface>
</node>
)");
    EXPECT_NE(node.findInterface("com.A"), nullptr);
    EXPECT_NE(node.findInterface("com.B"), nullptr);
    EXPECT_EQ(node.findInterface("com.C"), nullptr);
}

TEST(Parser, FindMethod) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.A"><method name="Foo"/><method name="Bar"/></interface>
</node>
)");
    auto* m = node.findMethod("com.A", "Foo");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name, "Foo");
    EXPECT_EQ(node.findMethod("com.A", "Baz"), nullptr);
    EXPECT_EQ(node.findMethod("com.B", "Foo"), nullptr);
}

TEST(Parser, FindProperty) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.A">
    <property name="X" type="i" access="read"/>
    <property name="Y" type="s" access="readwrite"/>
  </interface>
</node>
)");
    auto* iface = node.findInterface("com.A");
    ASSERT_NE(iface, nullptr);
    EXPECT_NE(iface->findProperty("X"), nullptr);
    EXPECT_NE(iface->findProperty("Y"), nullptr);
    EXPECT_EQ(iface->findProperty("Z"), nullptr);
    EXPECT_EQ(iface->findMethod("X"), nullptr);
}

// ============================================================
// DOCTYPE, comments, processing instructions
// ============================================================

TEST(Parser, SkipsDOCTYPE) {
    auto node = IntrospectionParser::parse(R"(
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node name="/test">
  <interface name="com.A"><method name="M"/></interface>
</node>
)");
    EXPECT_EQ(node.name, "/test");
    EXPECT_EQ(node.interfaces.size(), 1u);
}

TEST(Parser, SkipsXMLDeclaration) {
    auto node = IntrospectionParser::parse(R"(
<?xml version="1.0" encoding="UTF-8"?>
<node><interface name="com.A"><method name="M"/></interface></node>
)");
    EXPECT_EQ(node.interfaces.size(), 1u);
}

TEST(Parser, SkipsComments) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <!-- This is a comment -->
  <interface name="com.A">
    <!-- Another comment -->
    <method name="M"/>
  </interface>
</node>
)");
    EXPECT_EQ(node.interfaces.size(), 1u);
    EXPECT_EQ(node.interfaces[0].methods.size(), 1u);
}

TEST(Parser, IgnoresAnnotations) {
    auto node = IntrospectionParser::parse(R"(
<node>
  <interface name="com.A">
    <method name="M">
      <annotation name="org.freedesktop.DBus.Method.NoReply" value="true"/>
      <arg type="i" direction="in"/>
    </method>
  </interface>
</node>
)");
    EXPECT_EQ(node.interfaces[0].methods[0].inputSignature(), "i");
}

// ============================================================
// Single-quoted attributes
// ============================================================

TEST(Parser, SingleQuotedAttributes) {
    auto node = IntrospectionParser::parse(
        "<node name='/test'><interface name='com.A'>"
        "<method name='M'/></interface></node>");
    EXPECT_EQ(node.name, "/test");
    EXPECT_EQ(node.interfaces[0].name, "com.A");
}

// ============================================================
// Empty interface
// ============================================================

TEST(Parser, EmptyInterface) {
    auto node = IntrospectionParser::parse(R"(
<node><interface name="com.Empty"/></node>
)");
    ASSERT_EQ(node.interfaces.size(), 1u);
    EXPECT_EQ(node.interfaces[0].name, "com.Empty");
    EXPECT_TRUE(node.interfaces[0].methods.empty());
    EXPECT_TRUE(node.interfaces[0].properties.empty());
    EXPECT_TRUE(node.interfaces[0].signals.empty());
}

// ============================================================
// Round-trip: generated XML → parsed back
// ============================================================

TEST(Parser, RoundTripFullXml) {
    std::string xml = R"(<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node name="/com/test/Obj">
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg name="xml_data" type="s" direction="out"/>
    </method>
  </interface>
  <interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
    <method name="GetMachineId">
      <arg name="machine_uuid" type="s" direction="out"/>
    </method>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="property_name" type="s" direction="in"/>
      <arg name="value" type="v" direction="out"/>
    </method>
    <method name="Set">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="property_name" type="s" direction="in"/>
      <arg name="value" type="v" direction="in"/>
    </method>
    <method name="GetAll">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="all_properties" type="a{sv}" direction="out"/>
    </method>
    <signal name="PropertiesChanged">
      <arg name="interface_name" type="s"/>
      <arg name="changed_properties" type="a{sv}"/>
      <arg name="invalidated_properties" type="as"/>
    </signal>
  </interface>
  <interface name="com.test.MyService">
    <method name="Add">
      <arg name="arg0" type="i" direction="in"/>
      <arg name="arg1" type="i" direction="in"/>
      <arg name="result" type="i" direction="out"/>
    </method>
    <property name="Version" type="s" access="read"/>
    <property name="Volume" type="i" access="readwrite"/>
    <signal name="Alert">
      <arg name="arg0" type="i"/>
      <arg name="arg1" type="s"/>
    </signal>
  </interface>
</node>
)";
    auto node = IntrospectionParser::parse(xml);
    EXPECT_EQ(node.name, "/com/test/Obj");
    EXPECT_EQ(node.interfaces.size(), 4u);
    auto* propGet = node.findMethod("org.freedesktop.DBus.Properties", "Get");
    ASSERT_NE(propGet, nullptr);
    EXPECT_EQ(propGet->inputSignature(), "ss");
    EXPECT_EQ(propGet->outputSignature(), "v");
    auto* myIface = node.findInterface("com.test.MyService");
    ASSERT_NE(myIface, nullptr);
    EXPECT_EQ(myIface->methods.size(), 1u);
    EXPECT_EQ(myIface->properties.size(), 2u);
    EXPECT_EQ(myIface->signals.size(), 1u);
    EXPECT_EQ(myIface->findProperty("Version")->access, PropertyAccess::Read);
    EXPECT_EQ(myIface->findProperty("Volume")->access, PropertyAccess::ReadWrite);
}

// ============================================================
// Multiple methods in one interface
// ============================================================

TEST(Parser, ManyMethods) {
    std::string xml = "<node><interface name='com.A'>";
    for (int i = 0; i < 50; ++i) {
        xml += "<method name='M" + std::to_string(i) + "'>"
                                                       "<arg type='i' direction='in'/>"
                                                       "<arg type='s' direction='out'/>"
                                                       "</method>";
    }
    xml += "</interface></node>";
    auto node = IntrospectionParser::parse(xml);
    ASSERT_EQ(node.interfaces[0].methods.size(), 50u);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(node.interfaces[0].methods[i].name, "M" + std::to_string(i));
        EXPECT_EQ(node.interfaces[0].methods[i].inputSignature(), "i");
        EXPECT_EQ(node.interfaces[0].methods[i].outputSignature(), "s");
    }
}

// ============================================================
// Annotation tests
// ============================================================

TEST(Parser, MethodAnnotation) {
    auto node = IntrospectionParser::parse(R"(
<node name="/t">
  <interface name="com.A">
    <method name="Fire">
      <annotation name="org.freedesktop.DBus.Method.NoReply" value="true"/>
    </method>
  </interface>
</node>
)");
    auto* m = node.findMethod("com.A", "Fire");
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->isNoReply());
    EXPECT_EQ(m->annotations.size(), 1u);
    EXPECT_EQ(m->annotations.at("org.freedesktop.DBus.Method.NoReply"), "true");
}

TEST(Parser, DeprecatedProperty) {
    auto node = IntrospectionParser::parse(R"(
<node name="/t">
  <interface name="com.A">
    <property name="Old" type="u" access="read">
      <annotation name="org.freedesktop.DBus.Deprecated" value="true"/>
    </property>
    <property name="New" type="u" access="read"/>
  </interface>
</node>
)");
    auto* iface = node.findInterface("com.A");
    ASSERT_NE(iface, nullptr);
    EXPECT_TRUE(iface->findProperty("Old")->isDeprecated());
    EXPECT_FALSE(iface->findProperty("New")->isDeprecated());
}

TEST(Parser, EmitsChangedSignalConst) {
    auto node = IntrospectionParser::parse(R"(
<node name="/t">
  <interface name="com.A">
    <property name="Id" type="u" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="const"/>
    </property>
    <property name="Name" type="s" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="invalidates"/>
    </property>
    <property name="Status" type="s" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    </property>
    <property name="Default" type="i" access="read"/>
  </interface>
</node>
)");
    auto* iface = node.findInterface("com.A");
    EXPECT_EQ(iface->findProperty("Id")->emitsChangedSignal(), EmitsChangedSignal::Const);
    EXPECT_EQ(iface->findProperty("Name")->emitsChangedSignal(), EmitsChangedSignal::Invalidates);
    EXPECT_EQ(iface->findProperty("Status")->emitsChangedSignal(), EmitsChangedSignal::False);
    EXPECT_EQ(iface->findProperty("Default")->emitsChangedSignal(), EmitsChangedSignal::True);
}

TEST(Parser, ArgAnnotation) {
    auto node = IntrospectionParser::parse(R"(
<node name="/t">
  <interface name="com.A">
    <method name="Send">
      <arg name="data" type="ay" direction="in">
        <annotation name="org.gtk.GDBus.C.ForceGVariant" value="true"/>
      </arg>
      <arg name="fd" type="h" direction="out"/>
      <annotation name="org.gtk.GDBus.C.UnixFD" value="true"/>
    </method>
  </interface>
</node>
)");
    auto* m = node.findMethod("com.A", "Send");
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->inArgs.size(), 1u);
    EXPECT_EQ(m->inArgs[0].annotations.at("org.gtk.GDBus.C.ForceGVariant"), "true");
    EXPECT_EQ(m->annotations.at("org.gtk.GDBus.C.UnixFD"), "true");
}

TEST(Parser, InterfaceAnnotation) {
    auto node = IntrospectionParser::parse(R"(
<node name="/t">
  <interface name="com.A">
    <annotation name="org.freedesktop.DBus.Deprecated" value="true"/>
    <method name="M"/>
  </interface>
</node>
)");
    EXPECT_EQ(node.interfaces[0].annotations.at("org.freedesktop.DBus.Deprecated"), "true");
}

TEST(Parser, FullStingrayExample) {
    // Real-world XML from the user's example
    std::string xml = R"(
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="com.stingray.DemuxManager.Demux">
    <method name="AddSectionFilter">
      <arg name="pid" type="q" direction="in"/>
      <arg name="type" type="s" direction="in"/>
      <arg name="filter" type="ay" direction="in">
        <annotation name="org.gtk.GDBus.C.ForceGVariant" value="true"/>
      </arg>
      <arg name="mask" type="ay" direction="in">
        <annotation name="org.gtk.GDBus.C.ForceGVariant" value="true"/>
      </arg>
      <arg name="pipe" type="h" direction="out"/>
      <annotation name="org.gtk.GDBus.C.UnixFD" value="true"/>
    </method>
    <property name="Ids" type="au" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="const"/>
    </property>
    <property name="Id" type="u" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="const"/>
      <annotation name="org.freedesktop.DBus.Deprecated" value="true"/>
    </property>
  </interface>
</node>
)";
    auto node = IntrospectionParser::parse(xml);
    auto* iface = node.findInterface("com.stingray.DemuxManager.Demux");
    ASSERT_NE(iface, nullptr);

    // Method
    auto* m = iface->findMethod("AddSectionFilter");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->inputSignature(), "qsayay");
    EXPECT_EQ(m->outputSignature(), "h");
    EXPECT_EQ(m->annotations.at("org.gtk.GDBus.C.UnixFD"), "true");
    ASSERT_GE(m->inArgs.size(), 4u);
    EXPECT_EQ(m->inArgs[2].name, "filter");
    EXPECT_EQ(m->inArgs[2].annotations.at("org.gtk.GDBus.C.ForceGVariant"), "true");
    EXPECT_EQ(m->inArgs[3].annotations.at("org.gtk.GDBus.C.ForceGVariant"), "true");

    // Properties
    auto* ids = iface->findProperty("Ids");
    ASSERT_NE(ids, nullptr);
    EXPECT_EQ(ids->signature, "au");
    EXPECT_EQ(ids->emitsChangedSignal(), EmitsChangedSignal::Const);
    EXPECT_FALSE(ids->isDeprecated());

    auto* id = iface->findProperty("Id");
    ASSERT_NE(id, nullptr);
    EXPECT_TRUE(id->isDeprecated());
    EXPECT_EQ(id->emitsChangedSignal(), EmitsChangedSignal::Const);
    EXPECT_EQ(id->annotations.size(), 2u);
}