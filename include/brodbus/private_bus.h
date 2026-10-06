#pragma once

#include <map>
#include <string>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

namespace brodbus {

// The daemon's process id: pid_t where POSIX has one. Windows has no pid_t,
// and there PrivateBus is the unavailable stub whose pid() is always -1.
#if defined(_WIN32)
using process_id = int;
#else
using process_id = pid_t;
#endif

class PrivateBus {
public:
    explicit PrivateBus(const std::string& address = "");
    ~PrivateBus();

    PrivateBus(const PrivateBus&) = delete;
    PrivateBus& operator=(const PrivateBus&) = delete;

    PrivateBus(PrivateBus&& other) noexcept;
    PrivateBus& operator=(PrivateBus&& other) noexcept;

    const std::string& address() const noexcept { return address_; }
    process_id pid() const noexcept { return pid_; }
    bool is_valid() const noexcept { return pid_ > 0 && !address_.empty(); }
    bool ok() const noexcept { return is_valid(); }
    explicit operator bool() const noexcept { return is_valid(); }

    std::map<std::string, std::string> env() const {
        return {{"DBUS_SESSION_BUS_ADDRESS", address_}};
    }

    void stop();
    bool restart();

private:
    bool start();

    std::string base_address_;
    std::string address_;
    process_id pid_ = -1;
};

}  // namespace brodbus
