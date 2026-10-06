#pragma once

#if defined(__linux__)
#include <systemd/sd-bus.h>
#else
struct sd_bus_error {
    const char* name = nullptr;
    const char* message = nullptr;
    int _need_free = 0;
};
#define SD_BUS_ERROR_NULL { nullptr, nullptr, 0 }
#endif

#include <string>

namespace brodbus {

class Error {
public:
    Error() noexcept;
    explicit Error(sd_bus_error err) noexcept;
    ~Error();

    Error(const Error&) = delete;
    Error& operator=(const Error&) = delete;

    Error(Error&& other) noexcept;
    Error& operator=(Error&& other) noexcept;

    static Error create(const std::string& name, const std::string& message);
    static Error from_errno(int err);

    sd_bus_error* raw() noexcept { return &error_; }
    const sd_bus_error* raw() const noexcept { return &error_; }

    bool is_set() const noexcept;
    explicit operator bool() const noexcept { return is_set(); }

    const char* name() const noexcept;
    const char* message() const noexcept;
    int get_errno() const noexcept;

    std::string to_string() const;
    void reset() noexcept;

private:
    sd_bus_error error_;
};

}  // namespace brodbus
