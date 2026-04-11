/// @file TypeTraits.h
/// @brief Compile-time mapping of C++ types to D-Bus signatures, plus serialize/deserialize.

#ifndef MBEDBUS_TYPETRAITS_H
#define MBEDBUS_TYPETRAITS_H

#include <dbus/dbus.h>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <type_traits>

#include "Types.h"
#include "Error.h"

// Forward-declare Variant so we can specialize Traits for it
namespace mbedbus { class Variant; }

namespace mbedbus {
namespace types {

// ============================================================
// Primary Traits template (unspecialized = compile error)
// ============================================================

/// @brief Primary template. Specialize for each supported D-Bus type.
template<typename T, typename Enable = void>
struct Traits;

// ============================================================
// Basic types
// ============================================================

template<>
struct Traits<bool> {
    static constexpr int dbusType = DBUS_TYPE_BOOLEAN;
    static std::string signature() { return "b"; }
    static void serialize(DBusMessageIter& iter, bool v) {
        dbus_bool_t bv = v ? TRUE : FALSE;
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &bv);
    }
    static bool deserialize(DBusMessageIter& iter) {
        dbus_bool_t v;
        dbus_message_iter_get_basic(&iter, &v);
        return v != FALSE;
    }
};

template<>
struct Traits<uint8_t> {
    static constexpr int dbusType = DBUS_TYPE_BYTE;
    static std::string signature() { return "y"; }
    static void serialize(DBusMessageIter& iter, uint8_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_BYTE, &v);
    }
    static uint8_t deserialize(DBusMessageIter& iter) {
        uint8_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<int16_t> {
    static constexpr int dbusType = DBUS_TYPE_INT16;
    static std::string signature() { return "n"; }
    static void serialize(DBusMessageIter& iter, int16_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT16, &v);
    }
    static int16_t deserialize(DBusMessageIter& iter) {
        int16_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<uint16_t> {
    static constexpr int dbusType = DBUS_TYPE_UINT16;
    static std::string signature() { return "q"; }
    static void serialize(DBusMessageIter& iter, uint16_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &v);
    }
    static uint16_t deserialize(DBusMessageIter& iter) {
        uint16_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<int32_t> {
    static constexpr int dbusType = DBUS_TYPE_INT32;
    static std::string signature() { return "i"; }
    static void serialize(DBusMessageIter& iter, int32_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &v);
    }
    static int32_t deserialize(DBusMessageIter& iter) {
        int32_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<uint32_t> {
    static constexpr int dbusType = DBUS_TYPE_UINT32;
    static std::string signature() { return "u"; }
    static void serialize(DBusMessageIter& iter, uint32_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &v);
    }
    static uint32_t deserialize(DBusMessageIter& iter) {
        uint32_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<int64_t> {
    static constexpr int dbusType = DBUS_TYPE_INT64;
    static std::string signature() { return "x"; }
    static void serialize(DBusMessageIter& iter, int64_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &v);
    }
    static int64_t deserialize(DBusMessageIter& iter) {
        int64_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<uint64_t> {
    static constexpr int dbusType = DBUS_TYPE_UINT64;
    static std::string signature() { return "t"; }
    static void serialize(DBusMessageIter& iter, uint64_t v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &v);
    }
    static uint64_t deserialize(DBusMessageIter& iter) {
        uint64_t v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<double> {
    static constexpr int dbusType = DBUS_TYPE_DOUBLE;
    static std::string signature() { return "d"; }
    static void serialize(DBusMessageIter& iter, double v) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_DOUBLE, &v);
    }
    static double deserialize(DBusMessageIter& iter) {
        double v; dbus_message_iter_get_basic(&iter, &v); return v;
    }
};

template<>
struct Traits<std::string> {
    static constexpr int dbusType = DBUS_TYPE_STRING;
    static std::string signature() { return "s"; }
    static void serialize(DBusMessageIter& iter, const std::string& v) {
        const char* s = v.c_str();
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &s);
    }
    static std::string deserialize(DBusMessageIter& iter) {
        const char* s;
        dbus_message_iter_get_basic(&iter, &s);
        return std::string(s ? s : "");
    }
};

template<>
struct Traits<ObjectPath> {
    static constexpr int dbusType = DBUS_TYPE_OBJECT_PATH;
    static std::string signature() { return "o"; }
    static void serialize(DBusMessageIter& iter, const ObjectPath& v) {
        const char* s = v.c_str();
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &s);
    }
    static ObjectPath deserialize(DBusMessageIter& iter) {
        const char* s;
        dbus_message_iter_get_basic(&iter, &s);
        return ObjectPath(s ? s : "");
    }
};

template<>
struct Traits<Signature> {
    static constexpr int dbusType = DBUS_TYPE_SIGNATURE;
    static std::string signature() { return "g"; }
    static void serialize(DBusMessageIter& iter, const Signature& v) {
        const char* s = v.c_str();
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_SIGNATURE, &s);
    }
    static Signature deserialize(DBusMessageIter& iter) {
        const char* s;
        dbus_message_iter_get_basic(&iter, &s);
        return Signature(s ? s : "");
    }
};

template<>
struct Traits<UnixFd> {
    static constexpr int dbusType = DBUS_TYPE_UNIX_FD;
    static std::string signature() { return "h"; }
    static void serialize(DBusMessageIter& iter, const UnixFd& v) {
        int fd = v.get();
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_UNIX_FD, &fd);
    }
    static UnixFd deserialize(DBusMessageIter& iter) {
        int fd;
        dbus_message_iter_get_basic(&iter, &fd);
        return UnixFd(fd);
    }
};

// ============================================================
// Container types
// ============================================================

/// @brief Traits for std::vector<T> → D-Bus array "a" + signature(T).
template<typename T>
struct Traits<std::vector<T>> {
    static std::string signature() { return "a" + Traits<T>::signature(); }
    static void serialize(DBusMessageIter& iter, const std::vector<T>& v) {
        DBusMessageIter sub;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
            Traits<T>::signature().c_str(), &sub);
        for (auto& elem : v) {
            Traits<T>::serialize(sub, elem);
        }
        dbus_message_iter_close_container(&iter, &sub);
    }
    static std::vector<T> deserialize(DBusMessageIter& iter) {
        std::vector<T> result;
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
            throw Error("org.freedesktop.DBus.Error.InvalidArgs", "Expected array");
        }
        DBusMessageIter sub;
        dbus_message_iter_recurse(&iter, &sub);
        while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
            result.push_back(Traits<T>::deserialize(sub));
            dbus_message_iter_next(&sub);
        }
        return result;
    }
};

