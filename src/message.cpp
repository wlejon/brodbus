#include "brodbus/message.h"

#include <cstring>
#include <utility>

namespace brodbus {

#if defined(__linux__)

Message::~Message() {
    if (owned_ && msg_) {
        sd_bus_message_unref(msg_);
    }
}

Message::Message(Message&& other) noexcept
    : msg_(other.msg_), owned_(other.owned_) {
    other.msg_ = nullptr;
    other.owned_ = false;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        if (owned_ && msg_) {
            sd_bus_message_unref(msg_);
        }
        msg_ = other.msg_;
        owned_ = other.owned_;
        other.msg_ = nullptr;
        other.owned_ = false;
    }
    return *this;
}

void Message::reset(sd_bus_message* msg, bool owned) noexcept {
    if (owned_ && msg_) {
        sd_bus_message_unref(msg_);
    }
    msg_ = msg;
    owned_ = owned;
}

sd_bus_message* Message::release() noexcept {
    sd_bus_message* m = msg_;
    msg_ = nullptr;
    owned_ = false;
    return m;
}

std::string Message::get_path() const {
    if (!msg_) return {};
    const char* p = sd_bus_message_get_path(msg_);
    return p ? std::string(p) : std::string();
}

std::string Message::get_interface() const {
    if (!msg_) return {};
    const char* i = sd_bus_message_get_interface(msg_);
    return i ? std::string(i) : std::string();
}

std::string Message::get_member() const {
    if (!msg_) return {};
    const char* m = sd_bus_message_get_member(msg_);
    return m ? std::string(m) : std::string();
}

std::string Message::get_sender() const {
    if (!msg_) return {};
    const char* s = sd_bus_message_get_sender(msg_);
    return s ? std::string(s) : std::string();
}

std::string Message::get_destination() const {
    if (!msg_) return {};
    const char* d = sd_bus_message_get_destination(msg_);
    return d ? std::string(d) : std::string();
}

std::string Message::get_signature() const {
    if (!msg_) return {};
    const char* sig = sd_bus_message_get_signature(msg_, 1);
    return sig ? std::string(sig) : std::string();
}

uint8_t Message::get_type() const {
    if (!msg_) return 0;
    uint8_t t = 0;
    if (sd_bus_message_get_type(msg_, &t) < 0) return 0;
    return t;
}

bool Message::is_signal(const std::string& interface, const std::string& member) const {
    if (!msg_) return false;
    return sd_bus_message_is_signal(msg_, interface.c_str(), member.c_str()) > 0;
}

bool Message::is_method_call(const std::string& interface, const std::string& member) const {
    if (!msg_) return false;
    return sd_bus_message_is_method_call(msg_, interface.c_str(), member.c_str()) > 0;
}

bool Message::is_method_error(const char* name) const {
    if (!msg_) return false;
    return sd_bus_message_is_method_error(msg_, name) > 0;
}

const sd_bus_error* Message::get_error() const {
    if (!msg_) return nullptr;
    return sd_bus_message_get_error(msg_);
}

int Message::get_errno() const {
    if (!msg_) return -EINVAL;
    return sd_bus_message_get_errno(msg_);
}

int Message::open_container(char type, const char* contents) {
    if (!msg_) return -EINVAL;
    return sd_bus_message_open_container(msg_, type, contents);
}

int Message::close_container() {
    if (!msg_) return -EINVAL;
    return sd_bus_message_close_container(msg_);
}

int Message::enter_container(char type, const char* contents) {
    if (!msg_) return -EINVAL;
    return sd_bus_message_enter_container(msg_, type, contents);
}

int Message::exit_container() {
    if (!msg_) return -EINVAL;
    return sd_bus_message_exit_container(msg_);
}

bool Message::at_end(bool complete) const {
    if (!msg_) return true;
    return sd_bus_message_at_end(msg_, complete ? 1 : 0) > 0;
}

int Message::peek_type(char* type, const char** contents) const {
    if (!msg_) return -EINVAL;
    return sd_bus_message_peek_type(msg_, type, contents);
}

int Message::rewind(bool complete) {
    if (!msg_) return -EINVAL;
    return sd_bus_message_rewind(msg_, complete ? 1 : 0);
}

int Message::seal(uint64_t cookie, uint64_t timeout_usec) {
    if (!msg_) return -EINVAL;
    return sd_bus_message_seal(msg_, cookie, timeout_usec);
}

bool Message::append_basic(char type, const void* value) {
    if (!msg_) return false;
    return sd_bus_message_append_basic(msg_, type, value) >= 0;
}

bool Message::append_bool(bool val) {
    int b = val ? 1 : 0;
    return append_basic('b', &b);
}

bool Message::append_byte(uint8_t val) {
    return append_basic('y', &val);
}

bool Message::append_int16(int16_t val) {
    return append_basic('n', &val);
}

