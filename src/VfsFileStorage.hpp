#pragma once

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ESPressio_Persistence.hpp>

namespace ESPressio::Persistence::EspIdf {

    namespace Framework = ESPressio::System::CompositionFramework;


    /// Declares the compile-time guarantees of one hierarchical ESP-IDF VFS binding.
    /// TRetention is the commit-boundary retention guaranteed by the mounted filesystem.
    /// TCaseSensitivity is the path comparison behaviour of the mounted filesystem.
    /// TRemovability describes whether the backing medium can disappear while the system is running.
    /// TMaximumPathBytes is the largest complete EDP path accepted by the binding.
    /// TMaximumPathSegmentBytes is the largest individual path segment accepted by the binding.
    /// TMaximumFileSize is the largest logical file supported by the binding.
    template<
        RetentionLevel TRetention,
        TextCaseSensitivity TCaseSensitivity,
        MediaRemovability TRemovability,
        std::size_t TMaximumPathBytes,
        std::size_t TMaximumPathSegmentBytes,
        std::uint64_t TMaximumFileSize
    >
    struct VfsBindingProfile final {

        static constexpr RetentionLevel Retention = TRetention;
        static constexpr TextCaseSensitivity CaseSensitivity = TCaseSensitivity;
        static constexpr MediaRemovability Removability = TRemovability;
        static constexpr std::size_t MaximumPathBytes = TMaximumPathBytes;
        static constexpr std::size_t MaximumPathSegmentBytes = TMaximumPathSegmentBytes;
        static constexpr StorageSize MaximumFileSize{TMaximumFileSize};

    };


