#include "check.h"
#include "brodbus/bus.h"
#include "brodbus/message.h"
#include "brodbus/private_bus.h"

#include <unistd.h>

using namespace brodbus;

void test_message_lifecycle() {
    Message m;
    CHECK(!m.is_valid());
    CHECK(!m);
    CHECK(m.raw() == nullptr);
    CHECK_EQ(m.get_path(), std::string(""));
    CHECK_EQ(m.get_interface(), std::string(""));
    CHECK_EQ(m.get_member(), std::string(""));
}

void test_message_basic_types() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_message", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    Message m = bus->new_signal("/org/bro/Test", "org.bro.Test", "Data", &err);
    REQUIRE(m.is_valid());

    CHECK_EQ(m.get_path(), std::string("/org/bro/Test"));
    CHECK_EQ(m.get_interface(), std::string("org.bro.Test"));
    CHECK_EQ(m.get_member(), std::string("Data"));
    CHECK(m.is_signal("org.bro.Test", "Data"));

    // Append basic types
    CHECK(m.append_bool(true));
    CHECK(m.append_byte(0x42));
    CHECK(m.append_int16(-1234));
    CHECK(m.append_uint16(5678));
    CHECK(m.append_int32(-99999));
    CHECK(m.append_uint32(88888));
    CHECK(m.append_int64(-123456789LL));
    CHECK(m.append_uint64(987654321ULL));
    CHECK(m.append_double(3.14159));
    CHECK(m.append_string("Hello D-Bus"));
    CHECK(m.append_object_path(ObjectPath("/test/path")));

    int pipe_fds[2];
    REQUIRE(pipe(pipe_fds) == 0);
    CHECK(m.append_unix_fd(pipe_fds[0]));

    // Seal before rewind and read back
    CHECK(m.seal() >= 0);
    CHECK(m.rewind(true) >= 0);

    bool b = false;
    CHECK(m.read_bool(&b));
    CHECK_EQ(b, true);

    uint8_t y = 0;
    CHECK(m.read_byte(&y));
    CHECK_EQ(y, 0x42);

    int16_t n = 0;
    CHECK(m.read_int16(&n));
    CHECK_EQ(n, -1234);

    uint16_t q = 0;
    CHECK(m.read_uint16(&q));
    CHECK_EQ(q, 5678);

    int32_t i = 0;
    CHECK(m.read_int32(&i));
    CHECK_EQ(i, -99999);

    uint32_t u = 0;
    CHECK(m.read_uint32(&u));
    CHECK_EQ(u, 88888u);

    int64_t x = 0;
    CHECK(m.read_int64(&x));
    CHECK_EQ(x, -123456789LL);

    uint64_t t = 0;
    CHECK(m.read_uint64(&t));
    CHECK_EQ(t, 987654321ULL);

    double d = 0.0;
    CHECK(m.read_double(&d));
    CHECK(d > 3.14 && d < 3.15);

    std::string s;
    CHECK(m.read_string(&s));
    CHECK_EQ(s, std::string("Hello D-Bus"));

    ObjectPath o;
    CHECK(m.read_object_path(&o));
    CHECK_EQ(o.path, std::string("/test/path"));

    UnixFd h;
    CHECK(m.read_unix_fd(&h));
    CHECK(h.valid());

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

void test_message_containers() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_message", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    Message m = bus->new_signal("/org/bro/Test", "org.bro.Test", "Arrays", &err);
    REQUIRE(m.is_valid());

    std::vector<std::string> strings = {"alpha", "beta", "gamma"};
    CHECK(m.append_string_list(strings));

    std::vector<uint8_t> bytes = {10, 20, 30, 40, 50};
    CHECK(m.append_byte_list(bytes));

    CHECK(m.append_variant(PropertyValue(uint32_t(777))));
    CHECK(m.append_variant(PropertyValue("variant-string")));

    // Seal before rewind and read
    CHECK(m.seal() >= 0);
    CHECK(m.rewind(true) >= 0);

    std::vector<std::string> read_strings;
    CHECK(m.read_string_list(&read_strings));
    CHECK_EQ(read_strings.size(), strings.size());
    if (read_strings.size() == 3) {
        CHECK_EQ(read_strings[0], std::string("alpha"));
        CHECK_EQ(read_strings[1], std::string("beta"));
        CHECK_EQ(read_strings[2], std::string("gamma"));
    }

    std::vector<uint8_t> read_bytes;
    CHECK(m.read_byte_list(&read_bytes));
    CHECK_EQ(read_bytes.size(), bytes.size());
    if (read_bytes.size() == 5) {
        CHECK_EQ(read_bytes[0], 10);
        CHECK_EQ(read_bytes[4], 50);
    }

    PropertyValue v1;
    CHECK(m.read_variant(&v1));
    CHECK_EQ(v1.as_uint32(), 777u);

    PropertyValue v2;
    CHECK(m.read_variant(&v2));
    CHECK_EQ(v2.as_string(), std::string("variant-string"));
}

