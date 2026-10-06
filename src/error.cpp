#include "brodbus/error.h"

#include <cstring>
#include <utility>

namespace brodbus {

#if defined(__linux__)

Error::Error() noexcept : error_(SD_BUS_ERROR_NULL) {}

Error::Error(sd_bus_error err) noexcept : error_(err) {}

Error::~Error() {
    reset();
}

Error::Error(Error&& other) noexcept : error_(other.error_) {
    other.error_ = SD_BUS_ERROR_NULL;
}

Error& Error::operator=(Error&& other) noexcept {
    if (this != &other) {
        reset();
        error_ = other.error_;
        other.error_ = SD_BUS_ERROR_NULL;
    }
    return *this;
}

void Error::reset() noexcept {
    sd_bus_error_free(&error_);
    error_ = SD_BUS_ERROR_NULL;
}

Error Error::create(const std::string& name, const std::string& message) {
    Error err;
    sd_bus_error_set(&err.error_, name.c_str(), message.c_str());
    return err;
}

Error Error::from_errno(int err_no) {
    Error err;
    sd_bus_error_set_errno(&err.error_, err_no);
    return err;
}

bool Error::is_set() const noexcept {
    return sd_bus_error_is_set(&error_) != 0;
}

const char* Error::name() const noexcept {
    return error_.name ? error_.name : "";
}

const char* Error::message() const noexcept {
    return error_.message ? error_.message : "";
}

int Error::get_errno() const noexcept {
    return sd_bus_error_get_errno(&error_);
}

std::string Error::to_string() const {
    if (!is_set()) return {};
    const char* n = name();
    const char* m = message();
    if (n && *n && m && *m) {
        return std::string(n) + ": " + m;
    }
    if (n && *n) return n;
    if (m && *m) return m;
    return {};
}

#endif  // __linux__

}  // namespace brodbus
