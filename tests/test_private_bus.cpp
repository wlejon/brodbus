#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/private_bus.h"

#include <csignal>
#include <unistd.h>

using namespace brodbus;

void test_private_bus_launch_and_stop() {
    pid_t spawned_pid = -1;
    {
        PrivateBus daemon;
        if (!daemon.is_valid()) {
            bstest::skip("test_private_bus", "dbus-daemon could not start");
        }

        CHECK(daemon.is_valid());
        CHECK(static_cast<bool>(daemon));
        CHECK(daemon.pid() > 0);
        spawned_pid = daemon.pid();

        std::string addr = daemon.address();
        CHECK(!addr.empty());
        CHECK_EQ(addr.substr(0, 5), std::string("unix:"));

        auto env_map = daemon.env();
        CHECK(env_map.find("DBUS_SESSION_BUS_ADDRESS") != env_map.end());
        CHECK_EQ(env_map["DBUS_SESSION_BUS_ADDRESS"], addr);

        // Verify we can connect to the daemon
        std::string err;
        auto bus = Bus::open_address(addr, &err);
        REQUIRE(bus);
        CHECK(bus->is_valid());
        CHECK(!bus->unique_name().empty());
    }

    // Verify the daemon has exited after destruction
    if (spawned_pid > 0) {
        // kill with signal 0 checks process existence
        bool gone = bstest::wait_until([&] {
            return kill(spawned_pid, 0) != 0;
        }, std::chrono::seconds(2));
        CHECK(gone);
    }
}

void test_private_bus_move() {
    PrivateBus d1;
    if (!d1.is_valid()) {
        bstest::skip("test_private_bus", "dbus-daemon could not start");
    }

    pid_t p = d1.pid();
    std::string a = d1.address();

    PrivateBus d2 = std::move(d1);
    CHECK(!d1.is_valid());
    CHECK_EQ(d1.pid(), -1);
    CHECK(d2.is_valid());
    CHECK_EQ(d2.pid(), p);
    CHECK_EQ(d2.address(), a);

    PrivateBus d3;
    d3 = std::move(d2);
    CHECK(!d2.is_valid());
    CHECK(d3.is_valid());
    CHECK_EQ(d3.pid(), p);
    CHECK_EQ(d3.address(), a);

    d3.stop();
    CHECK(!d3.is_valid());
    CHECK_EQ(d3.pid(), -1);
}

int main() {
    test_private_bus_launch_and_stop();
    test_private_bus_move();
    return bstest::finish("test_private_bus");
}
