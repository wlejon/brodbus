#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/private_bus.h"
#include "brodbus/slot.h"

using namespace brodbus;

void test_slot_lifecycle() {
    Slot s;
    CHECK(!s.is_valid());
    CHECK(!s);
    CHECK(s.raw() == nullptr);
    CHECK(s.get() == nullptr);

    s.reset();
    CHECK(!s.is_valid());
}

void test_slot_with_bus() {
    PrivateBus daemon;
    if (!daemon.ok()) {
        bstest::skip("test_slot", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    int count = 0;
    Slot slot = bus->add_match("type='signal',interface='org.bro.Test'", [&](Message&) {
        ++count;
    }, &err);

    REQUIRE(slot.is_valid());
    CHECK(static_cast<bool>(slot));
    CHECK(slot.raw() != nullptr);

    // Test move constructor
    Slot slot2 = std::move(slot);
    CHECK(!slot.is_valid());
    CHECK(slot2.is_valid());

    // Test move assignment
    Slot slot3;
    slot3 = std::move(slot2);
    CHECK(!slot2.is_valid());
    CHECK(slot3.is_valid());

    // Test reset
    slot3.reset();
    CHECK(!slot3.is_valid());
    CHECK(slot3.raw() == nullptr);
}

int main() {
    test_slot_lifecycle();
    test_slot_with_bus();
    return bstest::finish("test_slot");
}
