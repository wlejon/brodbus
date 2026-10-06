#include "check.h"
#include "brodbus/error.h"

#include <cerrno>

using namespace brodbus;

void test_error_default() {
    Error err;
    CHECK(!err.is_set());
    CHECK(!err);
    CHECK_EQ(std::string(err.name()), std::string(""));
    CHECK_EQ(std::string(err.message()), std::string(""));
    CHECK_EQ(err.to_string(), std::string(""));
    CHECK_EQ(err.get_errno(), 0);
}

void test_error_create() {
    Error err = Error::create("org.freedesktop.DBus.Error.Failed", "Something went wrong");
    CHECK(err.is_set());
    CHECK(static_cast<bool>(err));
    CHECK_EQ(std::string(err.name()), std::string("org.freedesktop.DBus.Error.Failed"));
    CHECK_EQ(std::string(err.message()), std::string("Something went wrong"));
    CHECK_EQ(err.to_string(), std::string("org.freedesktop.DBus.Error.Failed: Something went wrong"));
}

void test_error_from_errno() {
    Error err = Error::from_errno(ENOENT);
    CHECK(err.is_set());
    CHECK_EQ(err.get_errno(), ENOENT);
    CHECK(!err.to_string().empty());
}

void test_error_move() {
    Error err1 = Error::create("org.test.Error", "Custom error");
    Error err2 = std::move(err1);
    CHECK(err2.is_set());
    CHECK_EQ(err2.to_string(), std::string("org.test.Error: Custom error"));
    CHECK(!err1.is_set());

    Error err3;
    err3 = std::move(err2);
    CHECK(err3.is_set());
    CHECK_EQ(err3.to_string(), std::string("org.test.Error: Custom error"));
    CHECK(!err2.is_set());
}

void test_error_reset() {
    Error err = Error::create("org.test.Reset", "Will be reset");
    CHECK(err.is_set());
    err.reset();
    CHECK(!err.is_set());
    CHECK_EQ(err.to_string(), std::string(""));
}

int main() {
    test_error_default();
    test_error_create();
    test_error_from_errno();
    test_error_move();
    test_error_reset();
    return bstest::finish("test_error");
}
