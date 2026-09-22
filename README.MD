# EDP-Persistence-ESP-IDF

Concrete ESP-IDF providers for the EDP-Persistence contract.

## Providers

- `VfsFileStorage<TBindingTag>` adapts an already-mounted ESP-IDF VFS base path to hierarchical `FileStorage`. Bootstrap owns mounting/unmounting. Directory mutation/enumeration, rename, append and bounded write-at are supported.
- `NvsKeyValueStorage<TBindingTag>` adapts one NVS partition/namespace to `KeyValueStorage`. Keys are limited to 15 UTF-8 bytes and values to 512 bytes. Mutations explicitly call `nvs_commit()` before reporting success.

`TBindingTag` gives independently selectable logical bindings distinct Composition provider types.

## Dependencies

- EDP-Persistence-ESP-IDF
  - EDP-Persistence
    - EDP-System
  - ESP-IDF

## Lifecycle

The application initializes NVS and mounts the selected VFS implementation before provider use. `NvsKeyValueStorage::Open()` opens an existing initialized partition/namespace and `Close()` releases its handle. `VfsFileStorage` stores a non-owning base-path pointer whose lifetime must dominate the provider.

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

A standalone ESP-IDF installation is activated through its own `export.sh` before `idf.py` is invoked. A PlatformIO-owned `framework-espidf` package is handled differently: the runner creates an isolated PlatformIO ESP-IDF project and invokes the discovered PlatformIO Core so PlatformIO supplies its own Python/toolchain/CMake/`sdkconfig` environment rather than incorrectly treating the framework package as a standalone Espressif installation.

Missing prerequisites are reported separately so an unavailable ESP-IDF installation is not conflated with missing sibling EDP repositories.

Useful diagnostic options:

```bash
python3 tests/run_tests.py --verbose
python3 tests/run_tests.py --keep-build
python3 tests/run_tests.py --idf-path /path/to/esp-idf
python3 tests/run_tests.py --platformio /path/to/platformio
python3 tests/run_tests.py --platformio-home /path/to/.platformio
```
