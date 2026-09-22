#include <cstdint>
#include <cstdio>

#include <esp_err.h>
#include <esp_vfs_fat.h>
#include <nvs_flash.h>
#include <wear_levelling.h>

#include <memory/ByteOperationsProvider.hpp>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    using namespace ESPressio::Persistence;

    /// VFS root used by the internal-flash FAT filesystem.
    constexpr char BasePath[] = "/edpfs";

    /// Partition label used by the demo FAT filesystem.
    constexpr char PartitionLabel[] = "storage";

    /// Logical Composition identity used by this demo.
    struct DemoBinding final {};


    // Demo provider types.

    /// Standard C/C++ ByteOperations concrete selected by the demo.
    using DemoByteOperationsProvider =
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider;

    /// Conservative semantic profile for the fixed internal FAT filesystem.
    using DemoFileProfile = EspIdf::VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseInsensitive,
        MediaRemovability::Fixed,
        63U,
        31U,
        4096ULL
    >;

    /// Concrete ESP-IDF VFS provider used by the demo.
    using DemoFileStorage = EspIdf::VfsFileStorage<
        DemoBinding,
        DemoFileProfile,
        DemoByteOperationsProvider
    >;

    /// Concrete ESP-IDF NVS provider used by the demo.
    using DemoKeyValueStorage =
        EspIdf::NvsKeyValueStorage<
            DemoBinding,
            DemoByteOperationsProvider
        >;


    /// Result of one demo operation group.
    enum class DemoStatus : std::uint8_t {
        Succeeded = 0U,
        Failed = 1U
    };


    // Runtime resources.

    /// Wear-levelling handle owned by the mounted FAT filesystem.
    wl_handle_t WearLevellingHandle = WL_INVALID_HANDLE;


    /// Initializes the default NVS partition, erasing it only for the documented recoverable states.
    [[nodiscard]] DemoStatus InitializeNvs() {
        auto Result = nvs_flash_init();

        if (Result == ESP_ERR_NVS_NO_FREE_PAGES ||
            Result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            if (nvs_flash_erase() != ESP_OK) {
                return DemoStatus::Failed;
            }

            Result = nvs_flash_init();
        }

        return Result == ESP_OK
            ? DemoStatus::Succeeded
            : DemoStatus::Failed;
    }


    /// Mounts the dedicated internal-flash FAT filesystem used by the FileStorage demo.
    [[nodiscard]] DemoStatus MountFileSystem() {
        esp_vfs_fat_mount_config_t Configuration{};
        Configuration.format_if_mount_failed = true;
        Configuration.max_files = 4U;
        Configuration.allocation_unit_size = 4096U;

        return esp_vfs_fat_spiflash_mount_rw_wl(
            BasePath,
            PartitionLabel,
            &Configuration,
            &WearLevellingHandle
        ) == ESP_OK
            ? DemoStatus::Succeeded
            : DemoStatus::Failed;
    }


    /// Exercises VfsFileStorage against the mounted internal-flash FAT filesystem.
    [[nodiscard]] DemoStatus RunFileStorageDemo(
        DemoByteOperationsProvider& ByteOperations
    ) {
        DemoFileStorage Storage(
            BasePath,
            ByteOperations
        );

        if (!Storage.IsFileStorageReady()) {
            std::printf("FileStorage: VFS base path is not ready\n");
            return DemoStatus::Failed;
        }

        constexpr auto Path = FilePathView::Validate("edp-demo.bin");
        static_assert(Path.Status == FilePathValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {
            0x31U,
            0x32U,
            0x33U,
            0x34U
        };

        if (Storage.ReplaceFile(
            Path.Value,
            SourceBufferView{
                Payload,
                sizeof(Payload)
            }
        ) != FileReplaceStatus::Succeeded) {
            std::printf("FileStorage: replace failed\n");
            return DemoStatus::Failed;
        }

        std::uint8_t Buffer[sizeof(Payload)]{};
        const auto Read = Storage.ReadFileAt(
            Path.Value,
            StorageOffset{},
            DestinationBufferView{
                Buffer,
                sizeof(Buffer)
            }
        );

        const bool IsReadMatch =
            Read.Status == FileReadStatus::Succeeded &&
            Read.BytesTransferred == sizeof(Payload) &&
            Buffer[0] == Payload[0] &&
            Buffer[1] == Payload[1] &&
            Buffer[2] == Payload[2] &&
            Buffer[3] == Payload[3];

        const auto RemoveStatus = Storage.RemoveFile(Path.Value);

        if (!IsReadMatch || RemoveStatus != FileRemoveStatus::Succeeded) {
            std::printf("FileStorage: verification failed\n");
            return DemoStatus::Failed;
        }

        std::printf("FileStorage: PASS\n");
        return DemoStatus::Succeeded;
    }


    /// Exercises NvsKeyValueStorage against the initialized default NVS partition.
    [[nodiscard]] DemoStatus RunKeyValueStorageDemo(
        DemoByteOperationsProvider& ByteOperations
    ) {
        DemoKeyValueStorage Storage(ByteOperations);

        if (Storage.Open(
            "edp-demo"
        ) != EspIdf::NvsOpenStatus::Succeeded) {
            std::printf("KeyValueStorage: NVS open failed\n");
            return DemoStatus::Failed;
        }

        constexpr auto Key = KeyView::Validate("payload");
        static_assert(Key.Status == KeyValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {
            0x71U,
            0x72U,
            0x73U
        };

        if (Storage.StoreValue(
            Key.Value,
            SourceBufferView{
                Payload,
                sizeof(Payload)
            }
        ) != KeyValueStoreStatus::Succeeded) {
            std::printf("KeyValueStorage: store failed\n");
            Storage.Close();
            return DemoStatus::Failed;
        }

        std::uint8_t Buffer[sizeof(Payload)]{};
        const auto Read = Storage.ReadValue(
            Key.Value,
            DestinationBufferView{
                Buffer,
                sizeof(Buffer)
            }
        );

        const bool IsReadMatch =
            Read.Status == KeyValueReadStatus::Succeeded &&
            Read.BytesTransferred == sizeof(Payload) &&
            Buffer[0] == Payload[0] &&
            Buffer[1] == Payload[1] &&
            Buffer[2] == Payload[2];

        const auto EmptyStoreStatus = Storage.StoreValue(
            Key.Value,
            SourceBufferView{
                nullptr,
                0U
            }
        );
        const auto EmptySize = Storage.GetValueSize(Key.Value);
        const auto RemoveStatus = Storage.RemoveKey(Key.Value);

        Storage.Close();

        const bool IsEmptyValueValid =
            EmptyStoreStatus == KeyValueStoreStatus::Succeeded &&
            EmptySize.Status == KeyValueSizeStatus::Succeeded &&
            EmptySize.Size.RawValue == 0U;

        if (!IsReadMatch ||
            !IsEmptyValueValid ||
            RemoveStatus != KeyValueRemoveStatus::Succeeded) {
            std::printf("KeyValueStorage: verification failed\n");
            return DemoStatus::Failed;
        }

        std::printf("KeyValueStorage: PASS\n");
        return DemoStatus::Succeeded;
    }

} // namespace


extern "C" void app_main() {
    DemoByteOperationsProvider ByteOperations;

    const auto NvsStatus = InitializeNvs();
    const auto FileSystemStatus = MountFileSystem();

    const auto FileStatus =
        FileSystemStatus == DemoStatus::Succeeded
            ? RunFileStorageDemo(ByteOperations)
            : DemoStatus::Failed;
    const auto KeyValueStatus =
        NvsStatus == DemoStatus::Succeeded
            ? RunKeyValueStorageDemo(ByteOperations)
            : DemoStatus::Failed;

    if (FileSystemStatus == DemoStatus::Succeeded) {
        static_cast<void>(
            esp_vfs_fat_spiflash_unmount_rw_wl(
                BasePath,
                WearLevellingHandle
            )
        );
    }

    std::printf(
        FileStatus == DemoStatus::Succeeded &&
        KeyValueStatus == DemoStatus::Succeeded
            ? "EDP-Persistence-ESP-IDF demo: PASS\n"
            : "EDP-Persistence-ESP-IDF demo: FAIL\n"
    );
}
