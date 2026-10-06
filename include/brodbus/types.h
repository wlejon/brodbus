#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace brodbus {

struct ObjectPath {
    std::string path;

    ObjectPath() = default;
    ObjectPath(std::string p) : path(std::move(p)) {}
    ObjectPath(const char* p) : path(p ? p : "") {}

    bool valid() const noexcept { return !path.empty() && path[0] == '/'; }
    explicit operator bool() const noexcept { return valid(); }
    const std::string& str() const noexcept { return path; }
    const char* c_str() const noexcept { return path.c_str(); }

    bool operator==(const ObjectPath& o) const noexcept { return path == o.path; }
    bool operator!=(const ObjectPath& o) const noexcept { return path != o.path; }
    bool operator<(const ObjectPath& o) const noexcept { return path < o.path; }
};

struct UnixFd {
    int fd = -1;

    UnixFd() = default;
    explicit UnixFd(int f) : fd(f) {}

    bool valid() const noexcept { return fd >= 0; }
    explicit operator bool() const noexcept { return valid(); }

    bool operator==(const UnixFd& o) const noexcept { return fd == o.fd; }
    bool operator!=(const UnixFd& o) const noexcept { return fd != o.fd; }
};

using PropertyValueVariant = std::variant<
    std::monostate,
    bool,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    double,
    std::string,
    ObjectPath,
    UnixFd,
    std::vector<std::string>,
    std::vector<uint8_t>
>;

class PropertyValue {
public:
    PropertyValue() = default;

    PropertyValue(bool v) : value_(v) {}
    PropertyValue(uint8_t v) : value_(v) {}
    PropertyValue(int16_t v) : value_(v) {}
    PropertyValue(uint16_t v) : value_(v) {}
    PropertyValue(int32_t v) : value_(v) {}
    PropertyValue(uint32_t v) : value_(v) {}
    PropertyValue(int64_t v) : value_(v) {}
    PropertyValue(uint64_t v) : value_(v) {}
    PropertyValue(double v) : value_(v) {}
    PropertyValue(std::string v) : value_(std::move(v)) {}
    PropertyValue(const char* v) : value_(std::string(v ? v : "")) {}
    PropertyValue(ObjectPath v) : value_(std::move(v)) {}
    PropertyValue(UnixFd v) : value_(v) {}
    PropertyValue(std::vector<std::string> v) : value_(std::move(v)) {}
    PropertyValue(std::vector<uint8_t> v) : value_(std::move(v)) {}

    bool is_empty() const noexcept { return std::holds_alternative<std::monostate>(value_); }
    explicit operator bool() const noexcept { return !is_empty(); }

    template <typename T>
    bool is() const noexcept { return std::holds_alternative<T>(value_); }

    template <typename T>
    const T* get_if() const noexcept { return std::get_if<T>(&value_); }

    template <typename T>
    T get_or(const T& def) const {
        if (auto p = get_if<T>()) return *p;
        return def;
    }

    bool as_bool(bool def = false) const;
    std::string as_string(const std::string& def = "") const;
    uint32_t as_uint32(uint32_t def = 0) const;
    int32_t as_int32(int32_t def = 0) const;
    uint64_t as_uint64(uint64_t def = 0) const;
    int64_t as_int64(int64_t def = 0) const;
    double as_double(double def = 0.0) const;
    ObjectPath as_object_path(const ObjectPath& def = {}) const;
    UnixFd as_unix_fd(UnixFd def = {}) const;
    std::vector<std::string> as_string_list() const;
    std::vector<uint8_t> as_byte_list() const;

    const PropertyValueVariant& raw_variant() const noexcept { return value_; }

    bool operator==(const PropertyValue& o) const = default;

private:
    PropertyValueVariant value_;
};

}  // namespace brodbus
