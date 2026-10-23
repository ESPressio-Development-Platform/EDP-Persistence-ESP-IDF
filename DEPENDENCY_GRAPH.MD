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

## Optional LittleFS backend (2026-10-23)

When and ONLY when consuming `LittleFsVfsMount.hpp`, the application
must supply upstream `joltwallet/littlefs` ESP-IDF component.
`LittleFsVfsBinding.hpp` uses the existing EDP-Persistence
VfsFileStorage and EDP-Memory ByteOperations contract; no additional
EDP mandatory dependency or reverse path is introduced. Native
LittleFS's mount/cache/wear logic stays in the vendor component,
not duplicated inside EDP-Persistence-ESP-IDF or EDP-Logging.
SPIFFS and NVS existing composition guarantees remain unchanged.
