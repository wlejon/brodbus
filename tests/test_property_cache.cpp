#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/private_bus.h"
#include "brodbus/property_cache.h"

#include <chrono>

using namespace brodbus;
using namespace std::chrono_literals;

void test_property_cache_manual() {
    PropertyCache cache;
    CHECK(!cache.is_valid());
    CHECK(!cache.has_property("Active"));
    CHECK_EQ(cache.get_bool("Active", false), false);

    bool changed = false;
    cache.on_change([&](const std::string& name, const PropertyValue& val) {
        if (name == "Active" && val.as_bool() == true) {
            changed = true;
        }
    });

    cache.set_cached_property("Active", true);
    CHECK(cache.has_property("Active"));
    CHECK_EQ(cache.get_bool("Active", false), true);
    CHECK(changed);

    cache.set_cached_property("Title", std::string("Desktop"));
    CHECK_EQ(cache.get_string("Title"), std::string("Desktop"));

    cache.set_cached_property("Count", uint32_t(50));
    CHECK_EQ(cache.get_uint32("Count"), 50u);

    CHECK_EQ(cache.size(), size_t(3));
}

void test_property_cache_signal_update() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_property_cache", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    const std::string path = "/org/bro/Device";
    const std::string iface = "org.bro.Device";

    PropertyCache cache(*bus, "", path, iface, /*auto_refresh=*/false);
    REQUIRE(cache.is_valid());

    // Pre-populate one property that will later be invalidated
    cache.set_cached_property("DeprecatedProp", std::string("old"));
    CHECK(cache.has_property("DeprecatedProp"));

    std::map<std::string, PropertyValue> received_changes;
    std::string invalidated_prop;

    cache.on_change([&](const std::string& name, const PropertyValue& val) {
        received_changes[name] = val;
    });

    cache.on_invalidate([&](const std::string& name) {
        invalidated_prop = name;
    });

    // Flush any match rules
    while (bus->process() > 0) {}

    // Emit org.freedesktop.DBus.Properties.PropertiesChanged signal
    // Signature: (sa{sv}as)
    Message sig = bus->new_signal(path, "org.freedesktop.DBus.Properties", "PropertiesChanged", &err);
    REQUIRE(sig.is_valid());

    CHECK(sig.append_string(iface));

    // Append changed_properties: a{sv}
    CHECK(sig.open_container('a', "{sv}") >= 0);

    // Entry 1: Name = "MyDevice"
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Name"));
    CHECK(sig.append_variant(PropertyValue("MyDevice")));
    CHECK(sig.close_container() >= 0);

    // Entry 2: Battery = 85 (uint32)
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Battery"));
    CHECK(sig.append_variant(PropertyValue(uint32_t(85))));
    CHECK(sig.close_container() >= 0);

    // Entry 3: Online = true (bool)
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Online"));
    CHECK(sig.append_variant(PropertyValue(true)));
    CHECK(sig.close_container() >= 0);

    CHECK(sig.close_container() >= 0);  // close array a{sv}

    // Append invalidated_properties: as
    std::vector<std::string> inv_list = {"DeprecatedProp"};
    CHECK(sig.append_string_list(inv_list));

    // Send signal
    CHECK(bus->send(sig, nullptr, &err));

    // Wait for signal processing
    bool got = bstest::wait_until([&] {
        bus->wait(20000);
        while (bus->process() > 0) {}
        return cache.has_property("Name") && cache.has_property("Battery");
    }, 3000ms);

    CHECK(got);
    CHECK_EQ(cache.get_string("Name"), std::string("MyDevice"));
    CHECK_EQ(cache.get_uint32("Battery"), 85u);
    CHECK_EQ(cache.get_bool("Online"), true);

    // Verify DeprecatedProp was removed via invalidated list
    CHECK(!cache.has_property("DeprecatedProp"));
    CHECK_EQ(invalidated_prop, std::string("DeprecatedProp"));

    CHECK_EQ(received_changes["Name"].as_string(), std::string("MyDevice"));
    CHECK_EQ(received_changes["Battery"].as_uint32(), 85u);
    CHECK_EQ(received_changes["Online"].as_bool(), true);
}

int main() {
    test_property_cache_manual();
    test_property_cache_signal_update();
    return bstest::finish("test_property_cache");
}
