/// @file Error.h
/// @brief D-Bus error representation and Expected<T> result type.

#ifndef MBEDBUS_ERROR_H
#define MBEDBUS_ERROR_H

#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace mbedbus {

/// @brief Represents a D-Bus error with name and message.
class Error : public std::runtime_error {
public:
    /// @brief Construct an error with name and message.
    Error(const std::string& name, const std::string& message)
        : std::runtime_error(message), name_(name), message_(message) {}

    /// @brief Construct an error with message only (generic error name).
    explicit Error(const std::string& message)
        : std::runtime_error(message),
          name_("com.mbedbus.Error"),
          message_(message) {}

    /// @brief Get the D-Bus error name (e.g. "org.freedesktop.DBus.Error.Failed").
    const std::string& name() const { return name_; }

    /// @brief Get the error message.
    const std::string& message() const { return message_; }

private:
    std::string name_;
    std::string message_;
};

/// @brief Result type: holds either a value of type T or an Error. C++14 analogue of std::expected.
template<typename T>
class Expected {
public:
    /// @brief Construct with a value.
    Expected(const T& value) : hasValue_(true), storage_{}, errStorage_{} {
        new (&storage_) T(value);
    }

    /// @brief Construct with a moved value.
    Expected(T&& value) : hasValue_(true), storage_{}, errStorage_{} {
        new (&storage_) T(std::move(value));
    }

    /// @brief Construct with an error.
    Expected(const Error& err) : hasValue_(false), storage_{}, errStorage_{} {
        new (&errStorage_) Error(err);
    }

    /// @brief Construct with a moved error.
    Expected(Error&& err) : hasValue_(false), storage_{}, errStorage_{} {
        new (&errStorage_) Error(std::move(err));
    }

    Expected(const Expected& o) : hasValue_(o.hasValue_), storage_{}, errStorage_{} {
        if (hasValue_) new (&storage_) T(o.valueRef());
        else new (&errStorage_) Error(o.errorRef());
    }

    Expected(Expected&& o) noexcept : hasValue_(o.hasValue_), storage_{}, errStorage_{} {
        if (hasValue_) new (&storage_) T(std::move(o.valueRef()));
        else new (&errStorage_) Error(std::move(o.errorRef()));
    }

    Expected& operator=(const Expected& o) {
        if (this != &o) {
            destroy();
            hasValue_ = o.hasValue_;
            if (hasValue_) new (&storage_) T(o.valueRef());
            else new (&errStorage_) Error(o.errorRef());
        }
        return *this;
    }

    Expected& operator=(Expected&& o) noexcept {
        if (this != &o) {
            destroy();
            hasValue_ = o.hasValue_;
            if (hasValue_) new (&storage_) T(std::move(o.valueRef()));
            else new (&errStorage_) Error(std::move(o.errorRef()));
        }
        return *this;
    }

    ~Expected() { destroy(); }

    /// @brief Check if the Expected holds a value.
    explicit operator bool() const { return hasValue_; }

    /// @brief Check if the Expected holds a value.
    bool hasValue() const { return hasValue_; }

    /// @brief Get the value. Throws Error if no value.
    T& value() {
        if (!hasValue_) throw errorRef();
        return valueRef();
    }

    /// @brief Get the value (const). Throws Error if no value.
    const T& value() const {
        if (!hasValue_) throw errorRef();
        return valueRef();
    }

    /// @brief Get the error. Throws std::logic_error if has value.
    const Error& error() const {
        if (hasValue_) throw std::logic_error("Expected has value, not error");
        return errorRef();
    }

    /// @brief Get value or a default.
    T valueOr(const T& def) const { return hasValue_ ? valueRef() : def; }

    /// @brief Map the value through a function.
    template<typename F>
    auto map(F&& f) const -> Expected<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (hasValue_) return Expected<U>(f(valueRef()));
        return Expected<U>(errorRef());
    }

private:
    void destroy() {
        if (hasValue_) valueRef().~T();
        else errorRef().~Error();
    }

    T& valueRef() { return *reinterpret_cast<T*>(&storage_); }
    const T& valueRef() const { return *reinterpret_cast<const T*>(&storage_); }
    Error& errorRef() { return *reinterpret_cast<Error*>(&errStorage_); }
    const Error& errorRef() const { return *reinterpret_cast<const Error*>(&errStorage_); }

    bool hasValue_;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
    typename std::aligned_storage<sizeof(Error), alignof(Error)>::type errStorage_;
};

/// @brief Specialization for void.
template<>
class Expected<void> {
public:
    Expected() : hasValue_(true) {}
    Expected(const Error& err) : hasValue_(false), error_(err) {}
    Expected(Error&& err) : hasValue_(false), error_(std::move(err)) {}

    explicit operator bool() const { return hasValue_; }
    bool hasValue() const { return hasValue_; }

    const Error& error() const {
        if (hasValue_) throw std::logic_error("Expected has value, not error");
        return error_;
    }

private:
    bool hasValue_;
    Error error_{""};
};

} // namespace mbedbus

#endif // MBEDBUS_ERROR_H
