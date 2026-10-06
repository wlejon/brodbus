#pragma once

#if defined(__linux__)
#include <systemd/sd-bus.h>
#else
struct sd_bus;
#endif

#include "brodbus/error.h"
#include "brodbus/message.h"
#include "brodbus/slot.h"
#include "brodbus/types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace brodbus {

class Bus {
public:
    using SignalCallback = std::function<void(Message& msg)>;
    using RawSignalCallback = std::function<void(sd_bus_message* msg)>;

    Bus() noexcept : bus_(nullptr) {}
    explicit Bus(sd_bus* bus) noexcept : bus_(bus) {}
    ~Bus();

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    Bus(Bus&& other) noexcept;
    Bus& operator=(Bus&& other) noexcept;

    void reset(sd_bus* bus = nullptr) noexcept;
    sd_bus* release() noexcept;

    sd_bus* raw() const noexcept { return bus_; }
    bool is_valid() const noexcept { return bus_ != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }

    // Connection setup
    static std::unique_ptr<Bus> open_system(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_user(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_address(const std::string& address, std::string* error = nullptr);

    // Event loop & fd
    int get_fd() const noexcept;
    int process();
    int wait(uint64_t timeout_usec = UINT64_MAX);
    int flush();

    std::string unique_name() const;

    // Names
    bool request_name(const std::string& name, uint64_t flags = 0, std::string* error = nullptr);
    bool release_name(const std::string& name, std::string* error = nullptr);

    // Matches & slots
    Slot add_match(const std::string& match_rule, SignalCallback callback, std::string* error = nullptr);
    Slot add_match(const std::string& match_rule, RawSignalCallback callback, std::string* error = nullptr);

    // Methods & messages
    Message new_method_call(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        std::string* error = nullptr);

    Message new_signal(
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        std::string* error = nullptr);

    Message call(Message& message, uint64_t timeout_usec = 0, Error* error = nullptr);
    bool send(Message& message, uint64_t* serial = nullptr, std::string* error = nullptr);

    bool emit_signal(
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        std::function<void(Message&)> build_args = nullptr,
        std::string* error = nullptr);

    bool call_method(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        std::function<void(Message&)> build_args,
        std::function<void(Message&)> parse_reply,
        std::string* error = nullptr,
        uint64_t timeout_usec = 0);

    // Property getters
    bool get_property_bool(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        bool* out,
        std::string* error = nullptr);

    bool get_property_byte(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint8_t* out,
        std::string* error = nullptr);

    bool get_property_int16(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        int16_t* out,
        std::string* error = nullptr);

    bool get_property_uint16(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint16_t* out,
        std::string* error = nullptr);

    bool get_property_int32(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        int32_t* out,
        std::string* error = nullptr);

    bool get_property_uint32(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint32_t* out,
        std::string* error = nullptr);

    bool get_property_int64(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        int64_t* out,
        std::string* error = nullptr);

    bool get_property_uint64(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint64_t* out,
        std::string* error = nullptr);

    bool get_property_double(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        double* out,
        std::string* error = nullptr);

    bool get_property_string(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        std::string* out,
        std::string* error = nullptr);

    bool get_property_strv(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        std::vector<std::string>* out,
        std::string* error = nullptr);

    // Property setters
    bool set_property_bool(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        bool val,
        std::string* error = nullptr);

    bool set_property_string(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        const std::string& val,
        std::string* error = nullptr);

    bool set_property_uint32(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint32_t val,
        std::string* error = nullptr);

private:
    sd_bus* bus_ = nullptr;
};

}  // namespace brodbus
