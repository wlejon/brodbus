#include "brodbus/types.h"

namespace brodbus {

bool PropertyValue::as_bool(bool def) const {
    if (auto p = get_if<bool>()) return *p;
    return def;
}

std::string PropertyValue::as_string(const std::string& def) const {
    if (auto p = get_if<std::string>()) return *p;
    if (auto p = get_if<ObjectPath>()) return p->path;
    return def;
}

uint32_t PropertyValue::as_uint32(uint32_t def) const {
    if (auto p = get_if<uint32_t>()) return *p;
    if (auto p = get_if<uint16_t>()) return static_cast<uint32_t>(*p);
    if (auto p = get_if<uint8_t>()) return static_cast<uint32_t>(*p);
    if (auto p = get_if<int32_t>()) return static_cast<uint32_t>(*p);
    return def;
}

int32_t PropertyValue::as_int32(int32_t def) const {
    if (auto p = get_if<int32_t>()) return *p;
    if (auto p = get_if<int16_t>()) return static_cast<int32_t>(*p);
    if (auto p = get_if<uint32_t>()) return static_cast<int32_t>(*p);
    return def;
}

uint64_t PropertyValue::as_uint64(uint64_t def) const {
    if (auto p = get_if<uint64_t>()) return *p;
    if (auto p = get_if<uint32_t>()) return static_cast<uint64_t>(*p);
    if (auto p = get_if<int64_t>()) return static_cast<uint64_t>(*p);
    return def;
}

int64_t PropertyValue::as_int64(int64_t def) const {
    if (auto p = get_if<int64_t>()) return *p;
    if (auto p = get_if<int32_t>()) return static_cast<int64_t>(*p);
    if (auto p = get_if<uint64_t>()) return static_cast<int64_t>(*p);
    return def;
}

double PropertyValue::as_double(double def) const {
    if (auto p = get_if<double>()) return *p;
    return def;
}

ObjectPath PropertyValue::as_object_path(const ObjectPath& def) const {
    if (auto p = get_if<ObjectPath>()) return *p;
    if (auto p = get_if<std::string>()) return ObjectPath(*p);
    return def;
}

UnixFd PropertyValue::as_unix_fd(UnixFd def) const {
    if (auto p = get_if<UnixFd>()) return *p;
    return def;
}

std::vector<std::string> PropertyValue::as_string_list() const {
    if (auto p = get_if<std::vector<std::string>>()) return *p;
    return {};
}

std::vector<uint8_t> PropertyValue::as_byte_list() const {
    if (auto p = get_if<std::vector<uint8_t>>()) return *p;
    return {};
}

}  // namespace brodbus