    /// TBindingTag distinguishes independently selectable ESP-IDF VFS roots in Composition.
    /// TBindingProfile declares the substrate guarantees supplied by the already-mounted hierarchical filesystem.
    template<class TBindingTag, class TBindingProfile>
    class VfsFileStorage final : public Framework::Provider<
        Domain,
        Framework::Provides<
            Framework::Offer<
                FileStorage,
                Framework::PropertyValue<FileAccessMode, AccessMode::ReadWrite>,
                Framework::PropertyValue<FileRetention, TBindingProfile::Retention>,
                Framework::PropertyValue<FileHierarchyMode, FileHierarchy::Hierarchical>,
                Framework::PropertyValue<FilePathCaseSensitivity, TBindingProfile::CaseSensitivity>,
                Framework::PropertyValue<FileMediaRemovability, TBindingProfile::Removability>,
                Framework::PropertyValue<MaximumPathBytes, TBindingProfile::MaximumPathBytes>,
                Framework::PropertyValue<MaximumPathSegmentBytes, TBindingProfile::MaximumPathSegmentBytes>,
                Framework::PropertyValue<MaximumFileSize, TBindingProfile::MaximumFileSize>,
                Framework::PropertyValue<DirectoryMutationSupport, Support::Supported>,
                Framework::PropertyValue<DirectoryEnumerationSupport, Support::Supported>,
                Framework::PropertyValue<RenameSupport, Support::Supported>,
                Framework::PropertyValue<AppendSupport, Support::Supported>,
                Framework::PropertyValue<WriteFileAtSupport, Support::Supported>,
                Framework::PropertyValue<FileCapacityReportingSupport, Support::Unsupported>,
                Framework::PropertyValue<FileInvocationConcurrency, InvocationConcurrency::CallerSerialized>,
                Framework::PropertyValue<FileFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<FileInterruptionAtomicity, InterruptionAtomicity::None>,
                Framework::PropertyValue<DirectoryMutationFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<DirectoryMutationInterruptionAtomicity, InterruptionAtomicity::None>,
                Framework::PropertyValue<RenameFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<RenameInterruptionAtomicity, InterruptionAtomicity::None>,
                Framework::PropertyValue<AppendFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<AppendInterruptionAtomicity, InterruptionAtomicity::None>,
                Framework::PropertyValue<WriteFileAtFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<WriteFileAtInterruptionAtomicity, InterruptionAtomicity::None>
            >
        >
    > {
    private:

        static_assert(
            TBindingProfile::MaximumPathBytes > 0U &&
            TBindingProfile::MaximumPathBytes <= 255U,
            "ESP-IDF VfsFileStorage binding path limit must be between 1 and 255 bytes"
        );

        static_assert(
            TBindingProfile::MaximumPathSegmentBytes > 0U &&
            TBindingProfile::MaximumPathSegmentBytes <= TBindingProfile::MaximumPathBytes,
            "ESP-IDF VfsFileStorage binding segment limit must be non-zero and no larger than the path limit"
        );

        static_assert(
            TBindingProfile::MaximumFileSize.RawValue <= static_cast<std::uint64_t>(LONG_MAX),
            "ESP-IDF VfsFileStorage binding file limit must fit the provider's fseek representation"
        );

        /// Maximum native path assembled by this provider.
        static constexpr std::size_t NativePathCapacity = 512U;

        // Bound VFS root.

        /// Non-owning null-terminated VFS base path supplied by Bootstrap.
        const char* BasePath_;

        /// Reports whether a caller-supplied base path leaves room for every advertised EDP path.
        [[nodiscard]] static bool IsBasePathRepresentable(const char* BasePath) noexcept {
            if (BasePath == nullptr) {
                return false;
            }

            const auto Length = std::strlen(BasePath);

            return Length + 1U + TBindingProfile::MaximumPathBytes + 1U <= NativePathCapacity;
        }

        /// Reports whether a canonical EDP path fits the binding's advertised limits.
        [[nodiscard]] static bool IsPathRepresentable(FilePathView Path) noexcept {
            if (Path.Size() > TBindingProfile::MaximumPathBytes) {
                return false;
            }

            std::size_t SegmentSize = 0U;

            for (std::size_t Index = 0U; Index < Path.Size(); ++Index) {
                if (Path.Data()[Index] == '/') {
                    if (SegmentSize > TBindingProfile::MaximumPathSegmentBytes) {
                        return false;
                    }

                    SegmentSize = 0U;
                    continue;
                }

                ++SegmentSize;
            }

            return SegmentSize <= TBindingProfile::MaximumPathSegmentBytes;
        }

        /// Builds a native path below BasePath_ without allocating.
        [[nodiscard]] bool MakeNativePath(
            FilePathView Path,
            char (&Buffer)[NativePathCapacity]
        ) const noexcept {
            if (BasePath_ == nullptr || !IsPathRepresentable(Path)) {
                return false;
            }

            const auto BaseLength = std::strlen(BasePath_);

            if (BaseLength + 1U + Path.Size() + 1U > NativePathCapacity) {
                return false;
            }

            std::memcpy(
                Buffer,
                BasePath_,
                BaseLength
            );
            Buffer[BaseLength] = '/';
            std::memcpy(
                Buffer + BaseLength + 1U,
                Path.Data(),
                Path.Size()
            );
            Buffer[BaseLength + 1U + Path.Size()] = '\0';
            return true;
        }

        /// Builds the native path for provider root.
        [[nodiscard]] bool MakeRootPath(char (&Buffer)[NativePathCapacity]) const noexcept {
            if (BasePath_ == nullptr) {
                return false;
            }

            const auto Length = std::strlen(BasePath_);

            if (Length + 1U > NativePathCapacity) {
                return false;
            }

            std::memcpy(
                Buffer,
                BasePath_,
                Length + 1U
            );
            return true;
        }


        /// Flushes one mutated file through the retention boundary advertised by the binding profile.
        [[nodiscard]] static bool CommitFileMutation(std::FILE* File) noexcept {
            const auto FlushResult = std::fflush(File);
            auto SyncResult = 0;

            if constexpr (TBindingProfile::Retention == RetentionLevel::PowerLoss) {
                const auto Descriptor = ::fileno(File);
                SyncResult = Descriptor < 0 ? -1 : ::fsync(Descriptor);
            }

            const auto CloseResult = std::fclose(File);

            return FlushResult == 0 && SyncResult == 0 && CloseResult == 0;
        }

    public:

        /// Constructs a provider over an already-mounted VFS base path.
        explicit VfsFileStorage(const char* BasePath) noexcept
            : BasePath_(IsBasePathRepresentable(BasePath) ? BasePath : nullptr) {}

        /// Reports whether a VFS base path is bound.
        [[nodiscard]] bool IsFileStorageReady() const noexcept {
            return BasePath_ != nullptr;
        }

        /// Returns the size of one regular file.
        [[nodiscard]] FileSizeResult GetFileSize(FilePathView Path) const noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return {FileSizeStatus::PathNotRepresentable, StorageSize{}};
            }

