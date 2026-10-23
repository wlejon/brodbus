#include "check.h"
#include "brodbus/types.h"

#include <limits>

using namespace brodbus;

void test_object_path() {
    ObjectPath p1;
    CHECK(!p1.valid());
    CHECK(!p1);

    ObjectPath p2("/org/freedesktop/DBus");
    CHECK(p2.valid());
    CHECK(static_cast<bool>(p2));
    CHECK_EQ(p2.str(), std::string("/org/freedesktop/DBus"));
    CHECK_EQ(std::string(p2.c_str()), std::string("/org/freedesktop/DBus"));

    ObjectPath p3("/org/freedesktop/DBus");
    CHECK(p2 == p3);
    CHECK(!(p2 != p3));

    ObjectPath p4("/a");
    CHECK(p4 < p2);

    // Adversarial object paths: root "/" is valid, but trailing slash, double slash,
    // embedded NUL, spaces, hyphens, or missing leading slash are invalid.
    CHECK(ObjectPath("/").valid());
    CHECK(!ObjectPath("//").valid());
    CHECK(!ObjectPath("/org//freedesktop").valid());
    CHECK(!ObjectPath("/org/freedesktop/").valid());
    CHECK(!ObjectPath("org/freedesktop").valid());
    CHECK(!ObjectPath("/org/free-desktop").valid());
    CHECK(!ObjectPath("/org/free.desktop").valid());
    CHECK(!ObjectPath(std::string("/org/\0bad", 9)).valid());
}

void test_dbus_signature_and_alignment_bounds() {
    // 1. Symbolic 64-bit alignment rounding `(pos + align - 1) & ~(align - 1)` verification:
    // Every valid D-Bus alignment (1, 2, 4, 8) must round up without overflow when
    // pos <= SIZE_MAX - (align - 1), and deterministically reject wraparound near SIZE_MAX.
    const std::size_t max_sz = std::numeric_limits<std::size_t>::max();
    for (std::size_t align : {std::size_t(1), std::size_t(2), std::size_t(4), std::size_t(8)}) {
        const std::size_t mask = align - 1;
        for (std::size_t pos = 0; pos <= 32; ++pos) {
            std::size_t out = 0;
            CHECK(align_dbus_offset(pos, align, &out));
            CHECK(out >= pos);
            CHECK((out % align) == 0);
            CHECK(out - pos < align);
        }
        // Largest representable aligned offset: SIZE_MAX & ~mask
        const std::size_t max_aligned = max_sz & ~mask;
        std::size_t out = 0;
        CHECK(align_dbus_offset(max_aligned, align, &out));
        CHECK_EQ(out, max_aligned);
        if (align > 1) {
            CHECK(align_dbus_offset(max_aligned - 1, align, &out));
            CHECK_EQ(out, max_aligned);
            // Every pos in [max_aligned + 1 .. SIZE_MAX] would wrap (pos + align - 1) to 0..mask-1!
            for (std::size_t overflow_pos = max_aligned + 1; overflow_pos != 0; ++overflow_pos) {
                CHECK(!align_dbus_offset(overflow_pos, align, &out));
                if (overflow_pos == max_sz) break;
            }
        }
    }
    // Invalid alignment values (0, 3, 5, 6, 7, 16) rejected.
    for (std::size_t bad_align : {std::size_t(0), std::size_t(3), std::size_t(6), std::size_t(16)}) {
        std::size_t out = 0;
        CHECK(!align_dbus_offset(0, bad_align, &out));
    }

    // 2. D-Bus type signature bounds & container nesting depth verification:
    CHECK(is_valid_signature("", true));
    CHECK(!is_valid_signature("", false));
    CHECK(is_valid_signature("s", false));
    CHECK(is_valid_signature("a{sv}", false));
    CHECK(is_valid_signature("(ii)a{ss}v", true));
    CHECK(!is_valid_signature("(ii)a{ss}v", false));  // multiple complete types not allowed in variant/array

    // Malformed signatures:
    CHECK(!is_valid_signature("a", true));         // array without element type
    CHECK(!is_valid_signature("()", true));        // empty struct forbidden by D-Bus spec
    CHECK(!is_valid_signature("(ii", true));       // unclosed struct
    CHECK(!is_valid_signature("ii)", true));       // unmatched closing paren
    CHECK(!is_valid_signature("{sv}", true));      // dict entry outside array 'a'
    CHECK(!is_valid_signature("a{vv}", true));     // non-basic dict key
    CHECK(!is_valid_signature("a{(ii)v}", true));  // struct as dict key
    CHECK(!is_valid_signature("a{s}", true));      // dict entry missing value type
    CHECK(!is_valid_signature("a{svi}", true));    // dict entry with >2 fields
    CHECK(!is_valid_signature(std::string("s\0i", 3), true));  // embedded NUL

    // Exact 32-deep array nesting is valid; 33-deep array nesting must be rejected.
    std::string a32(32, 'a');
    a32 += 'i';
    CHECK(is_valid_signature(a32, false));
    std::string a33(33, 'a');
    a33 += 'i';
    CHECK(!is_valid_signature(a33, false));

    // Exact 32-deep struct nesting is valid; 33-deep struct nesting must be rejected.
    std::string s32 = std::string(32, '(') + "i" + std::string(32, ')');
    CHECK(is_valid_signature(s32, false));
    std::string s33 = std::string(33, '(') + "i" + std::string(33, ')');
    CHECK(!is_valid_signature(s33, false));

    // Signature length > 255 must be rejected.
    CHECK(is_valid_signature(std::string(255, 'y'), true));
    CHECK(!is_valid_signature(std::string(256, 'y'), true));
}