bool Message::append_uint16(uint16_t val) {
    return append_basic('q', &val);
}

bool Message::append_int32(int32_t val) {
    return append_basic('i', &val);
}

bool Message::append_uint32(uint32_t val) {
    return append_basic('u', &val);
}

bool Message::append_int64(int64_t val) {
    return append_basic('x', &val);
}

bool Message::append_uint64(uint64_t val) {
    return append_basic('t', &val);
}

bool Message::append_double(double val) {
    return append_basic('d', &val);
}

bool Message::append_string(const std::string& val) {
    return append_basic('s', val.c_str());
}

bool Message::append_string(const char* val) {
    const char* str = val ? val : "";
    return append_basic('s', str);
}

bool Message::append_object_path(const std::string& val) {
    return append_basic('o', val.c_str());
}

bool Message::append_object_path(const ObjectPath& val) {
    return append_basic('o', val.c_str());
}

bool Message::append_unix_fd(int fd) {
    return append_basic('h', &fd);
}

bool Message::append_unix_fd(const UnixFd& val) {
    return append_basic('h', &val.fd);
}

bool Message::append_string_list(const std::vector<std::string>& list) {
    if (open_container('a', "s") < 0) return false;
    for (const auto& item : list) {
        if (!append_string(item)) {
            close_container();
            return false;
        }
    }
    return close_container() >= 0;
}

bool Message::append_byte_list(const std::vector<uint8_t>& bytes) {
    if (open_container('a', "y") < 0) return false;
    for (uint8_t b : bytes) {
        if (!append_byte(b)) {
            close_container();
            return false;
        }
    }
    return close_container() >= 0;
}

