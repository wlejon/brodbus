#pragma once

#if defined(__linux__)
#include <systemd/sd-bus.h>
#else
struct sd_bus_message;
struct sd_bus_error;
#endif

#include "brodbus/error.h"
#include "brodbus/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace brodbus {

class Message {
public:
    Message() noexcept : msg_(nullptr), owned_(true) {}
    explicit Message(sd_bus_message* msg, bool owned = true) noexcept
        : msg_(msg), owned_(owned) {}
    ~Message();

    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;

    Message(Message&& other) noexcept;
    Message& operator=(Message&& other) noexcept;

    void reset(sd_bus_message* msg = nullptr, bool owned = true) noexcept;
    sd_bus_message* release() noexcept;

    sd_bus_message* raw() const noexcept { return msg_; }
    bool is_valid() const noexcept { return msg_ != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }

    // Header inspection
    std::string get_path() const;
    std::string get_interface() const;
    std::string get_member() const;
    std::string get_sender() const;
    std::string get_destination() const;
    std::string get_signature() const;
    uint8_t get_type() const;

    bool is_signal(const std::string& interface, const std::string& member) const;
    bool is_method_call(const std::string& interface, const std::string& member) const;
    bool is_method_error(const char* name = nullptr) const;
    const sd_bus_error* get_error() const;
    int get_errno() const;

    // Container operations
    int open_container(char type, const char* contents = nullptr);
    int close_container();
    int enter_container(char type, const char* contents = nullptr);
    int exit_container();
    bool at_end(bool complete = false) const;
    int peek_type(char* type = nullptr, const char** contents = nullptr) const;
    int rewind(bool complete = false);
    int seal(uint64_t cookie = 1, uint64_t timeout_usec = 0);

    // Appending basic types
    bool append_basic(char type, const void* value);
    bool append_bool(bool val);
    bool append_byte(uint8_t val);
    bool append_int16(int16_t val);
    bool append_uint16(uint16_t val);
    bool append_int32(int32_t val);
    bool append_uint32(uint32_t val);
    bool append_int64(int64_t val);
    bool append_uint64(uint64_t val);
    bool append_double(double val);
    bool append_string(const std::string& val);
    bool append_string(const char* val);
    bool append_object_path(const std::string& val);
    bool append_object_path(const ObjectPath& val);
    bool append_unix_fd(int fd);
    bool append_unix_fd(const UnixFd& val);

    // Appending containers & variants
    bool append_string_list(const std::vector<std::string>& list);
    bool append_byte_list(const std::vector<uint8_t>& bytes);
    bool append_variant(const PropertyValue& val);

    // Reading basic types
    bool read_basic(char type, void* out);
    bool read_bool(bool* out);
    bool read_byte(uint8_t* out);
    bool read_int16(int16_t* out);
    bool read_uint16(uint16_t* out);
    bool read_int32(int32_t* out);
    bool read_uint32(uint32_t* out);
    bool read_int64(int64_t* out);
    bool read_uint64(uint64_t* out);
    bool read_double(double* out);
    bool read_string(std::string* out);
    bool read_object_path(std::string* out);
    bool read_object_path(ObjectPath* out);
    bool read_unix_fd(int* out);
    bool read_unix_fd(UnixFd* out);

    // Reading containers & variants
    bool read_string_list(std::vector<std::string>* out);
    bool read_byte_list(std::vector<uint8_t>* out);
    bool read_variant(PropertyValue* out);

    // Reply creation
    Message new_method_return(std::string* error = nullptr) const;
    Message new_method_error(const Error& error, std::string* out_err = nullptr) const;
    Message new_method_error(const std::string& name, const std::string& message, std::string* out_err = nullptr) const;

private:
    sd_bus_message* msg_ = nullptr;
    bool owned_ = true;
};

}  // namespace brodbus
