#include "mbedbus/ServiceObject.h"
#include <sstream>

namespace mbedbus {

/// Generate the standard D-Bus introspection XML for a ServiceObject.
std::string ServiceObject::introspect() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream xml;
    xml << "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
        << " \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n";
    xml << "<node name=\"" << path_ << "\">\n";

    // Standard interfaces always present
    // org.freedesktop.DBus.Introspectable
    xml << "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
        << "    <method name=\"Introspect\">\n"
        << "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
        << "    </method>\n"
        << "  </interface>\n";

    // org.freedesktop.DBus.Peer
    xml << "  <interface name=\"org.freedesktop.DBus.Peer\">\n"
        << "    <method name=\"Ping\"/>\n"
        << "    <method name=\"GetMachineId\">\n"
        << "      <arg name=\"machine_uuid\" type=\"s\" direction=\"out\"/>\n"
        << "    </method>\n"
        << "  </interface>\n";

    // org.freedesktop.DBus.Properties (if any interface has properties)
    bool hasProps = false;
    for (auto& kv : interfaces_) {
        if (!kv.second.properties.empty()) { hasProps = true; break; }
    }
    if (hasProps) {
        xml << "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
            << "    <method name=\"Get\">\n"
            << "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
            << "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
            << "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
            << "    </method>\n"
            << "    <method name=\"Set\">\n"
            << "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
            << "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
            << "      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
            << "    </method>\n"
            << "    <method name=\"GetAll\">\n"
            << "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
            << "      <arg name=\"all_properties\" type=\"a{sv}\" direction=\"out\"/>\n"
            << "    </method>\n"
            << "    <signal name=\"PropertiesChanged\">\n"
            << "      <arg name=\"interface_name\" type=\"s\"/>\n"
            << "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
            << "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
            << "    </signal>\n"
            << "  </interface>\n";
    }

    // User-defined interfaces
    for (auto& kv : interfaces_) {
        auto& iface = kv.second;
        xml << "  <interface name=\"" << iface.name << "\">\n";

        for (auto& method : iface.methods) {
            xml << "    <method name=\"" << method.name << "\">\n";
            for (auto& arg : method.inArgs) {
                xml << "      <arg name=\"" << arg.first
                    << "\" type=\"" << arg.second << "\" direction=\"in\"/>\n";
            }
            for (auto& arg : method.outArgs) {
                xml << "      <arg name=\"" << arg.first
                    << "\" type=\"" << arg.second << "\" direction=\"out\"/>\n";
            }
            xml << "    </method>\n";
        }

        for (auto& prop : iface.properties) {
            std::string access;
            if (prop.readable && prop.writable) access = "readwrite";
            else if (prop.readable) access = "read";
            else access = "write";
            xml << "    <property name=\"" << prop.name
                << "\" type=\"" << prop.signature
                << "\" access=\"" << access << "\"/>\n";
        }

        for (auto& sig : iface.signals) {
            xml << "    <signal name=\"" << sig.name << "\">\n";
            for (auto& arg : sig.args) {
                xml << "      <arg name=\"" << arg.first
                    << "\" type=\"" << arg.second << "\"/>\n";
            }
            xml << "    </signal>\n";
        }

        xml << "  </interface>\n";
    }

    // Child nodes
    for (auto& child : childNodes_) {
        xml << "  <node name=\"" << child << "\"/>\n";
    }

    xml << "</node>\n";
    return xml.str();
}

} // namespace mbedbus
