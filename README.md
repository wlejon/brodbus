# brodbus

[![CI](https://github.com/wlejon/brodbus/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/brodbus/actions/workflows/ci.yml)

A modern C++20 unified D-Bus layer for the bro desktop ecosystem.

`brodbus` unifies D-Bus connectivity, message serialization, signal matching,
property caching, and test fixtures across desktop libraries, eliminating
code duplication and preventing recurring edge cases like missing `Hello`
handshakes on private bus connections.

## Where it sits

Part of the **[bro](https://github.com/wlejon/bro)** desktop ecosystem (see the
[ecosystem architecture](https://github.com/wlejon/bro/blob/main/docs/ecosystem.md)).
Within the desktop stack, `brodbus` serves as the shared D-Bus substrate unifying
bus lifecycle, message codecs, and test doubles for the system and portal libraries:

- **[brosys](https://github.com/wlejon/brosys)**: System services (power, network, notifications, system tray).
- **[brocred](https://github.com/wlejon/brocred)**: Credential services (Secret Service provider and Polkit agent).
- **[broseat](https://github.com/wlejon/broseat)**: Seat and session lifecycle (logind session, seat device management, user manager units).
- **[broportal](https://github.com/wlejon/broportal)**: `xdg-desktop-portal` backend implementation (file picker, screenshot, screencast, remote desktop, settings, inhibitors, shortcuts).

## Platform support

`brodbus` is built primarily for **Linux** using `sd-bus` from `libsystemd >= 246`.
On Linux, it provides RAII connection management, signal matching, automatic
`Hello` handshakes on private bus connections, message container packing/unpacking,
and cached property tracking.

On **Windows and macOS**, the library provides clean stub implementations
(`src/unavailable.cpp`): all headers compile and link unconditionally, value
types work everywhere, and connection or bus entry points report clear failure
reasons (`"D-Bus is only supported on Linux"`). Downstream libraries can link
`brodbus::brodbus` across all platforms without requiring conditional compilation
or scattered `#ifdef` guards.

## Building

### Prerequisites

- **CMake 3.24+** and a **C++20** compiler (MSVC 2022+, GCC 12+, Clang 15+, Apple Clang).
- **Linux**: `libsystemd` (sd-bus >= 246) via pkg-config (`libsystemd-dev` on Debian/Ubuntu, `systemd-libs` on Arch).
  The test suite also requires `dbus-daemon` (`dbus-daemon` binary for private bus fixtures).
- **Windows / macOS**: No external libraries required.

### Standalone build

```bash
# Linux / macOS
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

# Windows (MSVC)
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake options:
- `BRODBUS_BUILD_TESTS`: Build tests (default `ON` when top-level, `OFF` when included via `add_subdirectory`).

### Consuming brodbus

Downstream projects consume the `brodbus::brodbus` CMake target. Ecosystem
consumers pin it with `bro_dependency()` (their copy of bro's
`cmake/bro_deps.cmake`): a target the outer project already added wins, else a
`../brodbus` working tree beside the top-level project, else the pinned commit,
fetched at configure (`-DFETCHCONTENT_SOURCE_DIR_BRODBUS=<path>` points at
another tree):

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)
bro_dependency(brodbus GITHUB wlejon/brodbus REF <40-hex sha> OPTIONS BRODBUS_BUILD_TESTS=OFF)

target_link_libraries(your_target PRIVATE brodbus::brodbus)
```

## API overview

```cpp
#include <brodbus/brodbus.h>

std::string err;

// Connect to the session bus (or open_system() / open_address())
auto bus = brodbus::Bus::open_user(&err);
if (!bus) { /* handle err */ }

// Acquire a well-known name
if (!bus->request_name("org.example.MyService", 0, &err)) { /* handle err */ }

// Listen for signals via RAII Slot
auto slot = bus->add_match(
    "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged'",
    [](brodbus::Message& msg) {
        std::string name, old_owner, new_owner;
        msg.read_string(&name);
        msg.read_string(&old_owner);
        msg.read_string(&new_owner);
    });

// Monitor properties with automatic caching and real-time updates
brodbus::PropertyCache cache(*bus, "org.freedesktop.systemd1",
                             "/org/freedesktop/systemd1",
                             "org.freedesktop.systemd1.Manager");
cache.refresh(&err);
std::string version = cache.get_string("Version");

// Pump event loop
while (running) {
    bus->wait(1000000);  // 1s timeout
    bus->process();
}
```

### Core classes

- **`Bus`**: RAII wrapper around `sd_bus*` (`sd_bus_flush_close_unref`).
  - `open_system()`: Connects to the system bus.
  - `open_user()`: Connects to the user session bus with automatic fallback to `/run/user/<uid>/bus`.
  - `open_address(address)`: Connects to arbitrary bus addresses (such as private test daemons), setting `sd_bus_set_bus_client(bus, 1)` to ensure the mandatory `Hello` handshake completes cleanly.
  - Event loop integration: `process()`, `wait()`, `flush()`, and `get_fd()` for poll/epoll integration.
  - Names: `request_name()`, `release_name()`, `unique_name()`.
  - Method calls & signals: `new_method_call()`, `new_signal()`, `call()`, `send()`, `emit_signal()`, `call_method()`.
  - Typed property access: `get_property_bool()`, `get_property_string()`, `get_property_uint32()`, `set_property_*()`, etc.
- **`Slot`**: RAII wrapper for `sd_bus_slot*` (`sd_bus_slot_unref`), managing signal match lifetime and dispatch callbacks.
- **`Message`**: RAII wrapper around `sd_bus_message*`.
  - Header inspection: `get_path()`, `get_interface()`, `get_member()`, `get_sender()`, `get_destination()`, `get_signature()`.
  - Basic types: `bool`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `double`, `std::string`, `ObjectPath`, `UnixFd`.
  - Container operations: `open_container()`, `close_container()`, `enter_container()`, `exit_container()`, `peek_type()`, `rewind()`, `at_end()`.
  - Collections & variants: `append_string_list()`, `read_string_list()`, `append_byte_list()`, `read_byte_list()`, `append_variant()`, `read_variant()`.
  - Replies: `new_method_return()`, `new_method_error()`.
- **`PropertyCache`**: Caches interface properties and automatically keeps them updated in real-time by listening to `org.freedesktop.DBus.Properties.PropertiesChanged` signals.
  - Queries: `has_property()`, `get()`, `all()`, typed getters (`get_bool()`, `get_string()`, `get_uint32()`, ...).
  - Callbacks: `on_change()` and `on_invalidate()`.
- **`Error`**: RAII wrapper around `sd_bus_error` (`sd_bus_error_free`) with `"Name: Message"` formatting and errno mapping.
- **`PrivateBus`**: Test fixture that spawns an isolated `dbus-daemon --session --nofork --nopidfile --print-address=1`, reads its socket address, provides environment variables (`DBUS_SESSION_BUS_ADDRESS`), and terminates cleanly on destruction via `SIGTERM` and `waitpid`.

| Header | Contents |
| :--- | :--- |
| `types.h` | `ObjectPath`, `UnixFd`, `PropertyValue` variant, type inspection |
| `error.h` | `Error`: RAII `sd_bus_error` management and formatting |
| `slot.h` | `Slot`: RAII `sd_bus_slot` signal match holder |
| `message.h` | `Message`: serialization, containers, typed reading/writing |
| `bus.h` | `Bus`: connection, event loop, names, calls, signals, properties |
| `property_cache.h` | `PropertyCache`: GetAll query, real-time property sync |
| `private_bus.h` | `PrivateBus`: isolated test daemon lifecycle |
| `brodbus.h` | Master umbrella header |

## Tests

Test assertions use `tests/check.h` (active in every configuration, no `assert()`).
A test that cannot run in the current environment exits code 77 with the reason
printed, and ctest reports it as skipped.

| Test | Platform | Target / Environment | Oracle |
|---|---|---|---|
| `test_types` | everywhere | in-process | Variant type storage, type queries, object path validation, unix fd holding |
| `test_unavailable` | Windows, macOS | in-process | All entry points report failure with `"D-Bus is only supported on Linux"` |
| `test_error` | Linux | in-process | RAII error creation, errno translation, reset, string formatting |
| `test_slot` | Linux | in-process | RAII slot wrapping, release, move semantics, lifetime termination |
| `test_private_bus` | Linux | spawned `dbus-daemon` | Starting, address discovery, environment export, stopping, restarting |
| `test_message` | Linux | private `dbus-daemon` | Message header inspection, basic types, container nesting (array/dict), variant read/write, reply generation |
| `test_bus` | Linux | private `dbus-daemon` | Connection creation, address connection with Hello handshake, name request/release, synchronous method calls, signal emission and matching |
| `test_property_cache` | Linux | private `dbus-daemon` | Initial GetAll refresh, change callbacks, invalidation handling, typed getters |

### Private bus test fixture & CI conditions

- **Test isolation**: All Linux tests that communicate with D-Bus use `PrivateBus`
  to spawn an isolated `dbus-daemon` session instance on an ephemeral UNIX socket.
  Tests never connect to or modify the host machine's session or system bus.
- **CI skipping**: If `dbus-daemon` is not installed on the system, tests relying
  on a live bus daemon cleanly exit 77 (skipped) with an explanatory message.

## License

MIT, see [LICENSE](LICENSE).
