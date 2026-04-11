#include "mbedbus/Variant.h"

namespace mbedbus {

void Variant::serializeIntoVariant(DBusMessageIter& iter) const {
    if (!holder_) {
        throw Error("com.mbedbus.Error.EmptyVariant", "Cannot serialize empty variant");
    }
    DBusMessageIter sub;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
                                     signature_.c_str(), &sub);
    holder_->serialize(sub);
    dbus_message_iter_close_container(&iter, &sub);
}

Variant Variant::deserializeFromVariant(DBusMessageIter& iter) {
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
        throw Error("org.freedesktop.DBus.Error.InvalidArgs",
                     "Expected variant type");
    }

    DBusMessageIter sub;
    dbus_message_iter_recurse(&iter, &sub);

    int argType = dbus_message_iter_get_arg_type(&sub);

    switch (argType) {
        case DBUS_TYPE_BOOLEAN: {
            dbus_bool_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(static_cast<bool>(v != FALSE));
        }
        case DBUS_TYPE_BYTE: {
            uint8_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_INT16: {
            int16_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_UINT16: {
            uint16_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_INT32: {
            int32_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_UINT32: {
            uint32_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_INT64: {
            int64_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_UINT64: {
            uint64_t v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_DOUBLE: {
            double v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(v);
        }
        case DBUS_TYPE_STRING: {
            const char* v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(std::string(v ? v : ""));
        }
        case DBUS_TYPE_OBJECT_PATH: {
            const char* v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(ObjectPath(v ? v : ""));
        }
        case DBUS_TYPE_SIGNATURE: {
            const char* v;
            dbus_message_iter_get_basic(&sub, &v);
            return Variant(Signature(v ? v : ""));
        }
        case DBUS_TYPE_ARRAY: {
            // Check if it's a dict (a{...}) or plain array
            int elemType = dbus_message_iter_get_element_type(&sub);
            if (elemType == DBUS_TYPE_DICT_ENTRY) {
                // Deserialize as map<string, Variant> (most common dict type)
                auto m = types::Traits<std::map<std::string, Variant>>::deserialize(sub);
                return Variant(std::move(m));
            } else if (elemType == DBUS_TYPE_STRING) {
                auto v = types::Traits<std::vector<std::string>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_BYTE) {
                auto v = types::Traits<std::vector<uint8_t>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_INT32) {
                auto v = types::Traits<std::vector<int32_t>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_UINT32) {
                auto v = types::Traits<std::vector<uint32_t>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_DOUBLE) {
                auto v = types::Traits<std::vector<double>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_VARIANT) {
                auto v = types::Traits<std::vector<Variant>>::deserialize(sub);
                return Variant(std::move(v));
            } else if (elemType == DBUS_TYPE_OBJECT_PATH) {
                auto v = types::Traits<std::vector<ObjectPath>>::deserialize(sub);
                return Variant(std::move(v));
            } else {
                // Fallback: try as vector of variants
                // Skip with empty variant
                return Variant(std::string("<unsupported array element type>"));
            }
        }
        case DBUS_TYPE_VARIANT: {
            // Nested variant
            return deserializeFromVariant(sub);
        }
        case DBUS_TYPE_UNIX_FD: {
            int fd;
            dbus_message_iter_get_basic(&sub, &fd);
            return Variant(UnixFd(fd));
        }
        default:
            return Variant(std::string("<unsupported type>"));
    }
}

} // namespace mbedbus
