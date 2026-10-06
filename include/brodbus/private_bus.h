#pragma once

#include <map>
#include <string>
#include <sys/types.h>

namespace brodbus {

class PrivateBus {
public:
    PrivateBus();
    ~PrivateBus();

    PrivateBus(const PrivateBus&) = delete;
    PrivateBus& operator=(const PrivateBus&) = delete;

    PrivateBus(PrivateBus&& other) noexcept;
    PrivateBus& operator=(PrivateBus&& other) noexcept;

    const std::string& address() const noexcept { return address_; }
    pid_t pid() const noexcept { return pid_; }
    bool is_valid() const noexcept { return pid_ > 0 && !address_.empty(); }
    bool ok() const noexcept { return is_valid(); }
    explicit operator bool() const noexcept { return is_valid(); }

    std::map<std::string, std::string> env() const {
        return {{"DBUS_SESSION_BUS_ADDRESS", address_}};
    }

    void stop();

private:
    std::string address_;
    pid_t pid_ = -1;
};

}  // namespace brodbus
