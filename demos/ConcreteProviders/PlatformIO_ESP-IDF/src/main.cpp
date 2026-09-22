#include <cstdint>
#include <cstdio>

#include <esp_err.h>
#include <esp_vfs_fat.h>
#include <nvs_flash.h>
#include <wear_levelling.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    using namespace ESPressio::Persistence;

    constexpr char BasePath[] = "/edpfs";
    constexpr char PartitionLabel[] = "storage";

    struct DemoBinding final {};

    using DemoFileProfile = EspIdf::VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseInsensitive,
        MediaRemovability::Fixed,
        63U,
        31U,
        4096ULL
    >;

    using DemoFileStorage = EspIdf::VfsFileStorage<
        DemoBinding,
        DemoFileProfile
    >;

    using DemoKeyValueStorage =
        EspIdf::NvsKeyValueStorage<DemoBinding>;

    wl_handle_t WearLevellingHandle = WL_INVALID_HANDLE;


    [[nodiscard]] bool InitializeNvs() {
        auto Result = nvs_flash_init();

        if (Result == ESP_ERR_NVS_NO_FREE_PAGES ||
            Result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            if (nvs_flash_erase() != ESP_OK) {
                return false;
            }

            Result = nvs_flash_init();
        }

        return Result == ESP_OK;
    }


    [[nodiscard]] bool MountFileSystem() {
        esp_vfs_fat_mount_config_t Configuration{};
        Configuration.format_if_mount_failed = true;
        Configuration.max_files = 4U;
        Configuration.allocation_unit_size = 4096U;

        return esp_vfs_fat_spiflash_mount_rw_wl(
            BasePath,
            PartitionLabel,
            &Configuration,
            &WearLevellingHandle
        ) == ESP_OK;
    }


    [[nodiscard]] bool RunFileStorageDemo() {
        DemoFileStorage Storage(BasePath);

        if (!Storage.IsFileStorageReady()) {
            std::printf("FileStorage: VFS base path is not ready\n");
            return false;
        }

        constexpr auto Path = FilePathView::Validate("edp-demo.bin");
        static_assert(Path.Status == FilePathValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {0x31U, 0x32U, 0x33U, 0x34U};

        if (Storage.ReplaceFile(
            Path.Value,
            SourceBufferView{Payload, sizeof(Payload)}
        ) != FileReplaceStatus::Succeeded) {
            std::printf("FileStorage: replace failed\n");
            return false;
        }

        std::uint8_t Buffer[sizeof(Payload)]{};
        const auto Read = Storage.ReadFileAt(
            Path.Value,
            StorageOffset{},
            DestinationBufferView{Buffer, sizeof(Buffer)}
        );

        const bool ReadMatches =
            Read.Status == FileReadStatus::Succeeded &&
            Read.BytesTransferred == sizeof(Payload) &&
            Buffer[0] == Payload[0] &&
            Buffer[1] == Payload[1] &&
            Buffer[2] == Payload[2] &&
            Buffer[3] == Payload[3];

        const auto Remove = Storage.RemoveFile(Path.Value);

        if (!ReadMatches || Remove != FileRemoveStatus::Succeeded) {
            std::printf("FileStorage: verification failed\n");
            return false;
        }

        std::printf("FileStorage: PASS\n");
        return true;
    }


    [[nodiscard]] bool RunKeyValueStorageDemo() {
        DemoKeyValueStorage Storage;

        if (!Storage.Open("edp-demo")) {
            std::printf("KeyValueStorage: NVS open failed\n");
            return false;
        }

        constexpr auto Key = KeyView::Validate("payload");
        static_assert(Key.Status == KeyValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {0x71U, 0x72U, 0x73U};

        if (Storage.StoreValue(
            Key.Value,
            SourceBufferView{Payload, sizeof(Payload)}
        ) != KeyValueStoreStatus::Succeeded) {
            std::printf("KeyValueStorage: store failed\n");
            Storage.Close();
            return false;
        }

        std::uint8_t Buffer[sizeof(Payload)]{};
        const auto Read = Storage.ReadValue(
            Key.Value,
            DestinationBufferView{Buffer, sizeof(Buffer)}
        );

        const bool ReadMatches =
            Read.Status == KeyValueReadStatus::Succeeded &&
            Read.BytesTransferred == sizeof(Payload) &&
            Buffer[0] == Payload[0] &&
            Buffer[1] == Payload[1] &&
            Buffer[2] == Payload[2];

        const auto EmptyStore = Storage.StoreValue(
            Key.Value,
            SourceBufferView{nullptr, 0U}
        );
        const auto EmptySize = Storage.GetValueSize(Key.Value);
        const auto Remove = Storage.RemoveKey(Key.Value);

        Storage.Close();

        const bool EmptyValueWorks =
            EmptyStore == KeyValueStoreStatus::Succeeded &&
            EmptySize.Status == KeyValueSizeStatus::Succeeded &&
            EmptySize.Size.RawValue == 0U;

        if (!ReadMatches ||
            !EmptyValueWorks ||
            Remove != KeyValueRemoveStatus::Succeeded) {
            std::printf("KeyValueStorage: verification failed\n");
            return false;
        }

        std::printf("KeyValueStorage: PASS\n");
        return true;
    }

} // namespace


extern "C" void app_main() {
    const bool NvsReady = InitializeNvs();
    const bool FileSystemReady = MountFileSystem();

    const bool FilePassed =
        FileSystemReady && RunFileStorageDemo();
    const bool KeyValuePassed =
        NvsReady && RunKeyValueStorageDemo();

    if (FileSystemReady) {
        esp_vfs_fat_spiflash_unmount_rw_wl(
            BasePath,
            WearLevellingHandle
        );
    }

    std::printf(
        FilePassed && KeyValuePassed
            ? "EDP-Persistence-ESP-IDF demo: PASS\n"
            : "EDP-Persistence-ESP-IDF demo: FAIL\n"
    );
}