void test_message_unsupported_variants_and_append_cleanup() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_message", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    Message m = bus->new_signal("/org/bro/Test", "org.bro.Test", "ComplexVariants", &err);
    REQUIRE(m.is_valid());

    // 1. Variant containing 'ao' (array of object paths)
    CHECK(m.open_container('v', "ao") >= 0);
    CHECK(m.open_container('a', "o") >= 0);
    CHECK(m.append_object_path("/org/bro/Item1"));
    CHECK(m.append_object_path("/org/bro/Item2"));
    CHECK(m.close_container() >= 0);
    CHECK(m.close_container() >= 0);

    // 2. Variant containing 'a{sv}' (nested dictionary)
    CHECK(m.open_container('v', "a{sv}") >= 0);
    CHECK(m.open_container('a', "{sv}") >= 0);
    CHECK(m.open_container('e', "sv") >= 0);
    CHECK(m.append_string("NestedKey"));
    CHECK(m.append_variant(PropertyValue(int32_t(42))));
    CHECK(m.close_container() >= 0);
    CHECK(m.close_container() >= 0);
    CHECK(m.close_container() >= 0);

    // 3. Variant containing struct '(ii)'
    CHECK(m.open_container('v', "(ii)") >= 0);
    CHECK(m.open_container('r', "ii") >= 0);
    CHECK(m.append_int32(10));
    CHECK(m.append_int32(20));
    CHECK(m.close_container() >= 0);
    CHECK(m.close_container() >= 0);

    // 4. Supported variant after unsupported variants
    CHECK(m.append_variant(PropertyValue(std::string("survives-after-skip"))));

    // 5. Invalid append_variant (e.g. invalid object path inside variant) cleans up 'v' container
    CHECK(!m.append_variant(PropertyValue(ObjectPath("not_a_valid_dbus_path"))));
    CHECK(!m.append_variant(PropertyValue(ObjectPath("/bad//double_slash"))));
    CHECK(!m.append_variant(PropertyValue(ObjectPath("/bad/trailing/"))));
    CHECK(!m.append_variant(PropertyValue(std::string("embedded\0nul", 12))));
    CHECK(!m.append_string(std::string("embedded\0nul", 12)));
    CHECK(!m.append_object_path("/bad//path"));
    CHECK(!m.append_object_path(std::string("/bad/\0nul", 9)));
    // Subsequent valid append on the same message must still succeed at top level
    CHECK(m.append_uint32(12345u));

    CHECK(m.seal() >= 0);
    CHECK(m.rewind(true) >= 0);

    PropertyValue v_ao(uint32_t(99));
    CHECK(m.read_variant(&v_ao));
    CHECK(v_ao.is_empty());

    PropertyValue v_asv(std::string("non-empty"));
    CHECK(m.read_variant(&v_asv));
    CHECK(v_asv.is_empty());

    PropertyValue v_struct(true);
    CHECK(m.read_variant(&v_struct));
    CHECK(v_struct.is_empty());

    PropertyValue v_after;
    CHECK(m.read_variant(&v_after));
    CHECK_EQ(v_after.as_string(), std::string("survives-after-skip"));

    uint32_t trailing = 0;
    CHECK(m.read_uint32(&trailing));
    CHECK_EQ(trailing, 12345u);
    CHECK(m.at_end(true));
}

void test_message_move() {
    PrivateBus daemon;
    if (!daemon.is_valid()) {
        bstest::skip("test_message", "dbus-daemon could not start");
    }

    std::string err;
    auto bus = Bus::open_address(daemon.address(), &err);
    REQUIRE(bus);

    Message m1 = bus->new_signal("/org/bro/Test", "org.bro.Test", "Ping", &err);
    REQUIRE(m1.is_valid());

    Message m2 = std::move(m1);
    CHECK(!m1.is_valid());
    CHECK(m2.is_valid());
    CHECK_EQ(m2.get_member(), std::string("Ping"));

    Message m3;
    m3 = std::move(m2);
    CHECK(!m2.is_valid());
    CHECK(m3.is_valid());
    CHECK_EQ(m3.get_member(), std::string("Ping"));
}

int main() {
    test_message_lifecycle();
    test_message_basic_types();
    test_message_containers();
    test_message_unsupported_variants_and_append_cleanup();
    test_message_move();
    return bstest::finish("test_message");
}
