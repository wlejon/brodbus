#include "brodbus/property_cache.h"

#include <utility>

namespace brodbus {

#if defined(__linux__)

PropertyCache::PropertyCache(
    Bus& bus,
    std::string destination,
    std::string path,
    std::string interface,
    bool auto_refresh)
    : bus_(&bus),
      destination_(std::move(destination)),
      path_(std::move(path)),
      interface_(std::move(interface)) {
    setup_match();
    if (auto_refresh) {
        refresh();
    }
}

PropertyCache::PropertyCache(PropertyCache&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.mutex_);
    bus_ = other.bus_;
    destination_ = std::move(other.destination_);
    path_ = std::move(other.path_);
    interface_ = std::move(other.interface_);
    slot_ = std::move(other.slot_);
    properties_ = std::move(other.properties_);
    change_callbacks_ = std::move(other.change_callbacks_);
    invalidate_callbacks_ = std::move(other.invalidate_callbacks_);
    other.bus_ = nullptr;
}

PropertyCache& PropertyCache::operator=(PropertyCache&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        bus_ = other.bus_;
        destination_ = std::move(other.destination_);
        path_ = std::move(other.path_);
        interface_ = std::move(other.interface_);
        slot_ = std::move(other.slot_);
        properties_ = std::move(other.properties_);
        change_callbacks_ = std::move(other.change_callbacks_);
        invalidate_callbacks_ = std::move(other.invalidate_callbacks_);
        other.bus_ = nullptr;
    }
    return *this;
}

void PropertyCache::setup_match() {
    if (!bus_ || path_.empty() || interface_.empty()) return;
    std::string rule = "type='signal',path='" + path_ +
                       "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',arg0='" +
                       interface_ + "'";
    std::string err;
    slot_ = bus_->add_match(rule, [this](Message& msg) {
        handle_properties_changed(msg);
    }, &err);
}

bool PropertyCache::refresh(std::string* error) {
    if (!bus_) {
        if (error) *error = "no bus connected";
        return false;
    }

    Message call_msg = bus_->new_method_call(
        destination_, path_, "org.freedesktop.DBus.Properties", "GetAll", error);
    if (!call_msg) return false;

    if (!call_msg.append_string(interface_)) {
        if (error) *error = "failed to append interface name";
        return false;
    }

    Error err;
    Message reply = bus_->call(call_msg, 5000000, &err);
    if (!reply) {
        if (error) *error = err.to_string();
        return false;
    }

    std::map<std::string, PropertyValue> new_props;
    if (reply.enter_container('a', "{sv}") >= 0) {
        while (!reply.at_end()) {
            if (reply.enter_container('e', "sv") >= 0) {
                std::string key;
                if (reply.read_string(&key)) {
                    PropertyValue val;
                    if (reply.read_variant(&val)) {
                        new_props[key] = std::move(val);
                    }
                }
                reply.exit_container();
            } else {
                break;
            }
        }
        reply.exit_container();
    }

    std::vector<std::pair<std::string, PropertyValue>> changed;
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [k, v] : new_props) {
            auto it = properties_.find(k);
            if (it == properties_.end() || !(it->second == v)) {
                changed.emplace_back(k, v);
            }
        }
        properties_ = std::move(new_props);
        callbacks = change_callbacks_;
    }

    for (const auto& [k, v] : changed) {
        for (const auto& cb : callbacks) {
            cb(k, v);
        }
    }

    return true;
}

void PropertyCache::handle_properties_changed(Message& msg) {
    std::string iface;
    if (!msg.read_string(&iface)) return;
    if (iface != interface_) return;

    std::vector<std::pair<std::string, PropertyValue>> changed;
    if (msg.enter_container('a', "{sv}") >= 0) {
        while (!msg.at_end()) {
            if (msg.enter_container('e', "sv") >= 0) {
                std::string key;
                if (msg.read_string(&key)) {
                    PropertyValue val;
                    if (msg.read_variant(&val)) {
                        changed.emplace_back(key, std::move(val));
                    }
                }
                msg.exit_container();
            } else {
                break;
            }
        }
        msg.exit_container();
    }

    std::vector<std::string> invalidated;
    if (msg.enter_container('a', "s") >= 0) {
        while (!msg.at_end()) {
            std::string key;
            if (msg.read_string(&key)) {
                invalidated.push_back(std::move(key));
            }
        }
        msg.exit_container();
    }

    std::vector<ChangeCallback> cbs;
    std::vector<InvalidateCallback> inv_cbs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [k, v] : changed) {
            properties_[k] = v;
        }
        for (const auto& k : invalidated) {
            properties_.erase(k);
        }
        cbs = change_callbacks_;
        inv_cbs = invalidate_callbacks_;
    }

    for (const auto& [k, v] : changed) {
        for (const auto& cb : cbs) {
            cb(k, v);
        }
    }

    for (const auto& k : invalidated) {
        for (const auto& cb : inv_cbs) {
            cb(k);
        }
    }
}

bool PropertyCache::has_property(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.find(name) != properties_.end();
}

std::optional<PropertyValue> PropertyCache::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = properties_.find(name);
    if (it != properties_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t PropertyCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.size();
}

std::map<std::string, PropertyValue> PropertyCache::all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_;
}

bool PropertyCache::get_bool(const std::string& name, bool default_val) const {
    auto val = get(name);
    return val ? val->as_bool(default_val) : default_val;
}

std::string PropertyCache::get_string(const std::string& name, const std::string& default_val) const {
    auto val = get(name);
    return val ? val->as_string(default_val) : default_val;
}

uint32_t PropertyCache::get_uint32(const std::string& name, uint32_t default_val) const {
    auto val = get(name);
    return val ? val->as_uint32(default_val) : default_val;
}

int32_t PropertyCache::get_int32(const std::string& name, int32_t default_val) const {
    auto val = get(name);
    return val ? val->as_int32(default_val) : default_val;
}

uint64_t PropertyCache::get_uint64(const std::string& name, uint64_t default_val) const {
    auto val = get(name);
    return val ? val->as_uint64(default_val) : default_val;
}

int64_t PropertyCache::get_int64(const std::string& name, int64_t default_val) const {
    auto val = get(name);
    return val ? val->as_int64(default_val) : default_val;
}

double PropertyCache::get_double(const std::string& name, double default_val) const {
    auto val = get(name);
    return val ? val->as_double(default_val) : default_val;
}

std::vector<std::string> PropertyCache::get_string_list(const std::string& name) const {
    auto val = get(name);
    return val ? val->as_string_list() : std::vector<std::string>{};
}

std::vector<uint8_t> PropertyCache::get_byte_list(const std::string& name) const {
    auto val = get(name);
    return val ? val->as_byte_list() : std::vector<uint8_t>{};
}

void PropertyCache::on_change(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callbacks_.push_back(std::move(cb));
}

void PropertyCache::on_invalidate(InvalidateCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    invalidate_callbacks_.push_back(std::move(cb));
}

void PropertyCache::set_cached_property(const std::string& name, PropertyValue value) {
    std::vector<ChangeCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        properties_[name] = value;
        cbs = change_callbacks_;
    }
    for (const auto& cb : cbs) {
        cb(name, value);
    }
}

#endif  // __linux__

}  // namespace brodbus
