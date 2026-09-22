# EDP-Persistence-ESP-IDF Dependency Graph

## Library Dependency Tree

```text
EDP-Persistence-ESP-IDF
├── EDP-Persistence [mandatory]
│   └── EDP-System [mandatory, transitive]
├── EDP-Memory [mandatory]
│   ├── EDP-System [mandatory, transitive]
│   └── EDP-Platform [mandatory, transitive]
│       └── EDP-System [mandatory, transitive]
└── ESP-IDF [mandatory concrete SDK]
```

## Mandatory Dependencies

- `EDP-Persistence` — defines the FileStorage and KeyValueStorage contracts implemented here.
- `EDP-Memory` — defines the `ByteOperations` capability consumed by both concrete providers for raw byte copies.
- `ESP-IDF` — supplies the domain-specific VFS/POSIX and NVS APIs adapted by this explicitly platform-specific Persistence repository.

The direct ESP-IDF SDK dependency is permitted by `Policy/V2/ABSTRACTION.MD` because this repository is the explicit `EDP-Persistence-ESP-IDF` concrete sibling of the SDK-neutral `EDP-Persistence` repository.

## Optional / development dependencies

- `EDP-Platform-Portable` — required only by the supplied tests and demos when they select the standard C/C++ `ByteOperationsProvider`. Consuming applications may supply any provider satisfying `EDP-Memory::ByteOperations` instead.

No other optional library dependency is imposed by the production provider headers.
