# Concrete ESP-IDF Persistence Providers

## Purpose and scope

EDP-Persistence-ESP-IDF provides ESP-IDF concrete adapters for the platform-neutral EDP-Persistence contract.

This repository is intentionally the platform-specific ESP-IDF concrete sibling of `EDP-Persistence`. Under the domain-specific exception defined by `Policy/V2/ABSTRACTION.MD`, ESP-IDF SDK references which implement Persistence-domain behavior may reside here directly. The domain-neutral `EDP-Persistence` repository remains SDK-free.

Persistence semantics remain defined by EDP-Persistence.

## ByteOperations dependency

Both concrete providers consume one caller-owned provider satisfying `EDP-Memory::ByteOperations`.

The ByteOperations provider is:

- supplied by application Bootstrap;
- held non-owningly by the Persistence provider;
- required to outlive the Persistence provider;
- used for raw byte-copy operations instead of calling Standard C/C++ memory-copy primitives directly.

The supplied tests and demos select `EDP-Platform-Portable::Memory::ByteOperationsProvider`, but an application may select any provider satisfying the EDP-Memory capability contract.

## VfsFileStorage

`VfsFileStorage<TBindingTag, TBindingProfile, TByteOperationsProvider>` adapts one already-mounted hierarchical VFS subtree.

The binding profile is part of the provider type and declares the semantic guarantees Composition consumers may rely upon:

- retention;
- path case sensitivity;
- media removability;
- maximum complete path bytes;
- maximum path-segment bytes;
- maximum file size;
- safe invocation concurrency.

ESP-IDF VFS can dispatch to materially different filesystems, so these guarantees are not inferred from VFS itself. The application must choose a profile that is true for the mounted backend.

The provider structurally offers:

- mandatory FileStorage operations;
- directory creation/removal;
- directory enumeration;
- rename;
- append;
- bounded write-at.

Use a different/specialized provider for a backend which does not genuinely supply those semantics.

The provider does not mount, format, unmount, or own the VFS backend. The mount, caller-owned base-path storage, and ByteOperations provider must all remain valid for the provider's complete lifetime.

`VfsFileStorage` advertises the binding profile's invocation-concurrency guarantee. The standard profile defaults to `CallerSerialized`; older/custom profiles without an explicit concurrency field are also treated as `CallerSerialized`. A binding may opt into `ConcurrentReads` or `ConcurrentOperations` only when the actual mounted filesystem safely supports that invocation level.

The provider does not add internal synchronization merely because a stronger binding guarantee is declared. The concurrency property is therefore zero-RAM/zero-runtime-overhead metadata describing the mounted substrate, not a promise manufactured by the adapter.

File mutation failure preservation is `MayModify`, and interruption atomicity is `None`.

For a binding which explicitly advertises `PowerLoss` retention, file-content mutation success additionally requires `fsync()` before close. Creation/removal/rename/directory metadata durability still depends on the mounted backend's documented syscall semantics; a PowerLoss profile is valid only when that backend documents the required semantics.

## NvsKeyValueStorage

`NvsKeyValueStorage<TBindingTag, TByteOperationsProvider>` adapts one initialized NVS partition/namespace.

The provider exposes opaque blobs up to 512 bytes.

NVS key names use the documented 15-byte ASCII representation. A valid EDP UTF-8 key which is not representable as ASCII returns `KeyNotRepresentable`.

Native zero-length NVS blobs preserve the EDP distinction between a present empty value and an absent key.

A fixed 512-byte scratch buffer is owned by the provider so truncated logical reads remain allocation-free even though `nvs_get_blob()` requires storage for the complete blob. Copying from that scratch buffer to caller memory is performed through the injected EDP-Memory ByteOperations provider.

Global NVS initialization remains application/Bootstrap responsibility.

`Open()` returns `NvsOpenStatus` rather than `bool`:

- `Succeeded` means the namespace is ready, including an idempotent call made while already open;
- `ProviderFailure` means ESP-IDF could not open the requested partition/namespace.

`Close()` releases the handle.

The namespace must remain dedicated to this opaque-blob provider while in use.

Base key/value mutation failure preservation is `MayModify`. Individual key mutation interruption atomicity remains `PowerLoss`; namespace-wide `ClearAllKeys()` is deliberately `None` because NVS erases matching entries iteratively.

## Composition identity

`TBindingTag` allows otherwise-compatible logical storage bindings to remain distinct provider types.

Consumers should select Persistence providers through Composition requirements/qualifiers rather than a runtime registry.

The injected ByteOperations provider remains a separate Memory-domain provider selected by Bootstrap; it is not folded into the Persistence capability identity.

Each concrete Persistence provider declares a cross-domain `DependsOn<Need<EDP-Memory::ByteOperations>>` Composition dependency. Application architecture validation therefore requires a Memory-domain Composition containing at least one provider satisfying ByteOperations.

## ISR boundary

These Persistence providers are **not ISR-safe and make no ISR-safety guarantee**.

Do not call VFS/POSIX filesystem, NVS, Persistence mutation, enumeration, lifecycle, or ordinary read operations from an ISR. Follow the ESPressio ISR policy: capture only the minimum ISR-local observation/state required, return promptly, and perform Persistence work later from ordinary execution context.

## Validation

`tests/ProviderBehaviorTests.cpp` exercises every concrete provider operation plus lifecycle/not-ready behavior using a bounded NVS test double and a temporary host POSIX filesystem.

`tests/ContractCompile.cpp` independently invokes `ValidatePersistenceProvider<T>()` for both providers while wiring the real EDP-Platform-Portable ByteOperations concrete type.

`tests/run_tests.py` performs:

1. host behavior-test compilation/execution;
2. PlatformIO Arduino demo source compilation;
3. PlatformIO ESP-IDF demo source compilation;
4. real ESP-IDF SDK contract compilation.

The runner requires sibling EDP-Persistence, EDP-Memory, EDP-Platform, EDP-Platform-Portable, and EDP-System checkouts.

## Build baseline

See [BUILDING.MD](./BUILDING.MD) for the mandatory C++20 baseline, PIOArduino recommendation for Arduino-facing demo/consumer builds, and ESP-IDF build guidance.
