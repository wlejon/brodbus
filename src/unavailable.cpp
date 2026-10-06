#include "brodbus/brodbus.h"

#if !defined(__linux__)

namespace brodbus {

// --- Error ---

Error::Error() noexcept : error_(SD_BUS_ERROR_NULL) {}
Error::Error(sd_bus_error err) noexcept : error_(err) {}
Error::~Error() { reset(); }

Error::Error(Error&& other) noexcept : error_(other.error_) {
    other.error_ = SD_BUS_ERROR_NULL;
}

Error& Error::operator=(Error&& other) noexcept {
    if (this != &other) {
        reset();
        error_ = other.error_;
        other.error_ = SD_BUS_ERROR_NULL;
    }
    return *this;
}

void Error::reset() noexcept {
    error_ = SD_BUS_ERROR_NULL;
}

Error Error::create(const std::string&, const std::string&) {
    return Error();
}

Error Error::from_errno(int) {
    return Error();
}

bool Error::is_set() const noexcept {
    return error_.name != nullptr;
}

const char* Error::name() const noexcept {
    return error_.name ? error_.name : "";
}

const char* Error::message() const noexcept {
    return error_.message ? error_.message : "";
}

int Error::get_errno() const noexcept {
    return 0;
}

std::string Error::to_string() const {
    if (!is_set()) return {};
    const char* n = name();
    const char* m = message();
    if (n && *n && m && *m) return std::string(n) + ": " + m;
    if (n && *n) return n;
    if (m && *m) return m;
    return {};
}

// --- Slot ---

Slot::~Slot() { reset(); }

Slot::Slot(Slot&& other) noexcept : slot_(other.slot_) {
    other.slot_ = nullptr;
}

Slot& Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        other.slot_ = nullptr;
    }
    return *this;
}

void Slot::reset(sd_bus_slot* slot) noexcept {
    slot_ = slot;
}

sd_bus_slot* Slot::release() noexcept {
    sd_bus_slot* s = slot_;
    slot_ = nullptr;
    return s;
}

// --- Message ---

Message::~Message() { reset(); }

Message::Message(Message&& other) noexcept
    : msg_(other.msg_), owned_(other.owned_) {
    other.msg_ = nullptr;
    other.owned_ = false;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        reset();
        msg_ = other.msg_;
        owned_ = other.owned_;
        other.msg_ = nullptr;
        other.owned_ = false;
    }
    return *this;
}

void Message::reset(sd_bus_message* msg, bool owned) noexcept {
    msg_ = msg;
    owned_ = owned;
}

sd_bus_message* Message::release() noexcept {
    sd_bus_message* m = msg_;
    msg_ = nullptr;
    owned_ = false;
    return m;
}

std::string Message::get_path() const { return {}; }
std::string Message::get_interface() const { return {}; }
std::string Message::get_member() const { return {}; }
std::string Message::get_sender() const { return {}; }
std::string Message::get_destination() const { return {}; }
std::string Message::get_signature() const { return {}; }
uint8_t Message::get_type() const { return 0; }
bool Message::is_signal(const std::string&, const std::string&) const { return false; }
bool Message::is_method_call(const std::string&, const std::string&) const { return false; }
bool Message::is_method_error(const char*) const { return false; }
const sd_bus_error* Message::get_error() const { return nullptr; }
int Message::get_errno() const { return -1; }

int Message::open_container(char, const char*) { return -1; }
int Message::close_container() { return -1; }
int Message::enter_container(char, const char*) { return -1; }
int Message::exit_container() { return -1; }
bool Message::at_end(bool) const { return true; }
int Message::peek_type(char*, const char**) const { return -1; }
int Message::rewind(bool) { return -1; }
int Message::seal(uint64_t, uint64_t) { return -1; }

bool Message::append_basic(char, const void*) { return false; }
bool Message::append_bool(bool) { return false; }
bool Message::append_byte(uint8_t) { return false; }
bool Message::append_int16(int16_t) { return false; }
bool Message::append_uint16(uint16_t) { return false; }
bool Message::append_int32(int32_t) { return false; }
bool Message::append_uint32(uint32_t) { return false; }
bool Message::append_int64(int64_t) { return false; }
bool Message::append_uint64(uint64_t) { return false; }
bool Message::append_double(double) { return false; }
bool Message::append_string(const std::string&) { return false; }
bool Message::append_string(const char*) { return false; }
bool Message::append_object_path(const std::string&) { return false; }
bool Message::append_object_path(const ObjectPath&) { return false; }
bool Message::append_unix_fd(int) { return false; }
bool Message::append_unix_fd(const UnixFd&) { return false; }
bool Message::append_string_list(const std::vector<std::string>&) { return false; }
bool Message::append_byte_list(const std::vector<uint8_t>&) { return false; }
bool Message::append_variant(const PropertyValue&) { return false; }

