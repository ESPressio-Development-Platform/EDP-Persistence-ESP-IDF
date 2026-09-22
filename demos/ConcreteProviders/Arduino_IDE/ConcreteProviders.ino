#include <Arduino.h>
#include <SD.h>

#include <memory/ByteOperationsProvider.hpp>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    using namespace ESPressio::Persistence;

    /// Logical Composition identity used by this demo.
    struct DemoBinding final {};


    // Demo provider types.

    /// Standard C/C++ ByteOperations concrete selected by the demo.
    using DemoByteOperationsProvider =
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider;

    /// Conservative semantic profile for the removable FAT filesystem used by this demo.
    using DemoFileProfile = EspIdf::VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseInsensitive,
        MediaRemovability::Removable,
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


    /// Exercises VfsFileStorage against the VFS mount created by Arduino SD.
    [[nodiscard]] DemoStatus RunFileStorageDemo(
        DemoByteOperationsProvider& ByteOperations
    ) {
        if (!SD.begin()) {
            Serial.println("FileStorage: SD mount failed");
            return DemoStatus::Failed;
        }

        DemoFileStorage Storage(
            "/sd",
            ByteOperations
        );

        if (!Storage.IsFileStorageReady()) {
            Serial.println("FileStorage: VFS base path is not ready");
            return DemoStatus::Failed;
        }

        constexpr auto Path = FilePathView::Validate("edp-demo.bin");
        static_assert(Path.Status == FilePathValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {
            0x21U,
            0x22U,
            0x23U,
            0x24U
        };

        if (Storage.ReplaceFile(
            Path.Value,
            SourceBufferView{
                Payload,
                sizeof(Payload)
            }
        ) != FileReplaceStatus::Succeeded) {
            Serial.println("FileStorage: replace failed");
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
            Serial.println("FileStorage: verification failed");
            return DemoStatus::Failed;
        }

        Serial.println("FileStorage: PASS");
        return DemoStatus::Succeeded;
    }


    /// Exercises NvsKeyValueStorage against the Arduino runtime's initialized NVS partition.
    [[nodiscard]] DemoStatus RunKeyValueStorageDemo(
        DemoByteOperationsProvider& ByteOperations
    ) {
        DemoKeyValueStorage Storage(ByteOperations);

        if (Storage.Open(
            "edp-demo"
        ) != EspIdf::NvsOpenStatus::Succeeded) {
            Serial.println("KeyValueStorage: NVS open failed");
            return DemoStatus::Failed;
        }

        constexpr auto Key = KeyView::Validate("payload");
        static_assert(Key.Status == KeyValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {
            0x61U,
            0x62U,
            0x63U
        };

        if (Storage.StoreValue(
            Key.Value,
            SourceBufferView{
                Payload,
                sizeof(Payload)
            }
        ) != KeyValueStoreStatus::Succeeded) {
            Serial.println("KeyValueStorage: store failed");
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
            Serial.println("KeyValueStorage: verification failed");
            return DemoStatus::Failed;
        }

        Serial.println("KeyValueStorage: PASS");
        return DemoStatus::Succeeded;
    }


    /// Runs both concrete provider demonstrations.
    void RunDemo() {
        DemoByteOperationsProvider ByteOperations;

        const auto FileStatus = RunFileStorageDemo(ByteOperations);
        const auto KeyValueStatus = RunKeyValueStorageDemo(ByteOperations);

        Serial.println(
            FileStatus == DemoStatus::Succeeded &&
            KeyValueStatus == DemoStatus::Succeeded
                ? "EDP-Persistence-ESP-IDF demo: PASS"
                : "EDP-Persistence-ESP-IDF demo: FAIL"
        );
    }

} // namespace


void setup() {
    Serial.begin(115200);
    delay(1000);
    RunDemo();
}


void loop() {
    delay(1000);
}