/// @brief Traits for std::map<K,V> → D-Bus dict "a{KV}".
template<typename K, typename V>
struct Traits<std::map<K, V>> {
    static std::string signature() {
        return "a{" + Traits<K>::signature() + Traits<V>::signature() + "}";
    }
    static void serialize(DBusMessageIter& iter, const std::map<K, V>& m) {
        std::string entrySig = "{" + Traits<K>::signature() + Traits<V>::signature() + "}";
        DBusMessageIter sub;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
            entrySig.c_str(), &sub);
        for (auto& kv : m) {
            DBusMessageIter entry;
            dbus_message_iter_open_container(&sub, DBUS_TYPE_DICT_ENTRY,
                nullptr, &entry);
            Traits<K>::serialize(entry, kv.first);
            Traits<V>::serialize(entry, kv.second);
            dbus_message_iter_close_container(&sub, &entry);
        }
        dbus_message_iter_close_container(&iter, &sub);
    }
    static std::map<K, V> deserialize(DBusMessageIter& iter) {
        std::map<K, V> result;
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
            throw Error("org.freedesktop.DBus.Error.InvalidArgs", "Expected array (dict)");
        }
        DBusMessageIter sub;
        dbus_message_iter_recurse(&iter, &sub);
        while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&sub, &entry);
            K key = Traits<K>::deserialize(entry);
            dbus_message_iter_next(&entry);
            V val = Traits<V>::deserialize(entry);
            result.emplace(std::move(key), std::move(val));
            dbus_message_iter_next(&sub);
        }
        return result;
    }
};

// ============================================================
// Tuple support
// ============================================================

