/// @file InterfaceBuilder.h
/// @brief Fluent builder API for defining D-Bus interfaces on ServiceObject.

#ifndef MBEDBUS_INTERFACEBUILDER_H
#define MBEDBUS_INTERFACEBUILDER_H

#include <dbus/dbus.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#include "Types.h"
#include "Error.h"
#include "Message.h"
#include "TypeTraits.h"
#include "Variant.h"

namespace mbedbus {

// Forward declarations
class ServiceObject;

/// @brief Metadata for a registered method.
struct MethodInfo {
    std::string name;
    std::string inputSig;
    std::string outputSig;
    std::function<Message(const Message&)> handler;
    /// Argument names for introspection (in then out).
    std::vector<std::pair<std::string, std::string>> inArgs;  // name, signature
    std::vector<std::pair<std::string, std::string>> outArgs; // name, signature
};

/// @brief Metadata for a registered property.
struct PropertyInfo {
    std::string name;
    std::string signature;
    std::function<Variant()> getter;                 // nullptr if write-only
    std::function<void(const Variant&)> setter;      // nullptr if read-only
    bool readable;
    bool writable;
};

/// @brief Metadata for a registered signal.
struct SignalInfo {
    std::string name;
    std::string signature;
    std::vector<std::pair<std::string, std::string>> args; // name, signature
};

/// @brief Holds all methods, properties, and signals for one D-Bus interface.
struct InterfaceData {
    std::string name;
    std::vector<MethodInfo> methods;
    std::vector<PropertyInfo> properties;
    std::vector<SignalInfo> signals;
};

// ============================================================
// Template helpers for wrapping user lambdas
// ============================================================

namespace detail {

// Extract function traits from lambdas/function objects
template<typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

template<typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
    using ReturnType = R;
    using ArgsTuple = std::tuple<typename std::decay<Args>::type...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)> : FunctionTraits<R(Args...)> {};

template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...) const> : FunctionTraits<R(Args...)> {};

template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...)> : FunctionTraits<R(Args...)> {};

// Call a function with arguments extracted from a tuple
template<typename F, typename Tuple, std::size_t... I>
auto applyImpl(F&& f, Tuple&& t, std::index_sequence<I...>)
    -> decltype(f(std::get<I>(std::forward<Tuple>(t))...))
{
    return f(std::get<I>(std::forward<Tuple>(t))...);
}

template<typename F, typename Tuple>
auto apply(F&& f, Tuple&& t)
    -> decltype(applyImpl(std::forward<F>(f), std::forward<Tuple>(t),
                          std::make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>{}))
{
    return applyImpl(std::forward<F>(f), std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>{});
}

// Build input signatures from Args tuple
template<typename Tuple, std::size_t... I>
std::vector<std::pair<std::string, std::string>> buildArgInfoImpl(std::index_sequence<I...>) {
    std::vector<std::pair<std::string, std::string>> result;
    // We use generic arg names
    using dummy = int[];
    (void)dummy{0, (result.emplace_back(std::make_pair("arg" + std::to_string(I),
        types::Traits<typename std::tuple_element<I, Tuple>::type>::signature())), 0)...};
    return result;
}

template<typename... Args>
std::vector<std::pair<std::string, std::string>> buildArgInfo() {
    return buildArgInfoImpl<std::tuple<Args...>>(std::index_sequence_for<Args...>{});
}

// Wrap a user function into std::function<Message(const Message&)>
// For non-void return types
template<typename F, typename R, typename ArgsTuple>
struct MethodWrapper;

template<typename F, typename R, typename... Args>
struct MethodWrapper<F, R, std::tuple<Args...>> {
    static std::function<Message(const Message&)> wrap(F fn) {
        return [fn](const Message& call) -> Message {
            DBusMessageIter iter;
            dbus_message_iter_init(call.raw(), &iter);
            auto args = types::deserializeArgs<Args...>(iter);
            R result = detail::apply(fn, std::move(args));
            Message reply = Message::createMethodReturn(call);
            reply.appendArgs(result);
            return reply;
        };
    }
};

// Specialization for void return
template<typename F, typename... Args>
struct MethodWrapper<F, void, std::tuple<Args...>> {
    static std::function<Message(const Message&)> wrap(F fn) {
        return [fn](const Message& call) -> Message {
            DBusMessageIter iter;
            dbus_message_iter_init(call.raw(), &iter);
            auto args = types::deserializeArgs<Args...>(iter);
            detail::apply(fn, std::move(args));
            return Message::createMethodReturn(call);
        };
    }
};

// Specialization for no args, non-void return
template<typename F, typename R>
struct MethodWrapper<F, R, std::tuple<>> {
    static std::function<Message(const Message&)> wrap(F fn) {
        return [fn](const Message& call) -> Message {
            R result = fn();
            Message reply = Message::createMethodReturn(call);
            reply.appendArgs(result);
            return reply;
        };
    }
};

