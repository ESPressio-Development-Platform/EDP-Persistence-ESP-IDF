# EDP-Persistence-ESP-IDF

Concrete ESP-IDF providers for the EDP-Persistence contract.

This repository is the explicit ESP-IDF concrete sibling of the SDK-neutral `EDP-Persistence` repository. Its direct ESP-IDF SDK references are permitted by the domain-specific exception in `Policy/V2/ABSTRACTION.MD`.

## Providers

- `VfsFileStorage<TBindingTag, TBindingProfile, TByteOperationsProvider>` adapts an already-mounted hierarchical ESP-IDF VFS base path to `FileStorage`.
- `NvsKeyValueStorage<TBindingTag, TByteOperationsProvider>` adapts one initialized NVS partition/namespace to `KeyValueStorage`.

`TBindingTag` gives independently selectable logical bindings distinct Composition provider types.

`TByteOperationsProvider` must satisfy `EDP-Memory::ByteOperations`. The provider is supplied by Bootstrap, held non-owningly, and must outlive the Persistence provider. All raw byte copies performed by these adapters route through that Memory capability.

## Documentation

- [Concrete provider contracts](./docs/PROVIDERS.MD) — provider semantics, lifecycle, ownership, Memory integration, ISR boundary, failure guarantees, and Composition integration.
- [Build baseline](./docs/BUILDING.MD) — mandatory C++20 baseline, PIOArduino guidance for Arduino-facing builds, and ESP-IDF configuration.
- [Concrete Providers demo](./demos/ConcreteProviders/README.MD) — hardware/runtime expectations and target-specific build surfaces.

## Dependencies

- EDP-Persistence-ESP-IDF
  - EDP-Persistence **[mandatory]**
    - EDP-System **[transitive]**
  - EDP-Memory **[mandatory]**
    - EDP-System **[transitive]**
    - EDP-Platform **[transitive]**
  - ESP-IDF **[mandatory concrete SDK]**
  - EDP-Platform-Portable **[optional: supplied tests/demos use its standard ByteOperations provider]**

A consuming application may replace EDP-Platform-Portable with any provider satisfying `EDP-Memory::ByteOperations`.

See [DEPENDENCY_GRAPH.MD](./DEPENDENCY_GRAPH.MD) for the maintained dependency graph.

## VFS binding profile

A VFS binding profile is a compile-time assertion about the specific mounted filesystem, not about ESP-IDF VFS in general.

For example:

```cpp
using ByteOperationsProvider =
    ESPressio::Platform::Portable::Memory::ByteOperationsProvider;

using StorageProfile = ESPressio::Persistence::EspIdf::VfsBindingProfile<
    ESPressio::Persistence::RetentionLevel::Restart,
    ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
    ESPressio::Persistence::MediaRemovability::Fixed,
    255U,
    255U,
    0x7FFFFFFFULL,
    ESPressio::Persistence::InvocationConcurrency::ConcurrentReads
>;

using StorageProvider =
    ESPressio::Persistence::EspIdf::VfsFileStorage<
        MyBinding,
        StorageProfile,
        ByteOperationsProvider
    >;
```

Only select values which are true for the mounted VFS backend. A profile claiming `PowerLoss` means every successful mutation performed through the provider has crossed a power-loss-surviving commit boundary. For file-content writes, the provider additionally performs `fsync()` before success when `PowerLoss` is advertised.

The final `InvocationConcurrency` template argument is optional and defaults to `CallerSerialized`, preserving the conservative behavior of existing profiles. Set it to `ConcurrentReads` only when the mounted filesystem genuinely permits safe concurrent read-side invocations. The provider adds no lock or runtime state for this guarantee; it advertises the binding fact exactly as supplied. Existing custom binding-profile types which do not expose a concurrency field also remain conservatively `CallerSerialized`.

A stronger concurrency value is a guarantee about the complete bound provider configuration, including the selected ByteOperations dependency. The supplied FAT demos use the stateless `EDP-Platform-Portable::Memory::ByteOperationsProvider`; applications using another ByteOperations concrete must ensure that dependency is compatible with the advertised invocation level.

## Lifecycle

The application initializes NVS and mounts the selected VFS implementation before provider use. The mount, caller-owned base-path storage, and ByteOperations provider must remain valid for the provider's complete lifetime.

Construct `NvsKeyValueStorage` with the selected ByteOperations provider, then call `Open()` after NVS initialization. `Open()` returns `NvsOpenStatus::Succeeded` or `NvsOpenStatus::ProviderFailure`. Call `Close()` during teardown.

The NVS namespace is dedicated to this opaque-blob provider while open.

## Concurrency and memory

`VfsFileStorage` advertises the concurrency guarantee supplied by its binding profile. Standard profiles default to `CallerSerialized`; a binding may explicitly advertise `ConcurrentReads` or `ConcurrentOperations` only when that is true for the mounted substrate. This changes no runtime RAM or locking behavior.

`NvsKeyValueStorage` remains `CallerSerialized`.

NVS contains one bounded 512-byte scratch buffer so caller destinations may receive truncated reads without dynamic allocation even though `nvs_get_blob()` retrieves the complete blob. Truncated-copy transfer uses the injected EDP-Memory ByteOperations provider.

## ISR boundary

These providers are not ISR-safe and make no ISR-safety guarantee. Do not perform VFS/POSIX, NVS, lifecycle, enumeration, read, or Persistence mutation work from an ISR. Capture only minimal ISR-local state and defer Persistence work to ordinary execution context.

## Build baseline

C++20 is mandatory. Pure ESP-IDF consumers compile the component as C++20; Arduino-facing builds should use PIOArduino. See [Build baseline](./docs/BUILDING.MD).

## Validation

From the repository root, with sibling `EDP-Persistence`, `EDP-Memory`, `EDP-Platform`, `EDP-Platform-Portable`, and `EDP-System` checkouts present:

```bash
python3 tests/run_tests.py
```

The runner validates:

1. complete concrete provider behavior/lifecycle coverage;
2. PlatformIO Arduino demo source;
3. PlatformIO ESP-IDF demo source;
4. the concrete providers against a real installed ESP-IDF SDK/toolchain.

For PlatformIO-owned ESP-IDF packages, the final stage uses the already-established direct target-compiler probe so local PlatformIO package-manager/upload failures do not masquerade as provider compiler failures.

Useful explicit overrides include `--persistence`, `--memory`, `--platform`, `--platform-portable`, `--system`, `--idf-path`, `--compiler`, and `--platformio-home`.
