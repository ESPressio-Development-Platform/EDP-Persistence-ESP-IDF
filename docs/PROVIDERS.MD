# Concrete ESP-IDF Persistence Providers

## Purpose and scope

EDP-Persistence-ESP-IDF provides ESP-IDF concrete adapters for the platform-neutral EDP-Persistence contract. It owns SDK adaptation only; Persistence semantics remain defined by EDP-Persistence.

## VfsFileStorage

`VfsFileStorage<TBindingTag, TBindingProfile>` adapts one already-mounted hierarchical VFS subtree.

The binding profile is part of the provider type and declares the semantic guarantees Composition consumers may rely upon:

- retention;
- path case sensitivity;
- media removability;
- maximum complete path bytes;
- maximum path-segment bytes;
- maximum file size.

ESP-IDF VFS can dispatch to materially different filesystems, so these guarantees are not inferred from VFS itself. The application must choose a profile that is true for the mounted backend. This provider structurally offers directory mutation/enumeration, rename, append and bounded write-at; use a different/specialized provider for a backend which does not genuinely provide those operations.

The provider does not mount, format, unmount, or own the VFS backend. The mount and caller-owned base-path storage must remain valid for the provider's complete lifetime.

All operations advertise `CallerSerialized`. File mutation failure preservation is `MayModify`, and interruption atomicity is `None`.

For a binding which explicitly advertises `PowerLoss` retention, file-content mutation success additionally requires `fsync()` before close. Creation/removal/rename/directory metadata durability still depends on the actual mounted backend; a PowerLoss profile is valid only when that backend documents the required semantics.

## NvsKeyValueStorage

`NvsKeyValueStorage<TBindingTag>` adapts one initialized NVS partition/namespace.

The provider exposes opaque blobs up to 512 bytes. NVS key names use the documented 15-byte ASCII representation; a valid EDP UTF-8 key which is not ASCII returns `KeyNotRepresentable`. Native zero-length NVS blobs preserve the EDP distinction between a present empty value and an absent key.

A fixed 512-byte scratch buffer is owned by the provider so truncated logical reads remain allocation-free even though `nvs_get_blob()` requires storage for the complete blob.

Global NVS initialization remains application/Bootstrap responsibility. `Open()` acquires the namespace handle and `Close()` releases it. The namespace must remain dedicated to this opaque-blob provider while in use.

Base key/value mutation failure preservation is `MayModify`. Individual key mutation interruption atomicity remains `PowerLoss`; namespace-wide `ClearAllKeys()` is deliberately `None` because NVS erases matching entries iteratively.

## Composition identity

`TBindingTag` allows otherwise-compatible logical storage bindings to remain distinct provider types. Consumers should select providers through Composition requirements/qualifiers rather than a runtime registry.

## Validation

`tests/ProviderBehaviorTests.cpp` executes adapter-owned behavior with a bounded NVS test double and a temporary host POSIX filesystem. It covers file mutation/read behavior, truncation facts, advertised size limits, key representability, zero-length logical values, clear/remove semantics, and related operation statuses.

`tests/ContractCompile.cpp` independently invokes `ValidatePersistenceProvider<T>()` for both concrete providers. `tests/run_tests.py` runs the host behavior tests first and then validates the real SDK contract through standalone ESP-IDF when available or through the direct PlatformIO-owned ESP-IDF compile probe.
