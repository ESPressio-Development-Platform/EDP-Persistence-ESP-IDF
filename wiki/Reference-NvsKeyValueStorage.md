# src/NvsKeyValueStorage.hpp

**Primary classification:** PUBLIC PROVIDER / EXTENSION API

**Source baseline:** `d3f5d43da09083bc81b0664f219025f08ce2561c`

[Open exact source](https://github.com/ESPressio-Development-Platform/EDP-Persistence-ESP-IDF/blob/d3f5d43da09083bc81b0664f219025f08ce2561c/src/NvsKeyValueStorage.hpp)

## Direct includes

- `nvs.h`
- `ESPressio_Persistence.hpp`
- `memory/ByteOperationsContract.hpp`

## Documented declarations

### `NvsOpenStatus`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Result of opening the NVS namespace owned by one provider instance.

```cpp
enum class NvsOpenStatus : std::uint8_t
```

### `TBindingTag`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Adapts one ESP-IDF NVS namespace to the EDP KeyValueStorage contract.

- **Template parameter `TBindingTag`:** Distinguishes independently selectable NVS namespaces.
- **Template parameter `TByteOperationsProvider`:** Supplies EDP-Memory raw byte-copy operations used by truncated reads.

```cpp
template<
        class TBindingTag,
```

### `Handle_`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Open ESP-IDF NVS handle.

```cpp
nvs_handle_t Handle_;
```

### `ByteOperations_`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Non-owning EDP-Memory byte-operation provider used for bounded raw copies.

```cpp
const TByteOperationsProvider* ByteOperations_;
```

### `mutable std::uint8_t ReadScratch_[512U];`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Bounded scratch space used only when the caller requests a truncated blob read.

```cpp
mutable std::uint8_t ReadScratch_[512U];
```

### `IsReady_`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Indicates whether Handle_ is currently open.

```cpp
bool IsReady_;
```

### `KeyCopyStatus`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Result of converting an EDP key to the native NVS key representation.

```cpp
enum class KeyCopyStatus : std::uint8_t
```

### `CopyKey`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Copies an EDP key into NVS's documented ASCII key representation.

```cpp
[[nodiscard]] static KeyCopyStatus CopyKey(
            KeyView Key,
            char (&Buffer)[16U]
        ) noexcept
```

### `NvsKeyValueStorage`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Constructs an unopened provider using one caller-owned ByteOperations provider.

```cpp
explicit NvsKeyValueStorage(
            const TByteOperationsProvider& ByteOperations
        ) noexcept
            : Handle_(0U),
```

### `Open`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Opens one caller-selected namespace in an initialized NVS partition.

```cpp
[[nodiscard]] NvsOpenStatus Open(
            const char* Namespace,
            const char* Partition = NVS_DEFAULT_PART_NAME
        ) noexcept
```

### `Close`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Closes the NVS handle.

```cpp
void Close() noexcept
```

### `IsKeyValueStorageReady`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Reports whether the NVS namespace is open.

```cpp
[[nodiscard]] bool IsKeyValueStorageReady() const noexcept
```

### `GetValueSize`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Returns the complete blob size for one key.

```cpp
[[nodiscard]] KeyValueSizeResult GetValueSize(KeyView Key) const noexcept
```

### `ReadValue`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Reads as much of one blob as the caller destination can hold.

```cpp
[[nodiscard]] KeyValueReadResult ReadValue(
            KeyView Key,
            DestinationBufferView Destination
        ) const noexcept
```

### `StoreValue`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Stores and commits one complete opaque blob.

```cpp
[[nodiscard]] KeyValueStoreStatus StoreValue(
            KeyView Key,
            SourceBufferView Source
        ) noexcept
```

### `RemoveKey`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Removes and commits one key.

```cpp
[[nodiscard]] KeyValueRemoveStatus RemoveKey(KeyView Key) noexcept
```

### `ClearAllKeys`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Removes and commits all keys in the bound namespace.

```cpp
[[nodiscard]] KeyValueClearStatus ClearAllKeys() noexcept
```

