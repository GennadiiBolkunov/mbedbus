/// @file Skeleton.h
/// @brief Skeleton: builds a ServiceObject from introspection XML, handlers bound by name.

#ifndef MBEDBUS_SKELETON_H
#define MBEDBUS_SKELETON_H

#include <string>
#include <map>
#include <memory>
#include <functional>

#include "Connection.h"
#include "ServiceObject.h"
#include "IntrospectionParser.h"
#include "TypeTraits.h"
#include "Variant.h"
#include "Error.h"

namespace mbedbus {

/// @brief Strictness mode for Skeleton::finalize().
    enum class SkeletonMode {
        Lenient, ///< Unbound methods return NotImplemented at runtime.
        Strict   ///< Unbound methods/properties cause an exception at finalize().
    };

/// @brief Builds a ServiceObject from XML introspection, with handlers bound by name.
///
/// Usage:
/// @code
///     auto skel = Skeleton::fromXml(conn, xmlString);
///     skel->bindMethod<int32_t, int32_t, int32_t>("com.example.Calc", "Add",
///         [](int32_t a, int32_t b) -> int32_t { return a + b; });
///     skel->bindPropertyGetter<std::string>("com.example.Calc", "Version",
///         []() -> std::string { return "1.0"; });
///     skel->finalize();
/// @endcode
    class Skeleton {
    public:
        /// @brief Create a skeleton from introspection XML.
        /// @param conn   D-Bus connection.
        /// @param xml    Introspection XML string (must contain exactly one <node>).
        /// @return Shared pointer to the Skeleton.
        static std::shared_ptr<Skeleton> fromXml(std::shared_ptr<Connection> conn,
            const std::string& xml);

        /// @brief Create a skeleton from a pre-parsed IntrospectedNode.
        static std::shared_ptr<Skeleton> fromNode(std::shared_ptr<Connection> conn,
            const IntrospectedNode& node);

        ~Skeleton() = default;

        /// @brief Get the parsed node (for inspection/debugging).
        const IntrospectedNode& node() const { return node_; }

        /// @brief Get the underlying ServiceObject (available after finalize).
        std::shared_ptr<ServiceObject> serviceObject() const { return obj_; }

        // ================================================================
        // Typed method binding
        // ================================================================

        /// @brief Bind a typed handler to a method. R = return type, Args = input types.
        /// Skeleton verifies that the D-Bus signature matches the XML declaration.
        template<typename R, typename... Args, typename F>
        void bindMethod(const std::string& iface, const std::string& method, F&& handler) {
            validateMethodSignature<R, Args...>(iface, method);
            using detail::MethodWrapper;
            using detail::FunctionTraits;
            auto wrapped = MethodWrapper<
                typename std::decay<F>::type, R, std::tuple<Args...>>::wrap(
                std::forward<F>(handler));
            setMethodHandler(iface, method, std::move(wrapped));
        }

        /// @brief Bind a void method (no return value).
        template<typename... Args, typename F>
        void bindVoidMethod(const std::string& iface, const std::string& method, F&& handler) {
            bindMethod<void, Args...>(iface, method, std::forward<F>(handler));
        }

        // ================================================================
        // Raw method binding (no signature check)
        // ================================================================

        /// @brief Bind a raw handler (receives/returns Message directly).
        void bindRawMethod(const std::string& iface, const std::string& method,
            std::function<Message(const Message&)> handler);

        // ================================================================
        // Property binding
        // ================================================================

        /// @brief Bind a typed getter for a read/readwrite property.
        template<typename T, typename F>
        void bindPropertyGetter(const std::string& iface, const std::string& prop, F&& getter) {
            validatePropertyType<T>(iface, prop);
            auto g = std::forward<F>(getter);
            setPropertyGetter(iface, prop, [g]() -> Variant { return Variant(g()); });
        }

