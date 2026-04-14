/// @file IntrospectionParser.h
/// @brief Parses D-Bus introspection XML into structured data with annotation support.

#ifndef MBEDBUS_INTROSPECTION_PARSER_H
#define MBEDBUS_INTROSPECTION_PARSER_H

#include <string>
#include <vector>
#include <map>

namespace mbedbus {

/// @brief Access mode for a D-Bus property.
    enum class PropertyAccess { Read, Write, ReadWrite };

/// @brief Well-known values for org.freedesktop.DBus.Property.EmitsChangedSignal.
    enum class EmitsChangedSignal { True, False, Invalidates, Const };

/// @brief Key-value annotations map. Key = annotation name, value = annotation value.
    using Annotations = std::map<std::string, std::string>;

/// @brief A single argument (of a method or signal) from introspection.
    struct IntrospectedArg {
        std::string name;
        std::string signature;
        Annotations annotations;
    };

/// @brief A method parsed from introspection XML.
    struct IntrospectedMethod {
        std::string name;
        std::vector<IntrospectedArg> inArgs;
        std::vector<IntrospectedArg> outArgs;
        Annotations annotations;

        std::string inputSignature() const {
            std::string s; for (auto& a : inArgs) s += a.signature; return s;
        }
        std::string outputSignature() const {
            std::string s; for (auto& a : outArgs) s += a.signature; return s;
        }

        /// @brief Check if method is marked deprecated.
        bool isDeprecated() const {
            auto it = annotations.find("org.freedesktop.DBus.Deprecated");
            return it != annotations.end() && it->second == "true";
        }

        /// @brief Check if method expects no reply (fire-and-forget).
        bool isNoReply() const {
            auto it = annotations.find("org.freedesktop.DBus.Method.NoReply");
            return it != annotations.end() && it->second == "true";
        }
    };

/// @brief A signal parsed from introspection XML.
    struct IntrospectedSignal {
        std::string name;
        std::vector<IntrospectedArg> args;
        Annotations annotations;

        std::string signature() const {
            std::string s; for (auto& a : args) s += a.signature; return s;
        }

        bool isDeprecated() const {
            auto it = annotations.find("org.freedesktop.DBus.Deprecated");
            return it != annotations.end() && it->second == "true";
        }
    };

/// @brief A property parsed from introspection XML.
    struct IntrospectedProperty {
        std::string name;
        std::string signature;
        PropertyAccess access;
        Annotations annotations;

        bool isDeprecated() const {
            auto it = annotations.find("org.freedesktop.DBus.Deprecated");
            return it != annotations.end() && it->second == "true";
        }

        /// @brief Get the EmitsChangedSignal behaviour for this property.
        EmitsChangedSignal emitsChangedSignal() const {
            auto it = annotations.find("org.freedesktop.DBus.Property.EmitsChangedSignal");
            if (it == annotations.end()) return EmitsChangedSignal::True;
            if (it->second == "false")       return EmitsChangedSignal::False;
            if (it->second == "invalidates") return EmitsChangedSignal::Invalidates;
            if (it->second == "const")       return EmitsChangedSignal::Const;
            return EmitsChangedSignal::True;
        }
    };

/// @brief A D-Bus interface parsed from introspection XML.
    struct IntrospectedInterface {
        std::string name;
        std::vector<IntrospectedMethod> methods;
        std::vector<IntrospectedSignal> signals;
        std::vector<IntrospectedProperty> properties;
        Annotations annotations;

        const IntrospectedMethod* findMethod(const std::string& methodName) const {
            for (auto& m : methods) if (m.name == methodName) return &m;
            return nullptr;
        }
        const IntrospectedProperty* findProperty(const std::string& propName) const {
            for (auto& p : properties) if (p.name == propName) return &p;
            return nullptr;
        }
        const IntrospectedSignal* findSignal(const std::string& sigName) const {
            for (auto& s : signals) if (s.name == sigName) return &s;
            return nullptr;
        }
    };

/// @brief A parsed introspection node (one object path).
    struct IntrospectedNode {
        std::string name;
        std::vector<IntrospectedInterface> interfaces;
        std::vector<std::string> childNodes;

        const IntrospectedInterface* findInterface(const std::string& ifaceName) const {
            for (auto& i : interfaces) if (i.name == ifaceName) return &i;
            return nullptr;
        }
        const IntrospectedMethod* findMethod(const std::string& ifaceName,
            const std::string& methodName) const {
            auto* iface = findInterface(ifaceName);
            return iface ? iface->findMethod(methodName) : nullptr;
        }
    };

/// @brief Parses D-Bus introspection XML into structured data.
    class IntrospectionParser {
    public:
        static IntrospectedNode parse(const std::string& xml);
    private:
        static std::string extractAttr(const std::string& tag, const std::string& attrName);
        static std::string extractTagName(const std::string& tag);
    };

} // namespace mbedbus

#endif // MBEDBUS_INTROSPECTION_PARSER_H