bool Message::read_basic(char, void*) { return false; }
bool Message::read_bool(bool*) { return false; }
bool Message::read_byte(uint8_t*) { return false; }
bool Message::read_int16(int16_t*) { return false; }
bool Message::read_uint16(uint16_t*) { return false; }
bool Message::read_int32(int32_t*) { return false; }
bool Message::read_uint32(uint32_t*) { return false; }
bool Message::read_int64(int64_t*) { return false; }
bool Message::read_uint64(uint64_t*) { return false; }
bool Message::read_double(double*) { return false; }
bool Message::read_string(std::string*) { return false; }
bool Message::read_object_path(std::string*) { return false; }
bool Message::read_object_path(ObjectPath*) { return false; }
bool Message::read_unix_fd(int*) { return false; }
bool Message::read_unix_fd(UnixFd*) { return false; }
bool Message::read_string_list(std::vector<std::string>*) { return false; }
bool Message::read_byte_list(std::vector<uint8_t>*) { return false; }
bool Message::read_variant(PropertyValue*) { return false; }

Message Message::new_method_return(std::string* error) const {
    if (error) *error = "D-Bus is only supported on Linux";
    return Message();
}

Message Message::new_method_error(const Error&, std::string* error) const {
    if (error) *error = "D-Bus is only supported on Linux";
    return Message();
}

Message Message::new_method_error(const std::string&, const std::string&, std::string* error) const {
    if (error) *error = "D-Bus is only supported on Linux";
    return Message();
}

// --- Bus ---

static const char* kNoDbusReason = "D-Bus is only supported on Linux";

Bus::~Bus() { reset(); }

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
    bus_ = bus;
}

sd_bus* Bus::release() noexcept {
    sd_bus* b = bus_;
    bus_ = nullptr;
    return b;
}

std::unique_ptr<Bus> Bus::open_system(std::string* error) {
    if (error) *error = kNoDbusReason;
    return nullptr;
}

std::unique_ptr<Bus> Bus::open_user(std::string* error) {
    if (error) *error = kNoDbusReason;
    return nullptr;
}

std::unique_ptr<Bus> Bus::open_address(const std::string&, std::string* error) {
    if (error) *error = kNoDbusReason;
    return nullptr;
}

int Bus::get_fd() const noexcept { return -1; }
int Bus::process() { return -1; }
int Bus::wait(uint64_t) { return -1; }
int Bus::flush() { return -1; }
std::string Bus::unique_name() const { return {}; }

