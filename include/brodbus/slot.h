#pragma once

#if defined(__linux__)
#include <systemd/sd-bus.h>
#else
struct sd_bus_slot;
#endif

namespace brodbus {

class Slot {
public:
    Slot() noexcept : slot_(nullptr) {}
    explicit Slot(sd_bus_slot* slot) noexcept : slot_(slot) {}
    ~Slot();

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    Slot(Slot&& other) noexcept;
    Slot& operator=(Slot&& other) noexcept;

    void reset(sd_bus_slot* slot = nullptr) noexcept;
    sd_bus_slot* release() noexcept;

    sd_bus_slot* raw() const noexcept { return slot_; }
    sd_bus_slot* get() const noexcept { return slot_; }
    bool is_valid() const noexcept { return slot_ != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }

private:
    sd_bus_slot* slot_ = nullptr;
};

}  // namespace brodbus
