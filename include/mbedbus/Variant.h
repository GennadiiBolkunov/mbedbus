/// @file Variant.h
/// @brief Type-erased D-Bus variant container.

#ifndef MBEDBUS_VARIANT_H
#define MBEDBUS_VARIANT_H

#include <dbus/dbus.h>
#include <string>
#include <memory>
#include <typeindex>
#include <stdexcept>

#include "Types.h"
#include "Error.h"
#include "TypeTraits.h"

namespace mbedbus {

/// @brief Type-erased container for any D-Bus-compatible value.
class Variant {
public:
    Variant() = default;

    /// @brief Construct a Variant holding a value of type T.
    template<typename T>
    explicit Variant(const T& value)
        : holder_(std::make_shared<Holder<T>>(value))
        , signature_(types::Traits<T>::signature())
    {}

    /// @brief Check if variant holds a value.
    bool empty() const { return !holder_; }

    /// @brief Get the D-Bus signature of the contained type.
    const std::string& signature() const { return signature_; }

    /// @brief Extract value of type T. Throws Error on type mismatch.
    template<typename T>
    const T& get() const {
        if (!holder_) {
            throw Error("com.mbedbus.Error.EmptyVariant", "Variant is empty");
        }
        auto* p = dynamic_cast<const Holder<T>*>(holder_.get());
        if (!p) {
            throw Error("com.mbedbus.Error.TypeMismatch",
                "Variant type mismatch: expected " + types::Traits<T>::signature()
                + " got " + signature_);
        }
        return p->value;
    }

    /// @brief Try to extract value. Returns nullptr on mismatch.
    template<typename T>
    const T* tryGet() const {
        if (!holder_) return nullptr;
        auto* p = dynamic_cast<const Holder<T>*>(holder_.get());
        return p ? &p->value : nullptr;
    }

    /// @brief Check if the variant holds a specific type.
    template<typename T>
    bool is() const {
        return holder_ && dynamic_cast<const Holder<T>*>(holder_.get()) != nullptr;
    }

    /// @brief Serialize this variant into a D-Bus message iterator (as variant container).
    void serializeIntoVariant(DBusMessageIter& iter) const;

    /// @brief Deserialize a variant from a D-Bus message iterator.
    static Variant deserializeFromVariant(DBusMessageIter& iter);

    /// @brief Serialize the inner value (without the variant wrapping).
    void serializeInner(DBusMessageIter& iter) const {
        if (holder_) holder_->serialize(iter);
    }

    bool operator==(const Variant& o) const {
        if (empty() && o.empty()) return true;
        if (empty() || o.empty()) return false;
        if (signature_ != o.signature_) return false;
        return holder_->equals(*o.holder_);
    }

    bool operator!=(const Variant& o) const { return !(*this == o); }

private:
    struct HolderBase {
        virtual ~HolderBase() = default;
        virtual void serialize(DBusMessageIter& iter) const = 0;
        virtual bool equals(const HolderBase& other) const = 0;
    };

    template<typename T>
    struct Holder : HolderBase {
        T value;
        explicit Holder(const T& v) : value(v) {}

        void serialize(DBusMessageIter& iter) const override {
            types::Traits<T>::serialize(iter, value);
        }

        bool equals(const HolderBase& other) const override {
            auto* o = dynamic_cast<const Holder<T>*>(&other);
            return o && o->value == value;
        }
    };

    std::shared_ptr<const HolderBase> holder_;
    std::string signature_;
};

// ============================================================
// Traits specialization for Variant
// ============================================================

namespace types {

template<>
struct Traits<Variant> {
    static constexpr int dbusType = DBUS_TYPE_VARIANT;
    static std::string signature() { return "v"; }
    static void serialize(DBusMessageIter& iter, const Variant& v) {
        v.serializeIntoVariant(iter);
    }
    static Variant deserialize(DBusMessageIter& iter) {
        return Variant::deserializeFromVariant(iter);
    }
};

} // namespace types
} // namespace mbedbus

#endif // MBEDBUS_VARIANT_H
