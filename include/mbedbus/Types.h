/// @file Types.h
/// @brief Strong typedefs for D-Bus types: ObjectPath, BusName, Signature, UnixFd.

#ifndef MBEDBUS_TYPES_H
#define MBEDBUS_TYPES_H

#include <string>
#include <utility>
#include <unistd.h>

namespace mbedbus {

/// @brief Strong typedef for D-Bus object path ("o").
class ObjectPath {
public:
    ObjectPath() = default;
    explicit ObjectPath(std::string path) : path_(std::move(path)) {}
    ObjectPath(const char* path) : path_(path) {}

    const std::string& str() const { return path_; }
    const char* c_str() const { return path_.c_str(); }
    bool empty() const { return path_.empty(); }

    bool operator==(const ObjectPath& o) const { return path_ == o.path_; }
    bool operator!=(const ObjectPath& o) const { return path_ != o.path_; }
    bool operator<(const ObjectPath& o) const { return path_ < o.path_; }

private:
    std::string path_;
};

/// @brief Strong typedef for D-Bus bus name.
class BusName {
public:
    BusName() = default;
    explicit BusName(std::string name) : name_(std::move(name)) {}
    BusName(const char* name) : name_(name) {}

    const std::string& str() const { return name_; }
    const char* c_str() const { return name_.c_str(); }
    bool empty() const { return name_.empty(); }

    bool operator==(const BusName& o) const { return name_ == o.name_; }
    bool operator!=(const BusName& o) const { return name_ != o.name_; }
    bool operator<(const BusName& o) const { return name_ < o.name_; }

private:
    std::string name_;
};

/// @brief Strong typedef for D-Bus type signature ("g").
class Signature {
public:
    Signature() = default;
    explicit Signature(std::string sig) : sig_(std::move(sig)) {}
    Signature(const char* sig) : sig_(sig) {}

    const std::string& str() const { return sig_; }
    const char* c_str() const { return sig_.c_str(); }

    bool operator==(const Signature& o) const { return sig_ == o.sig_; }
    bool operator!=(const Signature& o) const { return sig_ != o.sig_; }

private:
    std::string sig_;
};

/// @brief Wrapper for a Unix file descriptor transferred over D-Bus ("h").
class UnixFd {
public:
    UnixFd() : fd_(-1) {}
    explicit UnixFd(int fd) : fd_(fd) {}

    int get() const { return fd_; }

    /// @brief Release ownership of the fd (caller must close).
    int release() { int f = fd_; fd_ = -1; return f; }

    bool isValid() const { return fd_ >= 0; }

    bool operator==(const UnixFd& o) const { return fd_ == o.fd_; }
    bool operator!=(const UnixFd& o) const { return fd_ != o.fd_; }

private:
    int fd_;
};

/// @brief Call flags for method invocations.
enum class CallFlags : unsigned {
    Default = 0,
    NoReplyExpected = 1,
};

} // namespace mbedbus

#endif // MBEDBUS_TYPES_H
