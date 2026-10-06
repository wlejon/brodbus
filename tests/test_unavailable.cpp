#include "check.h"
#include "brodbus/brodbus.h"

#if defined(BRODBUS_BUILD_UNAVAILABLE_TEST)
using namespace brodbus::stub;
#else
using namespace brodbus;
#endif

void test_stubs() {
    std::string err;
    auto sys = Bus::open_system(&err);
    CHECK(sys == nullptr);
    CHECK_EQ(err, std::string("D-Bus is only supported on Linux"));

    err.clear();
    auto usr = Bus::open_user(&err);
    CHECK(usr == nullptr);
    CHECK_EQ(err, std::string("D-Bus is only supported on Linux"));

    err.clear();
    auto addr = Bus::open_address("unix:path=/tmp/test", &err);
    CHECK(addr == nullptr);
    CHECK_EQ(err, std::string("D-Bus is only supported on Linux"));

    Bus b;
    CHECK(!b.is_valid());
    CHECK_EQ(b.get_fd(), -1);
    CHECK_EQ(b.process(), -1);
    CHECK_EQ(b.wait(), -1);
    CHECK_EQ(b.flush(), -1);
    CHECK(!b.request_name("org.test", 0, &err));
    CHECK(!b.release_name("org.test", &err));

    Slot s;
    CHECK(!s.is_valid());

    Message m;
    CHECK(!m.is_valid());
    CHECK(!m.append_bool(true));
    bool val = false;
    CHECK(!m.read_bool(&val));

    PrivateBus pb;
    CHECK(!pb.is_valid());
    CHECK_EQ(pb.pid(), -1);
    CHECK(pb.address().empty());

    PropertyCache pc;
    CHECK(!pc.is_valid());
    CHECK(!pc.refresh(&err));
    CHECK(!pc.has_property("test"));

    Error e = Error::create("org.test.Stub", "msg");
    CHECK(!e.is_set());
}

int main() {
    test_stubs();
    return bstest::finish("test_unavailable");
}