            struct stat Information {};

            if (stat(NativePath, &Information) != 0) {
                return {errno == ENOENT ? FileSizeStatus::NotFound : FileSizeStatus::IoFailure, StorageSize{}};
            }

            if (!S_ISREG(Information.st_mode)) {
                return {FileSizeStatus::NotFound, StorageSize{}};
            }

            const auto Size = static_cast<std::uint64_t>(Information.st_size);

            if (Size > TBindingProfile::MaximumFileSize.RawValue) {
                return {FileSizeStatus::ProviderFailure, StorageSize{}};
            }

            return {FileSizeStatus::Succeeded, StorageSize{Size}};
        }

        /// Reads a bounded range from one regular file.
        [[nodiscard]] FileReadResult ReadFileAt(
            FilePathView Path,
            StorageOffset Offset,
            DestinationBufferView Destination
        ) const noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return {FileReadStatus::PathNotRepresentable, 0U, 0U, StorageSize{}};
            }

            auto* File = std::fopen(
                NativePath,
                "rb"
            );

            if (File == nullptr) {
                return {errno == ENOENT ? FileReadStatus::NotFound : FileReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            if (std::fseek(File, 0L, SEEK_END) != 0) {
                std::fclose(File);
                return {FileReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            const auto End = std::ftell(File);

            if (End < 0) {
                std::fclose(File);
                return {FileReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            const auto Size = static_cast<std::uint64_t>(End);

            if (Size > TBindingProfile::MaximumFileSize.RawValue) {
                std::fclose(File);
                return {FileReadStatus::ProviderFailure, 0U, 0U, StorageSize{}};
            }

            if (Offset.RawValue > Size || Offset.RawValue > static_cast<std::uint64_t>(LONG_MAX)) {
                std::fclose(File);
                return {FileReadStatus::InvalidOffset, 0U, 0U, StorageSize{}};
            }

            if (std::fseek(File, static_cast<long>(Offset.RawValue), SEEK_SET) != 0) {
                std::fclose(File);
                return {FileReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            const auto Available = Size - Offset.RawValue;
            const auto TransferSize = Available < Destination.Capacity ? static_cast<std::size_t>(Available) : Destination.Capacity;
            const auto Read = TransferSize == 0U ? 0U : std::fread(
                Destination.Address,
                1U,
                TransferSize,
                File
            );
            std::fclose(File);

            if (Read != TransferSize) {
                return {FileReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            std::uint8_t Facts = static_cast<std::uint8_t>(ReadFact::AvailableDataSizeIsKnown);

            if (Available > Destination.Capacity) {
                Facts |= static_cast<std::uint8_t>(ReadFact::WasTruncated);
            } else if (Available < Destination.Capacity) {
                Facts |= static_cast<std::uint8_t>(ReadFact::IsSmallerThanAvailableBuffer);
            }

            return {FileReadStatus::Succeeded, Facts, Read, StorageSize{Available}};
        }

        /// Creates or replaces one complete file.
        [[nodiscard]] FileReplaceStatus ReplaceFile(
            FilePathView Path,
            SourceBufferView Source
        ) noexcept {
            if (Source.Size > TBindingProfile::MaximumFileSize.RawValue) {
                return FileReplaceStatus::FileTooLarge;
            }

            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return FileReplaceStatus::PathNotRepresentable;
            }

            auto* File = std::fopen(
                NativePath,
                "wb"
            );

            if (File == nullptr) {
                return errno == ENOENT ? FileReplaceStatus::ParentNotFound : FileReplaceStatus::IoFailure;
            }

            const auto Written = Source.Size == 0U ? 0U : std::fwrite(
                Source.Address,
                1U,
                Source.Size,
                File
            );
            const auto Committed = CommitFileMutation(File);

            return Written == Source.Size && Committed ? FileReplaceStatus::Succeeded : FileReplaceStatus::IoFailure;
        }

        /// Removes one regular file.
        [[nodiscard]] FileRemoveStatus RemoveFile(FilePathView Path) noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return FileRemoveStatus::PathNotRepresentable;
            }

            if (unlink(NativePath) == 0) {
                return FileRemoveStatus::Succeeded;
            }

            return errno == ENOENT ? FileRemoveStatus::NotFound : FileRemoveStatus::IoFailure;
        }

        /// Creates one directory.
        [[nodiscard]] DirectoryCreateStatus CreateDirectory(FilePathView Path) noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return DirectoryCreateStatus::PathNotRepresentable;
            }

            if (mkdir(NativePath, 0775) == 0) {
                return DirectoryCreateStatus::Succeeded;
            }

            return errno == EEXIST ? DirectoryCreateStatus::AlreadyExists : DirectoryCreateStatus::IoFailure;
        }

        /// Removes one empty directory.
        [[nodiscard]] DirectoryRemoveStatus RemoveDirectory(FilePathView Path) noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return DirectoryRemoveStatus::PathNotRepresentable;
            }

            if (rmdir(NativePath) == 0) {
                return DirectoryRemoveStatus::Succeeded;
            }

            if (errno == ENOENT) {
                return DirectoryRemoveStatus::NotFound;
            }

            return errno == ENOTEMPTY ? DirectoryRemoveStatus::NotEmpty : DirectoryRemoveStatus::IoFailure;
        }

        /// TCallback is the caller-owned noexcept enumeration callback.
        template<FileEnumerationCallback TCallback>
        [[nodiscard]] FileEnumerationResult EnumerateDirectory(
            DirectoryPathView Directory,
            DestinationBufferView NameBuffer,
            TCallback& Callback
        ) const noexcept {
            char NativePath[NativePathCapacity];

            if (Directory.IsRoot()) {
                if (!MakeRootPath(NativePath)) {
                    return {FileEnumerationStatus::NotReady, StorageSize{}};
                }
            } else if (!MakeNativePath(Directory.Path(), NativePath)) {
                return {FileEnumerationStatus::PathNotRepresentable, StorageSize{}};
            }

            auto* Handle = opendir(NativePath);

            if (Handle == nullptr) {
                return {errno == ENOENT ? FileEnumerationStatus::NotFound : FileEnumerationStatus::IoFailure, StorageSize{}};
            }

            std::uint64_t Visited = 0U;

            while (auto* Entry = readdir(Handle)) {
                if (std::strcmp(Entry->d_name, ".") == 0 || std::strcmp(Entry->d_name, "..") == 0) {
                    continue;
                }

                const auto CompleteSize = std::strlen(Entry->d_name);
                auto DeliveredSize = CompleteSize < NameBuffer.Capacity ? CompleteSize : NameBuffer.Capacity;

                while (DeliveredSize != 0U && (static_cast<unsigned char>(Entry->d_name[DeliveredSize]) & 0xC0U) == 0x80U) {
                    --DeliveredSize;
                }

                if (DeliveredSize != 0U) {
                    std::memcpy(
                        NameBuffer.Address,
                        Entry->d_name,
                        DeliveredSize
                    );
                }

                std::uint8_t Facts = CompleteSize > NameBuffer.Capacity
                    ? static_cast<std::uint8_t>(FileEnumerationEntryFact::NameWasTruncated)
                    : CompleteSize < NameBuffer.Capacity
                        ? static_cast<std::uint8_t>(FileEnumerationEntryFact::NameIsSmallerThanAvailableBuffer)
                        : 0U;

                const auto IsDirectory = Entry->d_type == DT_DIR;
                StorageSize FileSize{};

                const FileEnumerationEntry Observation{
                    Detail::PersistenceProviderAccess::MakeTextView(
                        static_cast<const char*>(NameBuffer.Address),
                        DeliveredSize
                    ),
                    StorageSize{CompleteSize},
                    FileSize,
                    Facts,
                    IsDirectory ? FileEntryKind::Directory : FileEntryKind::File
                };

                ++Visited;

                if (Callback(Observation) == EnumerationControl::Stop) {
                    closedir(Handle);
                    return {FileEnumerationStatus::StoppedByCallback, StorageSize{Visited}};
                }
            }

            closedir(Handle);
            return {FileEnumerationStatus::Completed, StorageSize{Visited}};
        }

        /// Renames an entry without overwriting an existing destination.
        [[nodiscard]] FileRenameStatus RenameEntry(
            FilePathView Source,
            FilePathView Destination
        ) noexcept {
            char NativeSource[NativePathCapacity];
            char NativeDestination[NativePathCapacity];

            if (!MakeNativePath(Source, NativeSource) || !MakeNativePath(Destination, NativeDestination)) {
                return FileRenameStatus::PathNotRepresentable;
            }

            struct stat Information {};

            if (stat(NativeSource, &Information) != 0) {
                return FileRenameStatus::SourceNotFound;
            }

            if (stat(NativeDestination, &Information) == 0) {
                return FileRenameStatus::DestinationAlreadyExists;
            }

            return std::rename(
                NativeSource,
                NativeDestination
            ) == 0 ? FileRenameStatus::Succeeded : FileRenameStatus::IoFailure;
        }

        /// Appends a complete source buffer to an existing file.
        [[nodiscard]] FileAppendStatus AppendFile(
            FilePathView Path,
            SourceBufferView Source
        ) noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return FileAppendStatus::PathNotRepresentable;
            }

            struct stat Information {};

            if (stat(NativePath, &Information) != 0) {
                return FileAppendStatus::NotFound;
            }

            if (!S_ISREG(Information.st_mode)) {
                return FileAppendStatus::EntryTypeConflict;
            }

            const auto ExistingSize = static_cast<std::uint64_t>(Information.st_size);

            if (
                ExistingSize > TBindingProfile::MaximumFileSize.RawValue ||
                Source.Size > TBindingProfile::MaximumFileSize.RawValue - ExistingSize
            ) {
                return FileAppendStatus::FileTooLarge;
            }

            auto* File = std::fopen(
                NativePath,
                "ab"
            );

            if (File == nullptr) {
                return FileAppendStatus::IoFailure;
            }

            const auto Written = Source.Size == 0U ? 0U : std::fwrite(Source.Address, 1U, Source.Size, File);
            const auto Committed = CommitFileMutation(File);
            return Written == Source.Size && Committed ? FileAppendStatus::Succeeded : FileAppendStatus::IoFailure;
        }

        /// Replaces bytes within an existing file extent.
        [[nodiscard]] FileWriteAtStatus WriteFileAt(
            FilePathView Path,
            StorageOffset Offset,
            SourceBufferView Source
        ) noexcept {
            const auto SizeResult = GetFileSize(Path);

            if (SizeResult.Status == FileSizeStatus::NotFound) {
                return FileWriteAtStatus::NotFound;
            }

            if (SizeResult.Status != FileSizeStatus::Succeeded) {
                return FileWriteAtStatus::IoFailure;
            }

            if (Offset.RawValue > SizeResult.Size.RawValue || Source.Size > SizeResult.Size.RawValue - Offset.RawValue || Offset.RawValue > static_cast<std::uint64_t>(LONG_MAX)) {
                return FileWriteAtStatus::RangeOutOfBounds;
            }

            char NativePath[NativePathCapacity];

            if (!MakeNativePath(Path, NativePath)) {
                return FileWriteAtStatus::PathNotRepresentable;
            }

            auto* File = std::fopen(
                NativePath,
                "r+b"
            );

            if (File == nullptr || std::fseek(File, static_cast<long>(Offset.RawValue), SEEK_SET) != 0) {
                if (File != nullptr) {
                    std::fclose(File);
                }

                return FileWriteAtStatus::IoFailure;
            }

            const auto Written = Source.Size == 0U ? 0U : std::fwrite(Source.Address, 1U, Source.Size, File);
            const auto Committed = CommitFileMutation(File);
            return Written == Source.Size && Committed ? FileWriteAtStatus::Succeeded : FileWriteAtStatus::IoFailure;
        }


    };

} // ESPressio::Persistence::EspIdf