namespace detail {

template<std::size_t I, std::size_t N>
struct TupleSerializer {
    template<typename... Ts>
    static void serialize(DBusMessageIter& iter, const std::tuple<Ts...>& t) {
        using ElemType = typename std::tuple_element<I, std::tuple<Ts...>>::type;
        Traits<ElemType>::serialize(iter, std::get<I>(t));
        TupleSerializer<I + 1, N>::serialize(iter, t);
    }
    template<typename... Ts>
    static void deserialize(DBusMessageIter& iter, std::tuple<Ts...>& t) {
        using ElemType = typename std::tuple_element<I, std::tuple<Ts...>>::type;
        std::get<I>(t) = Traits<ElemType>::deserialize(iter);
        dbus_message_iter_next(&iter);
        TupleSerializer<I + 1, N>::deserialize(iter, t);
    }
    template<typename... Ts>
    static void appendSignature(std::string& s) {
        using ElemType = typename std::tuple_element<I, std::tuple<Ts...>>::type;
        s += Traits<ElemType>::signature();
        TupleSerializer<I + 1, N>::template appendSignature<Ts...>(s);
    }
};

template<std::size_t N>
struct TupleSerializer<N, N> {
    template<typename... Ts>
    static void serialize(DBusMessageIter&, const std::tuple<Ts...>&) {}
    template<typename... Ts>
    static void deserialize(DBusMessageIter&, std::tuple<Ts...>&) {}
    template<typename... Ts>
    static void appendSignature(std::string&) {}
};

} // namespace detail

/// @brief Traits for std::tuple<Ts...> → D-Bus struct "(...)".
template<typename... Ts>
struct Traits<std::tuple<Ts...>> {
    static std::string signature() {
        std::string s = "(";
        detail::TupleSerializer<0, sizeof...(Ts)>::template appendSignature<Ts...>(s);
        s += ")";
        return s;
    }
    static void serialize(DBusMessageIter& iter, const std::tuple<Ts...>& t) {
        DBusMessageIter sub;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &sub);
        detail::TupleSerializer<0, sizeof...(Ts)>::serialize(sub, t);
        dbus_message_iter_close_container(&iter, &sub);
    }
    static std::tuple<Ts...> deserialize(DBusMessageIter& iter) {
        std::tuple<Ts...> t;
        DBusMessageIter sub;
        dbus_message_iter_recurse(&iter, &sub);
        detail::TupleSerializer<0, sizeof...(Ts)>::deserialize(sub, t);
        return t;
    }
};

// Variant traits are declared here but implemented after Variant is defined.
// See Variant.h for the Traits<Variant> specialization.

// ============================================================
// Helpers
// ============================================================

namespace detail {

template<typename... Args>
struct AppendSigs;

template<>
struct AppendSigs<> {
    static void apply(std::string&) {}
};

template<typename T, typename... Rest>
struct AppendSigs<T, Rest...> {
    static void apply(std::string& s) {
        s += Traits<T>::signature();
        AppendSigs<Rest...>::apply(s);
    }
};

} // namespace detail

/// @brief Build the concatenated D-Bus signature for a list of types.
template<typename... Args>
std::string buildSignature() {
    std::string s;
    detail::AppendSigs<Args...>::apply(s);
    return s;
}

// ============================================================
// Serialize / deserialize multiple args
// ============================================================

namespace detail {

inline void serializeAll(DBusMessageIter&) {}

template<typename T, typename... Rest>
void serializeAll(DBusMessageIter& iter, const T& v, const Rest&... rest) {
    Traits<T>::serialize(iter, v);
    serializeAll(iter, rest...);
}

template<std::size_t I, typename Tuple>
typename std::enable_if<I == std::tuple_size<Tuple>::value>::type
deserializeArgs(DBusMessageIter&, Tuple&) {}

template<std::size_t I, typename Tuple>
typename std::enable_if<I < std::tuple_size<Tuple>::value>::type
deserializeArgs(DBusMessageIter& iter, Tuple& t) {
    using ElemType = typename std::tuple_element<I, Tuple>::type;
    std::get<I>(t) = Traits<ElemType>::deserialize(iter);
    dbus_message_iter_next(&iter);
    deserializeArgs<I + 1>(iter, t);
}

} // namespace detail

/// @brief Serialize multiple values into a message iterator.
template<typename... Args>
void serializeArgs(DBusMessageIter& iter, const Args&... args) {
    detail::serializeAll(iter, args...);
}

/// @brief Deserialize multiple arguments from a message iterator into a tuple.
template<typename... Args>
std::tuple<Args...> deserializeArgs(DBusMessageIter& iter) {
    std::tuple<Args...> t;
    detail::deserializeArgs<0>(iter, t);
    return t;
}

} // namespace types
} // namespace mbedbus

#endif // MBEDBUS_TYPETRAITS_H
