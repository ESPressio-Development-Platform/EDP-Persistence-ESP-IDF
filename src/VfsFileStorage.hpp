#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <ESPressio_Persistence.hpp>

namespace ESPressio::Persistence::EspIdf {

    namespace Framework = ESPressio::System::CompositionFramework;


    /// TBindingTag distinguishes independently selectable ESP-IDF VFS roots in Composition.
    template<class TBindingTag>
    class VfsFileStorage final : public Framework::Provider<
        Domain,
        Framework::Provides<
            Framework::Offer<
                FileStorage,
                Framework::PropertyValue<FileAccessMode, AccessMode::ReadWrite>,
                Framework::PropertyValue<FileRetention, RetentionLevel::PowerLoss>,
                Framework::PropertyValue<FileHierarchyMode, FileHierarchy::Hierarchical>,
                Framework::PropertyValue<FilePathCaseSensitivity, TextCaseSensitivity::CaseSensitive>,
                Framework::PropertyValue<FileMediaRemovability, MediaRemovability::Fixed>,
                Framework::PropertyValue<MaximumPathBytes, std::size_t{255U}>,
                Framework::PropertyValue<MaximumPathSegmentBytes, std::size_t{255U}>,
                Framework::PropertyValue<MaximumFileSize, StorageSize{0x7FFFFFFFULL}>,
                Framework::PropertyValue<DirectoryMutationSupport, Support::Supported>,
                Framework::PropertyValue<DirectoryEnumerationSupport, Support::Supported>,
                Framework::PropertyValue<RenameSupport, Support::Supported>,
                Framework::PropertyValue<AppendSupport, Support::Supported>,
                Framework::PropertyValue<WriteFileAtSupport, Support::Supported>,
                Framework::PropertyValue<FileCapacityReportingSupport, Support::Supported>,
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

        /// Maximum native path assembled by this provider.
        static constexpr std::size_t NativePathCapacity = 512U;

        // Bound VFS root.

        /// Non-owning null-terminated VFS base path supplied by Bootstrap.
        const char* BasePath_;

        /// Builds a native path below BasePath_ without allocating.
        [[nodiscard]] bool MakeNativePath(
            FilePathView Path,
            char (&Buffer)[NativePathCapacity]
        ) const noexcept {
            if (BasePath_ == nullptr || Path.Size() > 255U) {
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

    public:

        /// Constructs a provider over an already-mounted VFS base path.
        explicit VfsFileStorage(const char* BasePath) noexcept
            : BasePath_(BasePath) {}

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

            return {FileSizeStatus::Succeeded, StorageSize{static_cast<std::uint64_t>(Information.st_size)}};
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
            const auto FlushResult = std::fflush(File);
            const auto CloseResult = std::fclose(File);

            return Written == Source.Size && FlushResult == 0 && CloseResult == 0 ? FileReplaceStatus::Succeeded : FileReplaceStatus::IoFailure;
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

                if (!IsDirectory) {
                    Facts |= static_cast<std::uint8_t>(FileEnumerationEntryFact::FileSizeIsKnown);
                }

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

            auto* File = std::fopen(
                NativePath,
                "ab"
            );

            if (File == nullptr) {
                return FileAppendStatus::IoFailure;
            }

            const auto Written = Source.Size == 0U ? 0U : std::fwrite(Source.Address, 1U, Source.Size, File);
            const auto FlushResult = std::fflush(File);
            const auto CloseResult = std::fclose(File);
            return Written == Source.Size && FlushResult == 0 && CloseResult == 0 ? FileAppendStatus::Succeeded : FileAppendStatus::IoFailure;
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
            const auto FlushResult = std::fflush(File);
            const auto CloseResult = std::fclose(File);
            return Written == Source.Size && FlushResult == 0 && CloseResult == 0 ? FileWriteAtStatus::Succeeded : FileWriteAtStatus::IoFailure;
        }

        /// Reports total and currently available capacity for the mounted VFS allocation domain.
        [[nodiscard]] CapacityQueryResult GetFileStorageCapacity() const noexcept {
            char NativePath[NativePathCapacity];

            if (!MakeRootPath(NativePath)) {
                return {CapacityQueryStatus::NotReady, StorageSize{}, StorageSize{}};
            }

            struct statvfs Information {};

            if (statvfs(NativePath, &Information) != 0) {
                return {CapacityQueryStatus::IoFailure, StorageSize{}, StorageSize{}};
            }

            return {
                CapacityQueryStatus::Succeeded,
                StorageSize{static_cast<std::uint64_t>(Information.f_blocks) * Information.f_frsize},
                StorageSize{static_cast<std::uint64_t>(Information.f_bavail) * Information.f_frsize}
            };
        }

    };

} // ESPressio::Persistence::EspIdf
