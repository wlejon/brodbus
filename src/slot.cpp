#include "brodbus/slot.h"

#include <utility>

namespace brodbus {

#if defined(__linux__)

Slot::~Slot() {
    reset();
}

Slot::Slot(Slot&& other) noexcept : slot_(other.slot_) {
    other.slot_ = nullptr;
}

Slot& Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        other.slot_ = nullptr;
    }
    return *this;
}

void Slot::reset(sd_bus_slot* slot) noexcept {
    if (slot_) {
        sd_bus_slot_unref(slot_);
    }
    slot_ = slot;
}

sd_bus_slot* Slot::release() noexcept {
    sd_bus_slot* s = slot_;
    slot_ = nullptr;
    return s;
}

#endif  // __linux__

}  // namespace brodbus
