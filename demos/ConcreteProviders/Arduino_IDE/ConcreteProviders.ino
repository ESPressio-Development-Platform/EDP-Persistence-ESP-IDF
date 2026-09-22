#include <Arduino.h>
#include <SD.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    using namespace ESPressio::Persistence;

    struct DemoBinding final {};

    using DemoFileProfile = EspIdf::VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseInsensitive,
        MediaRemovability::Removable,
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


    bool RunFileStorageDemo() {
        if (!SD.begin()) {
            Serial.println("FileStorage: SD mount failed");
            return false;
        }

        DemoFileStorage Storage("/sd");

        if (!Storage.IsFileStorageReady()) {
            Serial.println("FileStorage: VFS base path is not ready");
            return false;
        }

        constexpr auto Path = FilePathView::Validate("edp-demo.bin");
        static_assert(Path.Status == FilePathValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {0x21U, 0x22U, 0x23U, 0x24U};

        if (Storage.ReplaceFile(
            Path.Value,
            SourceBufferView{Payload, sizeof(Payload)}
        ) != FileReplaceStatus::Succeeded) {
            Serial.println("FileStorage: replace failed");
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
            Serial.println("FileStorage: verification failed");
            return false;
        }

        Serial.println("FileStorage: PASS");
        return true;
    }


    bool RunKeyValueStorageDemo() {
        DemoKeyValueStorage Storage;

        if (!Storage.Open("edp-demo")) {
            Serial.println("KeyValueStorage: NVS open failed");
            return false;
        }

        constexpr auto Key = KeyView::Validate("payload");
        static_assert(Key.Status == KeyValidationStatus::Succeeded);

        const std::uint8_t Payload[] = {0x61U, 0x62U, 0x63U};

        if (Storage.StoreValue(
            Key.Value,
            SourceBufferView{Payload, sizeof(Payload)}
        ) != KeyValueStoreStatus::Succeeded) {
            Serial.println("KeyValueStorage: store failed");
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
            Serial.println("KeyValueStorage: verification failed");
            return false;
        }

        Serial.println("KeyValueStorage: PASS");
        return true;
    }


    void RunDemo() {
        const bool FilePassed = RunFileStorageDemo();
        const bool KeyValuePassed = RunKeyValueStorageDemo();

        Serial.println(
            FilePassed && KeyValuePassed
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
