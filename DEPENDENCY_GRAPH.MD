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

## 2026-10-26 low-space fixture dependency revalidation

The optional native LittleFS high-water test is inside the **existing**
`tests/hil/littlefs-bounded-espidf` directory and consumes the already
declared native ESP-IDF + `joltwallet/littlefs` fixture dependencies.
The offline extraction checker is Python-standard-library-only and
requires the existing external test-only `littlefs-python` command solely
when auditing a privately extracted device partition. No production
`EDP-Persistence-ESP-IDF` mandatory or optional library edge,
Composition capability, Memory operation or provider is added. The complete
existing dependency tree above remains unchanged; all versions are 0.1.0.
