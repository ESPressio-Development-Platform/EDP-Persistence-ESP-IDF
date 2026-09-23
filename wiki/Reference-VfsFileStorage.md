# src/VfsFileStorage.hpp

**Primary classification:** PUBLIC PROVIDER / EXTENSION API

**Source baseline:** `d3f5d43da09083bc81b0664f219025f08ce2561c`

[Open exact source](https://github.com/ESPressio-Development-Platform/EDP-Persistence-ESP-IDF/blob/d3f5d43da09083bc81b0664f219025f08ce2561c/src/VfsFileStorage.hpp)

## Direct includes

- `cerrno`
- `climits`
- `cstdio`
- `cstring`
- `dirent.h`
- `sys/stat.h`
- `unistd.h`
- `ESPressio_Persistence.hpp`
- `memory/ByteOperationsContract.hpp`

## Documented declarations

### `TBindingProfile`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Returns the binding's explicit invocation-concurrency guarantee when present.

Older/custom profiles which predate the concurrency field remain conservative.

```cpp
template<class TBindingProfile>
        [[nodiscard]] consteval InvocationConcurrency BindingConcurrency() noexcept
```

### `TRetention`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Declares the compile-time guarantees of one hierarchical ESP-IDF VFS binding.

- **Template parameter `TRetention`:** Commit-boundary retention guaranteed by the mounted filesystem.
- **Template parameter `TCaseSensitivity`:** Path comparison behaviour guaranteed by the mounted filesystem.
- **Template parameter `TRemovability`:** Whether the backing medium can disappear while the system is running.
- **Template parameter `TMaximumPathBytes`:** Largest complete EDP path accepted by the binding.
- **Template parameter `TMaximumPathSegmentBytes`:** Largest individual path segment accepted by the binding.
- **Template parameter `TMaximumFileSize`:** Largest logical file supported by the binding.
- **Template parameter `TInvocationConcurrency`:** Safe invocation concurrency guaranteed by the mounted filesystem.

```cpp
template<
        RetentionLevel TRetention,
```

### `Retention`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Commit-boundary retention guaranteed by the mounted filesystem.

```cpp
static constexpr RetentionLevel Retention = TRetention;
```

### `CaseSensitivity`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Case-sensitivity semantics guaranteed for paths.

```cpp
static constexpr TextCaseSensitivity CaseSensitivity = TCaseSensitivity;
```

### `Removability`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Whether the backing medium can be removed while the application is running.

```cpp
static constexpr MediaRemovability Removability = TRemovability;
```

### `MaximumPathBytes`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Maximum complete provider-relative path accepted by the binding.

```cpp
static constexpr std::size_t MaximumPathBytes = TMaximumPathBytes;
```

### `MaximumPathSegmentBytes`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Maximum individual path segment accepted by the binding.

```cpp
static constexpr std::size_t MaximumPathSegmentBytes = TMaximumPathSegmentBytes;
```

### `static constexpr StorageSize MaximumFileSize{TMaximumFileSize};`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Maximum logical file size supported by the binding.

```cpp
static constexpr StorageSize MaximumFileSize{TMaximumFileSize};
```

### `Concurrency`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Safe invocation concurrency guaranteed by the mounted filesystem substrate.

```cpp
static constexpr InvocationConcurrency Concurrency = TInvocationConcurrency;
```

### `TBindingTag`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Adapts one already-mounted ESP-IDF VFS root to the EDP FileStorage contract.

- **Template parameter `TBindingTag`:** Distinguishes independently selectable logical VFS bindings.
- **Template parameter `TBindingProfile`:** Declares the semantic guarantees of the mounted filesystem substrate.
- **Template parameter `TByteOperationsProvider`:** Supplies EDP-Memory raw byte-copy operations used by the adapter.

```cpp
template<
        class TBindingTag,
```

### `NativePathCapacity`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Maximum native path assembled by this provider.

```cpp
static constexpr std::size_t NativePathCapacity = 512U;
```

### `NativePathStatus`

**Classification:** PRIVATE IMPLEMENTATION · source access: `private`

Result of assembling one provider-native path.

```cpp
enum class NativePathStatus : std::uint8_t
```

### `FileMutationCommitStatus`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Result of flushing and closing one mutated file at its advertised retention boundary.

```cpp
enum class FileMutationCommitStatus : std::uint8_t
```

### `BasePath_`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Non-owning null-terminated VFS base path supplied by Bootstrap.

```cpp
const char* BasePath_;
```

### `ByteOperations_`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Non-owning EDP-Memory byte-operation provider used for bounded raw copies.

```cpp
const TByteOperationsProvider* ByteOperations_;
```

### `IsBasePathRepresentable`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Reports whether a caller-supplied base path leaves room for every advertised EDP path.

```cpp
[[nodiscard]] static bool IsBasePathRepresentable(const char* BasePath) noexcept
```

### `IsPathRepresentable`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Reports whether a canonical EDP path fits the binding's advertised limits.

```cpp
[[nodiscard]] static bool IsPathRepresentable(FilePathView Path) noexcept
```

### `MakeNativePath`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Builds a native path below BasePath_ without allocating.

```cpp
[[nodiscard]] NativePathStatus MakeNativePath(
            FilePathView Path,
            char (&Buffer)[NativePathCapacity]
        ) const noexcept
```

### `MakeRootPath`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Builds the native path for provider root.

```cpp
[[nodiscard]] NativePathStatus MakeRootPath(char (&Buffer)[NativePathCapacity]) const noexcept
```

### `CommitFileMutation`

**Classification:** PUBLIC PROVIDER / EXTENSION API

Flushes one mutated file through the retention boundary advertised by the binding profile.

```cpp
[[nodiscard]] static FileMutationCommitStatus CommitFileMutation(std::FILE* File) noexcept
```

### `VfsFileStorage`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Constructs a provider over an already-mounted VFS base path and ByteOperations provider.

```cpp
VfsFileStorage(
            const char* BasePath,
            const TByteOperationsProvider& ByteOperations
        ) noexcept
            : BasePath_(IsBasePathRepresentable(BasePath) ? BasePath : nullptr),
```

### `IsFileStorageReady`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Reports whether a VFS base path is bound.

```cpp
[[nodiscard]] bool IsFileStorageReady() const noexcept
```

### `GetFileSize`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Returns the size of one regular file.

```cpp
[[nodiscard]] FileSizeResult GetFileSize(FilePathView Path) const noexcept
```

### `ReadFileAt`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Reads a bounded range from one regular file.

```cpp
[[nodiscard]] FileReadResult ReadFileAt(
            FilePathView Path,
            StorageOffset Offset,
            DestinationBufferView Destination
        ) const noexcept
```

### `ReplaceFile`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Creates or replaces one complete file.

```cpp
[[nodiscard]] FileReplaceStatus ReplaceFile(
            FilePathView Path,
            SourceBufferView Source
        ) noexcept
```

### `RemoveFile`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Removes one regular file.

```cpp
[[nodiscard]] FileRemoveStatus RemoveFile(FilePathView Path) noexcept
```

### `CreateDirectory`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Creates one directory.

```cpp
[[nodiscard]] DirectoryCreateStatus CreateDirectory(FilePathView Path) noexcept
```

### `RemoveDirectory`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Removes one empty directory.

```cpp
[[nodiscard]] DirectoryRemoveStatus RemoveDirectory(FilePathView Path) noexcept
```

### `EnumerateDirectory`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

TCallback is the caller-owned noexcept enumeration callback.

```cpp
template<FileEnumerationCallback TCallback>
        [[nodiscard]] FileEnumerationResult EnumerateDirectory(
            DirectoryPathView Directory,
            DestinationBufferView NameBuffer,
            TCallback& Callback
        ) const noexcept
```

### `RenameEntry`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Renames an entry without overwriting an existing destination.

```cpp
[[nodiscard]] FileRenameStatus RenameEntry(
            FilePathView Source,
            FilePathView Destination
        ) noexcept
```

### `AppendFile`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Appends a complete source buffer to an existing file.

```cpp
[[nodiscard]] FileAppendStatus AppendFile(
            FilePathView Path,
            SourceBufferView Source
        ) noexcept
```

### `WriteFileAt`

**Classification:** PUBLIC PROVIDER / EXTENSION API · source access: `public`

Replaces bytes within an existing file extent.

```cpp
[[nodiscard]] FileWriteAtStatus WriteFileAt(
            FilePathView Path,
            StorageOffset Offset,
            SourceBufferView Source
        ) noexcept
```