bool Message::append_variant(const PropertyValue& val) {
    const auto& var = val.raw_variant();
    return std::visit([this](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            if (open_container('v', "b") < 0) return false;
            if (!append_bool(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            if (open_container('v', "y") < 0) return false;
            if (!append_byte(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            if (open_container('v', "n") < 0) return false;
            if (!append_int16(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if (open_container('v', "q") < 0) return false;
            if (!append_uint16(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            if (open_container('v', "i") < 0) return false;
            if (!append_int32(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (open_container('v', "u") < 0) return false;
            if (!append_uint32(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            if (open_container('v', "x") < 0) return false;
            if (!append_int64(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            if (open_container('v', "t") < 0) return false;
            if (!append_uint64(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, double>) {
            if (open_container('v', "d") < 0) return false;
            if (!append_double(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (open_container('v', "s") < 0) return false;
            if (!append_string(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, ObjectPath>) {
            if (open_container('v', "o") < 0) return false;
            if (!append_object_path(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, UnixFd>) {
            if (open_container('v', "h") < 0) return false;
            if (!append_unix_fd(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            if (open_container('v', "as") < 0) return false;
            if (!append_string_list(v)) return false;
            return close_container() >= 0;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            if (open_container('v', "ay") < 0) return false;
            if (!append_byte_list(v)) return false;
            return close_container() >= 0;
        } else {
            return false;
        }
    }, var);
}

bool Message::read_basic(char type, void* out) {
    if (!msg_) return false;
    return sd_bus_message_read_basic(msg_, type, out) > 0;
}

bool Message::read_bool(bool* out) {
    int b = 0;
    if (!read_basic('b', &b)) return false;
    if (out) *out = (b != 0);
    return true;
}

bool Message::read_byte(uint8_t* out) {
    return read_basic('y', out);
}

bool Message::read_int16(int16_t* out) {
    return read_basic('n', out);
}

bool Message::read_uint16(uint16_t* out) {
    return read_basic('q', out);
}

bool Message::read_int32(int32_t* out) {
    return read_basic('i', out);
}

bool Message::read_uint32(uint32_t* out) {
    return read_basic('u', out);
}

bool Message::read_int64(int64_t* out) {
    return read_basic('x', out);
}

bool Message::read_uint64(uint64_t* out) {
    return read_basic('t', out);
}

bool Message::read_double(double* out) {
    return read_basic('d', out);
}

bool Message::read_string(std::string* out) {
    const char* s = nullptr;
    if (!read_basic('s', &s)) return false;
    if (out) *out = (s ? s : "");
    return true;
}

bool Message::read_object_path(std::string* out) {
    const char* o = nullptr;
    if (!read_basic('o', &o)) return false;
    if (out) *out = (o ? o : "");
    return true;
}

bool Message::read_object_path(ObjectPath* out) {
    std::string s;
    if (!read_object_path(&s)) return false;
    if (out) *out = ObjectPath(std::move(s));
    return true;
}

bool Message::read_unix_fd(int* out) {
    int fd = -1;
    if (!read_basic('h', &fd)) return false;
    if (out) *out = fd;
    return true;
}

bool Message::read_unix_fd(UnixFd* out) {
    int fd = -1;
    if (!read_unix_fd(&fd)) return false;
    if (out) *out = UnixFd(fd);
    return true;
}

bool Message::read_string_list(std::vector<std::string>* out) {
    if (enter_container('a', "s") < 0) return false;
    if (out) out->clear();
    while (!at_end()) {
        std::string s;
        if (!read_string(&s)) {
            exit_container();
            return false;
        }
        if (out) out->push_back(std::move(s));
    }
    return exit_container() >= 0;
}

bool Message::read_byte_list(std::vector<uint8_t>* out) {
    if (enter_container('a', "y") < 0) return false;
    if (out) out->clear();
    while (!at_end()) {
        uint8_t b = 0;
        if (!read_byte(&b)) {
            exit_container();
            return false;
        }
        if (out) out->push_back(b);
    }
    return exit_container() >= 0;
}

bool Message::read_variant(PropertyValue* out) {
    if (enter_container('v', nullptr) < 0) return false;
    char type = 0;
    const char* contents = nullptr;
    if (peek_type(&type, &contents) < 0) {
        exit_container();
        return false;
    }

    bool ok = false;
    if (type == 'b') {
        bool b = false;
        if ((ok = read_bool(&b)) && out) *out = PropertyValue(b);
    } else if (type == 'y') {
        uint8_t y = 0;
        if ((ok = read_byte(&y)) && out) *out = PropertyValue(y);
    } else if (type == 'n') {
        int16_t n = 0;
        if ((ok = read_int16(&n)) && out) *out = PropertyValue(n);
    } else if (type == 'q') {
        uint16_t q = 0;
        if ((ok = read_uint16(&q)) && out) *out = PropertyValue(q);
    } else if (type == 'i') {
        int32_t i = 0;
        if ((ok = read_int32(&i)) && out) *out = PropertyValue(i);
    } else if (type == 'u') {
        uint32_t u = 0;
        if ((ok = read_uint32(&u)) && out) *out = PropertyValue(u);
    } else if (type == 'x') {
        int64_t x = 0;
        if ((ok = read_int64(&x)) && out) *out = PropertyValue(x);
    } else if (type == 't') {
        uint64_t t = 0;
        if ((ok = read_uint64(&t)) && out) *out = PropertyValue(t);
    } else if (type == 'd') {
        double d = 0.0;
        if ((ok = read_double(&d)) && out) *out = PropertyValue(d);
    } else if (type == 's') {
        std::string s;
        if ((ok = read_string(&s)) && out) *out = PropertyValue(std::move(s));
    } else if (type == 'o') {
        ObjectPath o;
        if ((ok = read_object_path(&o)) && out) *out = PropertyValue(std::move(o));
    } else if (type == 'h') {
        UnixFd h;
        if ((ok = read_unix_fd(&h)) && out) *out = PropertyValue(h);
    } else if (type == 'a' && contents && std::strcmp(contents, "s") == 0) {
        std::vector<std::string> list;
        if ((ok = read_string_list(&list)) && out) *out = PropertyValue(std::move(list));
    } else if (type == 'a' && contents && std::strcmp(contents, "y") == 0) {
        std::vector<uint8_t> bytes;
        if ((ok = read_byte_list(&bytes)) && out) *out = PropertyValue(std::move(bytes));
    } else {
        // Unknown or unsupported variant, record as empty
        ok = true;
        if (out) *out = PropertyValue();
    }

    if (!ok) {
        exit_container();
        return false;
    }
    return exit_container() >= 0;
}

Message Message::new_method_return(std::string* error) const {
    if (!msg_) {
        if (error) *error = "null message";
        return Message();
    }
    sd_bus_message* rep = nullptr;
    int r = sd_bus_message_new_method_return(msg_, &rep);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Message();
    }
    return Message(rep, true);
}

Message Message::new_method_error(const Error& err, std::string* out_err) const {
    if (!msg_) {
        if (out_err) *out_err = "null message";
        return Message();
    }
    sd_bus_message* rep = nullptr;
    int r = sd_bus_message_new_method_error(msg_, &rep, err.raw());
    if (r < 0) {
        if (out_err) *out_err = strerror(-r);
        return Message();
    }
    return Message(rep, true);
}

Message Message::new_method_error(const std::string& name, const std::string& message, std::string* out_err) const {
    if (!msg_) {
        if (out_err) *out_err = "null message";
        return Message();
    }
    sd_bus_error sdbus_err = SD_BUS_ERROR_NULL;
    sd_bus_error_set(&sdbus_err, name.c_str(), message.c_str());
    sd_bus_message* rep = nullptr;
    int r = sd_bus_message_new_method_error(msg_, &rep, &sdbus_err);
    sd_bus_error_free(&sdbus_err);
    if (r < 0) {
        if (out_err) *out_err = strerror(-r);
        return Message();
    }
    return Message(rep, true);
}

#endif  // __linux__

}  // namespace brodbus