void test_unix_fd() {
    UnixFd f1;
    CHECK(!f1.valid());
    CHECK(!f1);

    UnixFd f2(5);
    CHECK(f2.valid());
    CHECK(static_cast<bool>(f2));
    CHECK_EQ(f2.fd, 5);

    UnixFd f3(5);
    CHECK(f2 == f3);
    CHECK(!(f2 != f3));
}

void test_property_value_types() {
    PropertyValue empty;
    CHECK(empty.is_empty());
    CHECK(!empty);

    PropertyValue v_bool(true);
    CHECK(!v_bool.is_empty());
    CHECK(v_bool.is<bool>());
    CHECK_EQ(v_bool.as_bool(), true);
    CHECK_EQ(v_bool.as_bool(false), true);
    CHECK_EQ(empty.as_bool(false), false);

    PropertyValue v_u8(uint8_t(250));
    CHECK(v_u8.is<uint8_t>());
    CHECK_EQ(v_u8.as_uint32(), 250u);

    PropertyValue v_i16(int16_t(-1234));
    CHECK(v_i16.is<int16_t>());
    CHECK_EQ(v_i16.as_int32(), -1234);

    PropertyValue v_u16(uint16_t(65000));
    CHECK(v_u16.is<uint16_t>());
    CHECK_EQ(v_u16.as_uint32(), 65000u);

    PropertyValue v_i32(int32_t(-9999));
    CHECK(v_i32.is<int32_t>());
    CHECK_EQ(v_i32.as_int32(), -9999);

    PropertyValue v_u32(uint32_t(8888));
    CHECK(v_u32.is<uint32_t>());
    CHECK_EQ(v_u32.as_uint32(), 8888u);

    PropertyValue v_i64(int64_t(-1234567890LL));
    CHECK(v_i64.is<int64_t>());
    CHECK_EQ(v_i64.as_int64(), -1234567890LL);

    PropertyValue v_u64(uint64_t(9876543210ULL));
    CHECK(v_u64.is<uint64_t>());
    CHECK_EQ(v_u64.as_uint64(), 9876543210ULL);

    PropertyValue v_double(2.71828);
    CHECK(v_double.is<double>());
    CHECK(v_double.as_double() > 2.71 && v_double.as_double() < 2.72);

    PropertyValue v_str("hello world");
    CHECK(v_str.is<std::string>());
    CHECK_EQ(v_str.as_string(), std::string("hello world"));

    PropertyValue v_path(ObjectPath("/my/path"));
    CHECK(v_path.is<ObjectPath>());
    CHECK_EQ(v_path.as_object_path().path, std::string("/my/path"));
    CHECK_EQ(v_path.as_string(), std::string("/my/path"));

    PropertyValue v_fd(UnixFd(10));
    CHECK(v_fd.is<UnixFd>());
    CHECK_EQ(v_fd.as_unix_fd().fd, 10);

    std::vector<std::string> strings = {"foo", "bar"};
    PropertyValue v_str_list(strings);
    CHECK(v_str_list.is<std::vector<std::string>>());
    CHECK_EQ(v_str_list.as_string_list().size(), size_t(2));

    std::vector<uint8_t> bytes = {1, 2, 3};
    PropertyValue v_byte_list(bytes);
    CHECK(v_byte_list.is<std::vector<uint8_t>>());
    CHECK_EQ(v_byte_list.as_byte_list().size(), size_t(3));
}

void test_property_value_equality() {
    PropertyValue a(uint32_t(42));
    PropertyValue b(uint32_t(42));
    PropertyValue c(uint32_t(43));
    PropertyValue d("42");

    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(!(a == d));
}

int main() {
    test_object_path();
    test_dbus_signature_and_alignment_bounds();
    test_unix_fd();
    test_property_value_types();
    test_property_value_equality();
    return bstest::finish("test_types");
}
