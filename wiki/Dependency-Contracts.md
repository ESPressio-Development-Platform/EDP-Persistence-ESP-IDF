# Dependency Contracts

EDP-Persistence-ESP-IDF depends on **EDP-Persistence** and **EDP-Memory**.

## EDP-Persistence

The repository implements:

- `VfsFileStorage<TBindingTag,TBindingProfile,TByteOperationsProvider>` as a `FileStorage` provider;
- `NvsKeyValueStorage<TBindingTag,TByteOperationsProvider>` as a `KeyValueStorage` provider.

Their advertised properties are compile-time facts used by consumers such as EDP-Localisation.

## EDP-Memory ByteOperations

Both provider Contracts require exactly one external `Memory::ByteOperations` provider and validate it through the Memory internal provider-trait contract.

## VfsFileStorage

Advertises read/write hierarchical FileStorage. Retention, case sensitivity, removability, path/segment/file limits and invocation concurrency come from the binding profile. Directory mutation/enumeration, rename, append and ranged write are supported; capacity reporting is unsupported.

The provider wraps an **already-mounted** VFS subtree and must not claim stronger durability/concurrency than the mounted backend really supplies.

## NvsKeyValueStorage

Advertises read/write PowerLoss-retained KeyValueStorage with case-sensitive fixed media, 15-byte keys, 512-byte values, CallerSerialized invocation and ClearAll support. Native NVS empty blobs are represented directly.

## Ownership

Partition/VFS lifecycle remains outside the provider. Bootstrap owns the ByteOperations provider and constructs the Persistence provider with explicit native binding configuration.

> Dependency contract audit baseline: `0629cdd5c4c3be4193a020d1f1e2f64dfa7e5322` (`main`).
