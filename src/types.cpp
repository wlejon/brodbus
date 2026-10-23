#include "brodbus/types.h"

#include <limits>

namespace brodbus {

bool is_valid_object_path(std::string_view p) noexcept {
    if (p.empty() || p[0] != '/') return false;
    if (p.size() == 1) return true;
    if (p.back() == '/') return false;
    for (std::size_t i = 1; i < p.size(); ++i) {
        char c = p[i];
        if (c == '/') {
            if (p[i - 1] == '/') return false;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

namespace {

bool is_basic_type(char c) noexcept {
    switch (c) {
        case 'y': case 'b': case 'n': case 'q':
        case 'i': case 'u': case 'x': case 't':
        case 'd': case 's': case 'o': case 'g':
        case 'h':
            return true;
        default:
            return false;
    }
}

bool parse_single_type(std::string_view sig, std::size_t& pos,
                       int array_depth, int struct_depth) noexcept {
    if (pos >= sig.size()) return false;
    if (array_depth > 32 || struct_depth > 32) return false;
    const char c = sig[pos++];
    if (is_basic_type(c) || c == 'v') return true;
    if (c == 'a') {
        if (array_depth + 1 > 32 || pos >= sig.size()) return false;
        if (sig[pos] == '{') {
            if (struct_depth + 1 > 32) return false;
            ++pos;  // consume '{'
            if (pos >= sig.size() || !is_basic_type(sig[pos])) return false;
            ++pos;  // key type
            if (!parse_single_type(sig, pos, array_depth + 1, struct_depth + 1)) return false;
            if (pos >= sig.size() || sig[pos] != '}') return false;
            ++pos;  // consume '}'
            return true;
        }
        return parse_single_type(sig, pos, array_depth + 1, struct_depth);
    }
    if (c == '(') {
        if (struct_depth + 1 > 32) return false;
        if (pos >= sig.size() || sig[pos] == ')') return false;  // empty struct () is forbidden in D-Bus
        while (pos < sig.size() && sig[pos] != ')') {
            if (!parse_single_type(sig, pos, array_depth, struct_depth + 1)) return false;
        }
        if (pos >= sig.size() || sig[pos] != ')') return false;
        ++pos;  // consume ')'
        return true;
    }
    return false;
}

}  // namespace

bool is_valid_signature(std::string_view sig, bool allow_multiple) noexcept {
    if (sig.size() > 255) return false;
    for (char c : sig) {
        if (c == '\0') return false;
    }
    if (sig.empty()) return allow_multiple;
    std::size_t pos = 0;
    int count = 0;
    while (pos < sig.size()) {
        if (!parse_single_type(sig, pos, 0, 0)) return false;
        ++count;
        if (!allow_multiple && count > 1) return false;
    }
    return pos == sig.size();
}

bool align_dbus_offset(std::size_t pos, std::size_t align, std::size_t* out) noexcept {
    if (align == 0 || (align & (align - 1)) != 0 || align > 8) return false;
    const std::size_t mask = align - 1;
    if (pos > std::numeric_limits<std::size_t>::max() - mask) return false;
    if (out) *out = (pos + mask) & ~mask;
    return true;
}

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