        /// @brief Bind a typed setter for a readwrite/write property.
        template<typename T, typename F>
        void bindPropertySetter(const std::string& iface, const std::string& prop, F&& setter) {
            validatePropertyType<T>(iface, prop);
            auto s = std::forward<F>(setter);
            setPropertySetter(iface, prop, [s](const Variant& v) { s(v.get<T>()); });
        }

        // ================================================================
        // Signals
        // ================================================================

        /// @brief Emit a signal declared in the XML.
        template<typename... Args>
        void emitSignal(const std::string& iface, const std::string& signalName,
            const Args&... args) {
            if (!obj_) throw Error("com.mbedbus.Error", "Skeleton not finalized");
            obj_->emitSignal(iface, signalName, args...);
        }

        // ================================================================
        // Finalize
        // ================================================================

        /// @brief Register the object on the bus.
        /// @param mode Lenient (default) or Strict.
        ///   In Strict mode, throws if any method or readable property lacks a handler.
        void finalize(SkeletonMode mode = SkeletonMode::Lenient);

        /// @brief Get the object path.
        const std::string& path() const { return node_.name; }

    private:
        Skeleton(std::shared_ptr<Connection> conn, IntrospectedNode node);

        void setMethodHandler(const std::string& iface, const std::string& method,
            std::function<Message(const Message&)> handler);
        void setPropertyGetter(const std::string& iface, const std::string& prop,
            std::function<Variant()> getter);
        void setPropertySetter(const std::string& iface, const std::string& prop,
            std::function<void(const Variant&)> setter);

        template<typename R>
        static typename std::enable_if<!std::is_void<R>::value, std::string>::type
        outputSig() { return types::Traits<R>::signature(); }

        template<typename R>
        static typename std::enable_if<std::is_void<R>::value, std::string>::type
        outputSig() { return ""; }

        template<typename R, typename... Args>
        void validateMethodSignature(const std::string& iface, const std::string& method) {
            auto* m = node_.findMethod(iface, method);
            if (!m) throw Error("com.mbedbus.Error",
                    "Method " + iface + "." + method + " not found in XML");
            std::string expectedIn = m->inputSignature();
            std::string expectedOut = m->outputSignature();
            std::string actualIn = types::buildSignature<Args...>();
            std::string actualOut = outputSig<R>();
            if (actualIn != expectedIn)
                throw Error("com.mbedbus.Error",
                    "Signature mismatch for " + iface + "." + method +
                    " input: XML says '" + expectedIn + "', handler has '" + actualIn + "'");
            if (actualOut != expectedOut)
                throw Error("com.mbedbus.Error",
                    "Signature mismatch for " + iface + "." + method +
                    " output: XML says '" + expectedOut + "', handler has '" + actualOut + "'");
        }

        template<typename T>
        void validatePropertyType(const std::string& iface, const std::string& prop) {
            auto* ifacePtr = node_.findInterface(iface);
            if (!ifacePtr) throw Error("com.mbedbus.Error",
                    "Interface " + iface + " not found in XML");
            auto* p = ifacePtr->findProperty(prop);
            if (!p) throw Error("com.mbedbus.Error",
                    "Property " + iface + "." + prop + " not found in XML");
            std::string actual = types::Traits<T>::signature();
            if (actual != p->signature)
                throw Error("com.mbedbus.Error",
                    "Type mismatch for " + iface + "." + prop +
                    ": XML says '" + p->signature + "', handler has '" + actual + "'");
        }

        std::shared_ptr<Connection> conn_;
        IntrospectedNode node_;
        std::shared_ptr<ServiceObject> obj_;

        // Storage for handlers before finalize()
        struct MethodBinding {
            std::function<Message(const Message&)> handler;
        };
        struct PropertyBinding {
            std::function<Variant()> getter;
            std::function<void(const Variant&)> setter;
        };
        // iface -> method -> handler
        std::map<std::string, std::map<std::string, MethodBinding>> methodBindings_;
        // iface -> property -> getter/setter
        std::map<std::string, std::map<std::string, PropertyBinding>> propertyBindings_;
    };

} // namespace mbedbus

#endif // MBEDBUS_SKELETON_H
