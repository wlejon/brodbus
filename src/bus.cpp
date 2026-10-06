#include "brodbus/bus.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <utility>

namespace brodbus {

#if defined(__linux__)

Bus::~Bus() {
    reset();
}

Bus::Bus(Bus&& other) noexcept : bus_(other.bus_) {
    other.bus_ = nullptr;
}

Bus& Bus::operator=(Bus&& other) noexcept {
    if (this != &other) {
        reset();
        bus_ = other.bus_;
        other.bus_ = nullptr;
    }
    return *this;
}

void Bus::reset(sd_bus* bus) noexcept {
    if (bus_) {
        sd_bus_flush_close_unref(bus_);
    }
    bus_ = bus;
}

sd_bus* Bus::release() noexcept {
    sd_bus* b = bus_;
    bus_ = nullptr;
    return b;
}

std::unique_ptr<Bus> Bus::open_system(std::string* error) {
    sd_bus* raw_bus = nullptr;
    int r = sd_bus_open_system(&raw_bus);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return nullptr;
    }
    return std::make_unique<Bus>(raw_bus);
}

std::unique_ptr<Bus> Bus::open_user(std::string* error) {
    sd_bus* raw_bus = nullptr;
    int r = sd_bus_open_user(&raw_bus);
    if (r >= 0 && raw_bus) {
        return std::make_unique<Bus>(raw_bus);
    }

    // Fallback to /run/user/<uid>/bus
    std::string user_bus = "/run/user/" + std::to_string(getuid()) + "/bus";
    if (access(user_bus.c_str(), R_OK | W_OK) == 0) {
        return open_address("unix:path=" + user_bus, error);
    }

    if (error) {
        *error = strerror(-r);
    }
    return nullptr;
}

std::unique_ptr<Bus> Bus::open_address(const std::string& address, std::string* error) {
    sd_bus* raw_bus = nullptr;
    int r = sd_bus_new(&raw_bus);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return nullptr;
    }
    r = sd_bus_set_address(raw_bus, address.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(raw_bus);
        return nullptr;
    }
    // IMPORTANT: open_address MUST set sd_bus_set_bus_client(bus, 1) to send Hello
    // on private message buses before sd_bus_start(bus).
    r = sd_bus_set_bus_client(raw_bus, 1);
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(raw_bus);
        return nullptr;
    }
    r = sd_bus_start(raw_bus);
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(raw_bus);
        return nullptr;
    }
    return std::make_unique<Bus>(raw_bus);
}

int Bus::get_fd() const noexcept {
    return bus_ ? sd_bus_get_fd(bus_) : -1;
}

int Bus::process() {
    return bus_ ? sd_bus_process(bus_, nullptr) : -1;
}

int Bus::wait(uint64_t timeout_usec) {
    return bus_ ? sd_bus_wait(bus_, timeout_usec) : -1;
}

int Bus::flush() {
    return bus_ ? sd_bus_flush(bus_) : -1;
}

std::string Bus::unique_name() const {
    if (!bus_) return {};
    const char* name = nullptr;
    if (sd_bus_get_unique_name(bus_, &name) < 0 || !name) return {};
    return name;
}

bool Bus::request_name(const std::string& name, uint64_t flags, std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return false;
    }
    int r = sd_bus_request_name(bus_, name.c_str(), flags);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return false;
    }
    return true;
}

bool Bus::release_name(const std::string& name, std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return false;
    }
    int r = sd_bus_release_name(bus_, name.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return false;
    }
    return true;
}

namespace {
struct MatchContext {
    Bus::SignalCallback callback;
    Bus::RawSignalCallback raw_callback;
};

int on_match_signal(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* ctx = static_cast<MatchContext*>(userdata);
    if (ctx) {
        if (ctx->callback) {
            Message msg(m, false);
            ctx->callback(msg);
        }
        if (ctx->raw_callback) {
            ctx->raw_callback(m);
        }
    }
    return 0;
}
}  // namespace

Slot Bus::add_match(const std::string& match_rule, SignalCallback callback, std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return Slot();
    }
    auto ctx = std::make_unique<MatchContext>();
    ctx->callback = std::move(callback);

    sd_bus_slot* slot = nullptr;
    int r = sd_bus_add_match(bus_, &slot, match_rule.c_str(), on_match_signal, ctx.get());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Slot();
    }

    sd_bus_slot_set_destroy_callback(slot, [](void* ud) {
        delete static_cast<MatchContext*>(ud);
    });
    ctx.release();
    return Slot(slot);
}

