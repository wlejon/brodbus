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

void test_private_bus_restart() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_private_bus", "dbus-daemon could not start");
    }

    std::string addr1 = daemon.address();
    pid_t pid1 = daemon.pid();
    auto pos1 = addr1.find(",guid=");
    std::string base1 = (pos1 != std::string::npos) ? addr1.substr(0, pos1) : addr1;

    REQUIRE(daemon.restart());
    CHECK(daemon.is_valid());
    CHECK(daemon.pid() > 0);
    CHECK(daemon.pid() != pid1);

    std::string addr2 = daemon.address();
    auto pos2 = addr2.find(",guid=");
    std::string base2 = (pos2 != std::string::npos) ? addr2.substr(0, pos2) : addr2;

    CHECK_EQ(base1, base2);

    // Verify the restarted bus works
    std::string err;
    auto bus = Bus::open_address(addr2, &err);
    REQUIRE(bus);
    CHECK(bus->is_valid());
    CHECK(!bus->unique_name().empty());
}

int main() {
    test_private_bus_launch_and_stop();
    test_private_bus_move();
    test_private_bus_restart();
    return bstest::finish("test_private_bus");
}
