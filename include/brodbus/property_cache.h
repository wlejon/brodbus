#pragma once

#include "brodbus/bus.h"
#include "brodbus/message.h"
#include "brodbus/slot.h"
#include "brodbus/types.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace brodbus {

class PropertyCache {
public:
    using ChangeCallback = std::function<void(const std::string& name, const PropertyValue& value)>;
    using InvalidateCallback = std::function<void(const std::string& name)>;

    PropertyCache() = default;
    PropertyCache(Bus& bus,
                  std::string destination,
                  std::string path,
                  std::string interface,
                  bool auto_refresh = true);
    ~PropertyCache() = default;

    PropertyCache(const PropertyCache&) = delete;
    PropertyCache& operator=(const PropertyCache&) = delete;

    PropertyCache(PropertyCache&& other) noexcept;
    PropertyCache& operator=(PropertyCache&& other) noexcept;

    // Refresh properties via GetAll
    bool refresh(std::string* error = nullptr);

    // Queries
    bool has_property(const std::string& name) const;
    std::optional<PropertyValue> get(const std::string& name) const;
    size_t size() const;
    std::map<std::string, PropertyValue> all() const;

    // Typed getters
    bool get_bool(const std::string& name, bool default_val = false) const;
    std::string get_string(const std::string& name, const std::string& default_val = "") const;
    uint32_t get_uint32(const std::string& name, uint32_t default_val = 0) const;
    int32_t get_int32(const std::string& name, int32_t default_val = 0) const;
    uint64_t get_uint64(const std::string& name, uint64_t default_val = 0) const;
    int64_t get_int64(const std::string& name, int64_t default_val = 0) const;
    double get_double(const std::string& name, double default_val = 0.0) const;
    std::vector<std::string> get_string_list(const std::string& name) const;
    std::vector<uint8_t> get_byte_list(const std::string& name) const;

    // Callbacks
    void on_change(ChangeCallback cb);
    void on_invalidate(InvalidateCallback cb);

    // Accessors
    const std::string& destination() const noexcept { return destination_; }
    const std::string& path() const noexcept { return path_; }
    const std::string& interface() const noexcept { return interface_; }
    bool is_valid() const noexcept { return bus_ != nullptr && slot_.is_valid(); }
    explicit operator bool() const noexcept { return is_valid(); }

    // Manual update
    void set_cached_property(const std::string& name, PropertyValue value);

private:
    void setup_match();
    void handle_properties_changed(Message& msg);

    Bus* bus_ = nullptr;
    std::string destination_;
    std::string path_;
    std::string interface_;
    Slot slot_;

    mutable std::mutex mutex_;
    std::map<std::string, PropertyValue> properties_;
    std::vector<ChangeCallback> change_callbacks_;
    std::vector<InvalidateCallback> invalidate_callbacks_;
};

}  // namespace brodbus