Slot Bus::add_match(const std::string& match_rule, RawSignalCallback callback, std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return Slot();
    }
    auto ctx = std::make_unique<MatchContext>();
    ctx->raw_callback = std::move(callback);

    sd_bus_slot* slot = nullptr;
    int r = sd_bus_add_match(bus_, &slot, match_rule.c_str(), on_match_signal, ctx.get());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Slot();
    }

    sd_bus_slot_set_destroy_callback(slot, [](void* ud) {
        delete static_cast<MatchContext*>(ud);
    });
    ctx.release();
    return Slot(slot);
}

Message Bus::new_method_call(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return Message();
    }
    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(bus_, &m, destination.c_str(), path.c_str(), interface.c_str(), member.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Message();
    }
    return Message(m, true);
}

Message Bus::new_signal(
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return Message();
    }
    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_signal(bus_, &m, path.c_str(), interface.c_str(), member.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Message();
    }
    return Message(m, true);
}

Message Bus::call(Message& message, uint64_t timeout_usec, Error* error) {
    if (!bus_ || !message.raw()) return Message();
    sd_bus_error sdbus_err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    int r = sd_bus_call(bus_, message.raw(), timeout_usec, &sdbus_err, &reply);
    if (r < 0) {
        if (error) {
            *error = Error(sdbus_err);
        } else {
            sd_bus_error_free(&sdbus_err);
        }
        return Message();
    }
    sd_bus_error_free(&sdbus_err);
    return Message(reply, true);
}

bool Bus::send(Message& message, uint64_t* serial, std::string* error) {
    if (!bus_ || !message.raw()) {
        if (error) *error = "invalid bus or message";
        return false;
    }
    int r = sd_bus_send(bus_, message.raw(), serial);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return false;
    }
    return true;
}

bool Bus::emit_signal(
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    std::function<void(Message&)> build_args,
    std::string* error) {
    Message msg = new_signal(path, interface, member, error);
    if (!msg) return false;
    if (build_args) {
        build_args(msg);
    }
    return send(msg, nullptr, error);
}

bool Bus::call_method(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    std::function<void(Message&)> build_args,
    std::function<void(Message&)> parse_reply,
    std::string* error,
    uint64_t timeout_usec) {
    Message msg = new_method_call(destination, path, interface, member, error);
    if (!msg) return false;
    if (build_args) {
        build_args(msg);
    }
    Error err;
    Message reply = call(msg, timeout_usec, &err);
    if (!reply) {
        if (error) *error = err.to_string();
        return false;
    }
    if (parse_reply) {
        parse_reply(reply);
    }
    return true;
}

bool Bus::get_property_bool(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    bool* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'b', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = (val != 0);
    return true;
}

bool Bus::get_property_byte(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint8_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    uint8_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'y', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_int16(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    int16_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int16_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'n', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_uint16(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint16_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    uint16_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'q', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_int32(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    int32_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int32_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'i', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_uint32(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint32_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    uint32_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'u', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_int64(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    int64_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int64_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'x', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_uint64(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint64_t* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    uint64_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 't', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_double(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    double* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    double val = 0.0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'd', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::get_property_string(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    std::string* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    char* str = nullptr;
    int r = sd_bus_get_property_string(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, &str);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = str ? str : "";
    free(str);
    return true;
}

bool Bus::get_property_strv(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    std::vector<std::string>* out,
    std::string* error) {
    if (!bus_ || !out) {
        if (error) *error = "null bus or output";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    char** strv = nullptr;
    int r = sd_bus_get_property_strv(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, &strv);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    out->clear();
    if (strv) {
        for (char** p = strv; *p; ++p) {
            out->push_back(*p);
            free(*p);
        }
        free(strv);
    }
    return true;
}

bool Bus::set_property_bool(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    bool val,
    std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_set_property(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, "b", val ? 1 : 0);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    return true;
}

bool Bus::set_property_string(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    const std::string& val,
    std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_set_property(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, "s", val.c_str());
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    return true;
}

bool Bus::set_property_uint32(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint32_t val,
    std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not connected";
        return false;
    }
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_set_property(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, "u", val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    return true;
}

#endif  // __linux__

}  // namespace brodbus
