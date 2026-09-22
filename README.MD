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

The runner creates an isolated temporary project and compiles `tests/ContractCompile.cpp` against the real platform SDK and real sibling EDP contract repositories. Use `--keep-build` to retain the generated project for diagnosis.
