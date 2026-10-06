#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/private_bus.h"

#include <chrono>

using namespace brodbus;
using namespace std::chrono_literals;

void test_bus_connection_and_hello() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_bus", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);
    CHECK(bus->is_valid());
    CHECK(static_cast<bool>(*bus));
    CHECK(bus->get_fd() >= 0);

    // Because open_address sets sd_bus_set_bus_client(bus, 1) and calls Hello,
    // the daemon assigns a unique bus name (e.g. :1.0).
    std::string unique = bus->unique_name();
    CHECK(!unique.empty());
    CHECK_EQ(unique[0], ':');

    // Test move semantics
    Bus moved = std::move(*bus);
    CHECK(!bus->is_valid());
    CHECK(moved.is_valid());
    CHECK_EQ(moved.unique_name(), unique);
}

void test_bus_names() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_bus", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    const std::string name = "org.bro.Service";
    CHECK(bus->request_name(name, 0, &err));

    // Release the name
    CHECK(bus->release_name(name, &err));
}

void test_bus_signals() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_bus", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    int count = 0;
    std::string received_msg;

    Slot slot = bus->add_match(
        "type='signal',interface='org.bro.Test',member='Notify'",
        [&](Message& msg) {
            msg.read_string(&received_msg);
            ++count;
        },
        &err);
    REQUIRE(slot.is_valid());

    // Flush any pending match setup
    while (bus->process() > 0) {}

    // Emit signal
    CHECK(bus->emit_signal("/org/bro/Test", "org.bro.Test", "Notify", [](Message& m) {
        m.append_string("signal-content-123");
    }, &err));

    bool got = bstest::wait_until([&] {
        bus->wait(20000);
        while (bus->process() > 0) {}
        return count > 0;
    }, 3000ms);

    CHECK(got);
    CHECK_EQ(count, 1);
    CHECK_EQ(received_msg, std::string("signal-content-123"));

    // Reset slot and verify signal is no longer delivered
    slot.reset();
    CHECK(!slot.is_valid());

    int prev_count = count;
    CHECK(bus->emit_signal("/org/bro/Test", "org.bro.Test", "Notify", [](Message& m) {
        m.append_string("signal-content-456");
    }, &err));

    for (int i = 0; i < 5; ++i) {
        bus->wait(10000);
        while (bus->process() > 0) {}
    }
    CHECK_EQ(count, prev_count);
}

void test_bus_method_call() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_bus", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    // Call org.freedesktop.DBus.ListNames
    std::vector<std::string> names;
    bool ok = bus->call_method(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames",
        nullptr,
        [&](Message& reply) {
            reply.read_string_list(&names);
        },
        &err);

    CHECK(ok);
    CHECK(!names.empty());

    bool found_dbus = false;
    bool found_unique = false;
    std::string unique = bus->unique_name();
    for (const auto& n : names) {
        if (n == "org.freedesktop.DBus") found_dbus = true;
        if (n == unique) found_unique = true;
    }
    CHECK(found_dbus);
    CHECK(found_unique);

    // Method call on nonexistent destination should fail with error
    Error call_err;
    Message bad_msg = bus->new_method_call("org.bro.Nonexistent", "/a", "org.bro.X", "Y");
    Message bad_reply = bus->call(bad_msg, 100000, &call_err);
    CHECK(!bad_reply.is_valid());
    CHECK(call_err.is_set());
    CHECK(!call_err.to_string().empty());
}

void test_bus_property_getters() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_bus", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    // Query nonexistent property
    std::string s;
    bool ok = bus->get_property_string("org.bro.Nobody", "/nobody", "org.bro.Nobody", "Prop", &s, &err);
    CHECK(!ok);
    CHECK(!err.empty());
}

int main() {
    test_bus_connection_and_hello();
    test_bus_names();
    test_bus_signals();
    test_bus_method_call();
    test_bus_property_getters();
    return bstest::finish("test_bus");
}