bool Bus::request_name(const std::string&, uint64_t, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool Bus::release_name(const std::string&, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

Slot Bus::add_match(const std::string&, SignalCallback, std::string* error) {
    if (error) *error = kNoDbusReason;
    return Slot();
}

Slot Bus::add_match(const std::string&, RawSignalCallback, std::string* error) {
    if (error) *error = kNoDbusReason;
    return Slot();
}

Message Bus::new_method_call(const std::string&, const std::string&, const std::string&, const std::string&, std::string* error) {
    if (error) *error = kNoDbusReason;
    return Message();
}

Message Bus::new_signal(const std::string&, const std::string&, const std::string&, std::string* error) {
    if (error) *error = kNoDbusReason;
    return Message();
}

Message Bus::call(Message&, uint64_t, Error*) { return Message(); }
bool Bus::send(Message&, uint64_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool Bus::emit_signal(const std::string&, const std::string&, const std::string&, std::function<void(Message&)>, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool Bus::call_method(const std::string&, const std::string&, const std::string&, const std::string&,
                      std::function<void(Message&)>, std::function<void(Message&)>, std::string* error, uint64_t) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool Bus::get_property_bool(const std::string&, const std::string&, const std::string&, const std::string&, bool*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_byte(const std::string&, const std::string&, const std::string&, const std::string&, uint8_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_int16(const std::string&, const std::string&, const std::string&, const std::string&, int16_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_uint16(const std::string&, const std::string&, const std::string&, const std::string&, uint16_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_int32(const std::string&, const std::string&, const std::string&, const std::string&, int32_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_uint32(const std::string&, const std::string&, const std::string&, const std::string&, uint32_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_int64(const std::string&, const std::string&, const std::string&, const std::string&, int64_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_uint64(const std::string&, const std::string&, const std::string&, const std::string&, uint64_t*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_double(const std::string&, const std::string&, const std::string&, const std::string&, double*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_string(const std::string&, const std::string&, const std::string&, const std::string&, std::string*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::get_property_strv(const std::string&, const std::string&, const std::string&, const std::string&, std::vector<std::string>*, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool Bus::set_property_bool(const std::string&, const std::string&, const std::string&, const std::string&, bool, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::set_property_string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}
bool Bus::set_property_uint32(const std::string&, const std::string&, const std::string&, const std::string&, uint32_t, std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

// --- PropertyCache ---

PropertyCache::PropertyCache(Bus& bus, std::string destination, std::string path, std::string interface, bool)
    : bus_(&bus), destination_(std::move(destination)), path_(std::move(path)), interface_(std::move(interface)) {}

PropertyCache::PropertyCache(PropertyCache&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.mutex_);
    bus_ = other.bus_;
    destination_ = std::move(other.destination_);
    path_ = std::move(other.path_);
    interface_ = std::move(other.interface_);
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
        properties_ = std::move(other.properties_);
        change_callbacks_ = std::move(other.change_callbacks_);
        invalidate_callbacks_ = std::move(other.invalidate_callbacks_);
        other.bus_ = nullptr;
    }
    return *this;
}

void PropertyCache::setup_match() {}
void PropertyCache::handle_properties_changed(Message&) {}

bool PropertyCache::refresh(std::string* error) {
    if (error) *error = kNoDbusReason;
    return false;
}

bool PropertyCache::has_property(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.find(name) != properties_.end();
}

std::optional<PropertyValue> PropertyCache::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = properties_.find(name);
    if (it != properties_.end()) return it->second;
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
    auto v = get(name);
    return v ? v->as_bool(default_val) : default_val;
}

std::string PropertyCache::get_string(const std::string& name, const std::string& default_val) const {
    auto v = get(name);
    return v ? v->as_string(default_val) : default_val;
}

uint32_t PropertyCache::get_uint32(const std::string& name, uint32_t default_val) const {
    auto v = get(name);
    return v ? v->as_uint32(default_val) : default_val;
}

int32_t PropertyCache::get_int32(const std::string& name, int32_t default_val) const {
    auto v = get(name);
    return v ? v->as_int32(default_val) : default_val;
}

uint64_t PropertyCache::get_uint64(const std::string& name, uint64_t default_val) const {
    auto v = get(name);
    return v ? v->as_uint64(default_val) : default_val;
}

int64_t PropertyCache::get_int64(const std::string& name, int64_t default_val) const {
    auto v = get(name);
    return v ? v->as_int64(default_val) : default_val;
}

double PropertyCache::get_double(const std::string& name, double default_val) const {
    auto v = get(name);
    return v ? v->as_double(default_val) : default_val;
}

std::vector<std::string> PropertyCache::get_string_list(const std::string& name) const {
    auto v = get(name);
    return v ? v->as_string_list() : std::vector<std::string>{};
}

std::vector<uint8_t> PropertyCache::get_byte_list(const std::string& name) const {
    auto v = get(name);
    return v ? v->as_byte_list() : std::vector<uint8_t>{};
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

// --- PrivateBus ---

PrivateBus::PrivateBus() {}
PrivateBus::~PrivateBus() {}

PrivateBus::PrivateBus(PrivateBus&& other) noexcept
    : address_(std::move(other.address_)), pid_(other.pid_) {
    other.pid_ = -1;
}

PrivateBus& PrivateBus::operator=(PrivateBus&& other) noexcept {
    if (this != &other) {
        address_ = std::move(other.address_);
        pid_ = other.pid_;
        other.pid_ = -1;
    }
    return *this;
}

void PrivateBus::stop() {}

}  // namespace brodbus

#endif  // !__linux__
