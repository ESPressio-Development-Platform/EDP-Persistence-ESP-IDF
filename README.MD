# EDP-Persistence-ESP-IDF

Concrete ESP-IDF providers for the EDP-Persistence contract.

## Providers

- `VfsFileStorage<TBindingTag, TBindingProfile>` adapts an already-mounted hierarchical ESP-IDF VFS base path to `FileStorage`. The binding profile explicitly declares retention, path case sensitivity, media removability, path/segment limits, and maximum file size. This provider is intentionally not a universal claim about every VFS backend: use it only when the mounted filesystem actually supplies the advertised hierarchical directory, rename, append, and bounded write-at operations.
- `NvsKeyValueStorage<TBindingTag>` adapts one initialized NVS partition/namespace to `KeyValueStorage`. Keys are limited to the documented 15-byte ASCII representation; valid EDP UTF-8 keys outside ASCII return `KeyNotRepresentable`. Values remain bounded to 512 bytes, including a present zero-length blob. Mutations call `nvs_commit()` before reporting success.

`TBindingTag` gives independently selectable logical bindings distinct Composition provider types.

## Documentation

- [Concrete provider contracts](./docs/PROVIDERS.MD) — provider semantics, lifecycle, ownership, memory, failure guarantees and Composition integration.

## Dependencies

- EDP-Persistence-ESP-IDF
  - EDP-Persistence
    - EDP-System
  - ESP-IDF

[Dependency graph](./DEPENDENCY_GRAPH.MD)

## VFS binding profile

A VFS binding profile is part of the provider type. It is a compile-time assertion about the specific mounted filesystem, not about ESP-IDF VFS in general.

For example:

```cpp
using StorageProfile = ESPressio::Persistence::EspIdf::VfsBindingProfile<
    ESPressio::Persistence::RetentionLevel::Restart,
    ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
    ESPressio::Persistence::MediaRemovability::Fixed,
    255U,
    255U,
    0x7FFFFFFFULL
>;

using StorageProvider =
    ESPressio::Persistence::EspIdf::VfsFileStorage<MyBinding, StorageProfile>;
```

Only select values that are true for the actual mounted VFS backend. A profile claiming `PowerLoss` means every successful mutation performed through the provider has crossed a power-loss-surviving commit boundary. For file-content writes, the provider additionally performs `fsync()` before success when `PowerLoss` is advertised; metadata durability still depends on the mounted backend's documented syscall semantics.

## Lifecycle

The application initializes NVS and mounts the selected VFS implementation before provider use. The VFS mount must remain active for the provider's complete use lifetime. `NvsKeyValueStorage::Open()` opens an existing initialized partition/namespace and `Close()` releases its handle; that namespace is dedicated to this opaque-blob provider while it is in use. `VfsFileStorage` stores a non-owning base-path pointer whose lifetime must dominate the provider.

## Concurrency and memory

Both providers advertise `CallerSerialized`. NVS includes a bounded 512-byte scratch buffer so a caller can request a truncated blob read without dynamic allocation even though `nvs_get_blob()` requires storage for the complete blob.

## Validation

Concrete SDK compilation and target runtime testing belong in this repository. The abstract contract remains platform-neutral in EDP-Persistence.

## Local contract compilation

From this repository root, with sibling `EDP-Persistence` and `EDP-System` checkouts present, run:

```bash
python3 tests/run_tests.py
```

The runner creates an isolated temporary project and compiles `tests/ContractCompile.cpp` against the real platform SDK and real sibling EDP contract repositories.

ESP-IDF discovery is deliberately self-contained. The runner checks an explicit `--idf-path`, `IDF_PATH`, an explicit `--idf-py`, `idf.py` already on `PATH`, conventional `~/esp/esp-idf` and `~/esp-idf` locations, versioned `~/esp/*/esp-idf` and `~/.espressif/*/esp-idf` installations, common development roots, and the PlatformIO `framework-espidf` package.

A standalone ESP-IDF installation is activated through its own `export.sh` before `idf.py` is invoked. A PlatformIO-owned `framework-espidf` package is handled differently: the runner performs a compile-only probe directly with the installed Xtensa compiler and only the ESP-IDF public headers actually consumed by these providers (`nvs_flash/include` and `esp_common/include`). VFS/POSIX headers intentionally come from the Xtensa/Newlib toolchain itself. This avoids both PlatformIO package-management/upload tooling and host-only ESP-IDF compatibility headers shadowing target headers. A temporary ESP32 `sdkconfig.h` supplies only the target/compiler configuration required to parse those public SDK headers.

Missing prerequisites are reported separately so an unavailable ESP-IDF installation is not conflated with missing sibling EDP repositories.

Useful diagnostic options:

```bash
python3 tests/run_tests.py --verbose
python3 tests/run_tests.py --keep-build
python3 tests/run_tests.py --idf-path /path/to/esp-idf
python3 tests/run_tests.py --compiler /path/to/xtensa-compiler
python3 tests/run_tests.py --platformio-home /path/to/.platformio
```
