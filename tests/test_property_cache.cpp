#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/private_bus.h"
#include "brodbus/property_cache.h"

#include <atomic>
#include <chrono>
#include <thread>

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

    // Entry 0: Complex unsupported variant (ao) BEFORE supported entries
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Devices"));
    CHECK(sig.open_container('v', "ao") >= 0);
    CHECK(sig.open_container('a', "o") >= 0);
    CHECK(sig.append_object_path("/org/bro/Device/1"));
    CHECK(sig.append_object_path("/org/bro/Device/2"));
    CHECK(sig.close_container() >= 0);
    CHECK(sig.close_container() >= 0);
    CHECK(sig.close_container() >= 0);

    // Entry 1: Name = "MyDevice"
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Name"));
    CHECK(sig.append_variant(PropertyValue("MyDevice")));
    CHECK(sig.close_container() >= 0);

    // Entry 1b: Complex unsupported struct variant ((ii)) in the middle
    CHECK(sig.open_container('e', "sv") >= 0);
    CHECK(sig.append_string("Geometry"));
    CHECK(sig.open_container('v', "(ii)") >= 0);
    CHECK(sig.open_container('r', "ii") >= 0);
    CHECK(sig.append_int32(1920));
    CHECK(sig.append_int32(1080));
    CHECK(sig.close_container() >= 0);
    CHECK(sig.close_container() >= 0);
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

static int handle_get_all_mock(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    Bus* srv_bus = static_cast<Bus*>(userdata);
    Message req(m, false);
    if (!req.is_method_call("org.freedesktop.DBus.Properties", "GetAll")) {
        return 0;
    }

    Message reply = req.new_method_return();
    if (!reply.is_valid()) return -1;

    reply.open_container('a', "{sv}");

    // 1. Unsupported 'ao' variant first
    reply.open_container('e', "sv");
    reply.append_string("ObjectPaths");
    reply.open_container('v', "ao");
    reply.open_container('a', "o");
    reply.append_object_path("/org/bro/A");
    reply.append_object_path("/org/bro/B");
    reply.close_container();
    reply.close_container();
    reply.close_container();

    // 2. Supported string property
    reply.open_container('e', "sv");
    reply.append_string("Model");
    reply.append_variant(PropertyValue(std::string("BroBook Pro")));
    reply.close_container();

    // 3. Unsupported 'a{sv}' nested dict variant
    reply.open_container('e', "sv");
    reply.append_string("Metadata");
    reply.open_container('v', "a{sv}");
    reply.open_container('a', "{sv}");
    reply.open_container('e', "sv");
    reply.append_string("SubKey");
    reply.append_variant(PropertyValue(uint32_t(123)));
    reply.close_container();
    reply.close_container();
    reply.close_container();
    reply.close_container();

    // 4. Supported uint32 property after complex variants
    reply.open_container('e', "sv");
    reply.append_string("Percentage");
    reply.append_variant(PropertyValue(uint32_t(92)));
    reply.close_container();

    reply.close_container();

    srv_bus->send(reply);
    return 1;
}

void test_property_cache_refresh_with_unsupported_variants() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_property_cache", "dbus-daemon could not start");
    }

    std::string err;
    auto srv_bus = Bus::open_address(daemon.address(), &err);
    auto cli_bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(srv_bus);
    REQUIRE(cli_bus);

    const std::string service = "org.bro.MockPower";
    const std::string path = "/org/bro/MockPower";
    const std::string iface = "org.bro.MockPower.Device";

    REQUIRE(srv_bus->request_name(service, 0, &err));

    sd_bus_slot* obj_slot = nullptr;
    int r = sd_bus_add_object(srv_bus->raw(), &obj_slot, path.c_str(), handle_get_all_mock, srv_bus.get());
    REQUIRE(r >= 0);

    std::atomic<bool> stop_srv{false};
    std::thread srv_thread([&] {
        while (!stop_srv.load()) {
            srv_bus->wait(20000);
            while (srv_bus->process() > 0) {}
        }
    });

    PropertyCache cache(*cli_bus, service, path, iface, /*auto_refresh=*/true);
    CHECK_EQ(cache.get_string("Model"), std::string("BroBook Pro"));
    CHECK_EQ(cache.get_uint32("Percentage"), 92u);
    CHECK(cache.has_property("ObjectPaths"));
    CHECK(cache.get("ObjectPaths")->is_empty());

    stop_srv.store(true);
    srv_thread.join();
    sd_bus_slot_unref(obj_slot);
}

int main() {
    test_property_cache_manual();
    test_property_cache_signal_update();
    test_property_cache_refresh_with_unsupported_variants();
    return bstest::finish("test_property_cache");
}
