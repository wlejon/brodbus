# brodbus

A modern C++20 unified D-Bus layer for the bro desktop ecosystem.

`brodbus` unifies D-Bus connectivity, message serialization, property caching, and test fixtures across the desktop libraries (`brosys`, `brocred`, `broseat`, `broportal`), eliminating duplication and preventing recurring edge cases like missing `Hello` handshakes on private bus connections.

## Features

- **Connection Management (`Bus`)**: RAII management of `sd_bus*` (`sd_bus_flush_close_unref`).
  - `open_system()`: Connects to the system D-Bus.
  - `open_user()`: Connects to the user session bus with automatic fallback to `/run/user/<uid>/bus`.
  - `open_address(address)`: Connects to arbitrary bus addresses (such as private test daemons), setting `sd_bus_set_bus_client(bus, 1)` to ensure the `Hello` handshake completes before starting.
  - Methods for event processing (`process()`, `wait()`, `flush()`), file descriptor polling (`get_fd()`), and well-known name acquisition (`request_name()`, `release_name()`).
- **Signal Matching & Slots (`Slot`)**: RAII wrapper for `sd_bus_slot*` (`sd_bus_slot_unref`), with support for callback registration and automatic lifetime management.
- **Message Serialization (`Message`)**: RAII wrapper around `sd_bus_message*`.
  - Basic types: `bool`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `double`, `std::string`, `ObjectPath`, `UnixFd`.
  - Container helpers: `open_container()`, `close_container()`, `enter_container()`, `exit_container()`, `peek_type()`, `rewind()`, `at_end()`.
  - String list, byte list, and variant helpers.
- **Property Helpers & Cache (`PropertyCache`)**:
  - Direct property getters on `Bus`: `get_property_bool()`, `get_property_string()`, `get_property_uint32()`, etc.
  - `PropertyCache`: Caches interface properties and automatically updates in real-time by listening to `org.freedesktop.DBus.Properties.PropertiesChanged` signals.
- **Error Mapping (`Error`)**: RAII wrapper around `sd_bus_error` (`sd_bus_error_free`) with `"Name: Message"` formatting.
- **Private Bus Test Fixture (`PrivateBus`)**: Spawns an isolated `dbus-daemon --session --nofork --nopidfile --print-address=1`, reads its address, provides environment variables (`DBUS_SESSION_BUS_ADDRESS`), and terminates it cleanly on destruction via `SIGTERM` and `waitpid`.
- **Non-Linux Stubs**: Clean stub implementations for non-Linux platforms (Windows/macOS) so downstream libraries can compile anywhere.

## Building

```bash
cmake -B build -G Ninja
ninja -C build -j 4
ctest --test-dir build --output-on-failure
```