// Specialization for no args, void return
template<typename F>
struct MethodWrapper<F, void, std::tuple<>> {
    static std::function<Message(const Message&)> wrap(F fn) {
        return [fn](const Message& call) -> Message {
            fn();
            return Message::createMethodReturn(call);
        };
    }
};

} // namespace detail

/// @brief Fluent builder for adding methods, properties, and signals to an interface.
class InterfaceBuilder {
public:
    /// @brief Constructor. Used internally by ServiceObject.
    InterfaceBuilder(InterfaceData& data, ServiceObject* owner)
        : data_(data), owner_(owner) {}

    /// @brief Add a method with automatic signature deduction.
    template<typename F>
    InterfaceBuilder& addMethod(const std::string& name, F&& handler) {
        using Traits = detail::FunctionTraits<typename std::decay<F>::type>;
        using R = typename Traits::ReturnType;
        using ArgsTuple = typename Traits::ArgsTuple;

        MethodInfo mi;
        mi.name = name;
        mi.inputSig = buildInputSig<ArgsTuple>(
            std::make_index_sequence<std::tuple_size<ArgsTuple>::value>{});
        mi.outputSig = buildOutputSig<R>();
        mi.handler = detail::MethodWrapper<
            typename std::decay<F>::type, R, ArgsTuple>::wrap(std::forward<F>(handler));
        mi.inArgs = buildInArgs<ArgsTuple>(
            std::make_index_sequence<std::tuple_size<ArgsTuple>::value>{});
        mi.outArgs = buildOutArgs<R>();
        data_.methods.push_back(std::move(mi));
        return *this;
    }

    /// @brief Add a read-only property.
    template<typename Getter>
    InterfaceBuilder& addProperty(const std::string& name, Getter&& getter) {
        using R = typename detail::FunctionTraits<typename std::decay<Getter>::type>::ReturnType;
        PropertyInfo pi;
        pi.name = name;
        pi.signature = types::Traits<R>::signature();
        pi.readable = true;
        pi.writable = false;
        auto g = std::forward<Getter>(getter);
        pi.getter = [g]() -> Variant { return Variant(g()); };
        data_.properties.push_back(std::move(pi));
        return *this;
    }

    /// @brief Add a read-write property.
    template<typename Getter, typename Setter>
    InterfaceBuilder& addProperty(const std::string& name, Getter&& getter, Setter&& setter) {
        using R = typename detail::FunctionTraits<typename std::decay<Getter>::type>::ReturnType;
        PropertyInfo pi;
        pi.name = name;
        pi.signature = types::Traits<R>::signature();
        pi.readable = true;
        pi.writable = true;
        auto g = std::forward<Getter>(getter);
        auto s = std::forward<Setter>(setter);
        pi.getter = [g]() -> Variant { return Variant(g()); };
        pi.setter = [s](const Variant& v) {
            s(v.get<R>());
        };
        data_.properties.push_back(std::move(pi));
        return *this;
    }

    /// @brief Add a signal declaration.
    template<typename... Args>
    InterfaceBuilder& addSignal(const std::string& name) {
        SignalInfo si;
        si.name = name;
        si.signature = types::buildSignature<Args...>();
        si.args = detail::buildArgInfo<Args...>();
        data_.signals.push_back(std::move(si));
        return *this;
    }

private:
    template<typename Tuple, std::size_t... I>
    static std::string buildInputSig(std::index_sequence<I...>) {
        std::string sig;
        using dummy = int[];
        (void)dummy{0, (sig += types::Traits<
            typename std::tuple_element<I, Tuple>::type>::signature(), 0)...};
        return sig;
    }

    template<typename R>
    static typename std::enable_if<!std::is_void<R>::value, std::string>::type
    buildOutputSig() { return types::Traits<R>::signature(); }

    template<typename R>
    static typename std::enable_if<std::is_void<R>::value, std::string>::type
    buildOutputSig() { return ""; }

    template<typename Tuple, std::size_t... I>
    static std::vector<std::pair<std::string, std::string>>
    buildInArgs(std::index_sequence<I...>) {
        std::vector<std::pair<std::string, std::string>> result;
        using dummy = int[];
        (void)dummy{0, (result.emplace_back(std::make_pair("arg" + std::to_string(I),
            types::Traits<typename std::tuple_element<I, Tuple>::type>::signature())), 0)...};
        return result;
    }

    template<typename R>
    static typename std::enable_if<!std::is_void<R>::value,
        std::vector<std::pair<std::string, std::string>>>::type
    buildOutArgs() {
        return {{"result", types::Traits<R>::signature()}};
    }

    template<typename R>
    static typename std::enable_if<std::is_void<R>::value,
        std::vector<std::pair<std::string, std::string>>>::type
    buildOutArgs() { return {}; }

    InterfaceData& data_;
    ServiceObject* owner_;
};

} // namespace mbedbus

#endif // MBEDBUS_INTERFACEBUILDER_H
