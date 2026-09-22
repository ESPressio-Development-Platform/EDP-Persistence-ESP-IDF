#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    using namespace ESPressio::Persistence;

    struct TestBinding final {};

    using TestFileProfile = EspIdf::VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseSensitive,
        MediaRemovability::Fixed,
        31U,
        15U,
        8ULL
    >;

    using TestFileStorage = EspIdf::VfsFileStorage<
        TestBinding,
        TestFileProfile
    >;

    using TestKeyValueStorage =
        EspIdf::NvsKeyValueStorage<TestBinding>;


    void TestFileStorage() {
        char Template[] = "/tmp/edp-persistence-idf-XXXXXX";
        char* Root = mkdtemp(Template);
        assert(Root != nullptr);

        {
            TestFileStorage Storage(Root);
            assert(Storage.IsFileStorageReady());

            constexpr auto Path = FilePathView::Validate("file.bin");
            constexpr auto Renamed = FilePathView::Validate("renamed.bin");
            constexpr auto Directory = FilePathView::Validate("dir");

            static_assert(Path.Status == FilePathValidationStatus::Succeeded);
            static_assert(Renamed.Status == FilePathValidationStatus::Succeeded);
            static_assert(Directory.Status == FilePathValidationStatus::Succeeded);

            const std::uint8_t Initial[] = {1U, 2U, 3U, 4U};
            assert(Storage.ReplaceFile(
                Path.Value,
                SourceBufferView{Initial, sizeof(Initial)}
            ) == FileReplaceStatus::Succeeded);

            std::uint8_t Truncated[2U]{};
            const auto TruncatedRead = Storage.ReadFileAt(
                Path.Value,
                StorageOffset{},
                DestinationBufferView{Truncated, sizeof(Truncated)}
            );

            assert(TruncatedRead.Status == FileReadStatus::Succeeded);
            assert(TruncatedRead.BytesTransferred == 2U);
            assert(TruncatedRead.AvailableDataSize.RawValue == 4U);
            assert(HasFact(
                TruncatedRead.Facts,
                ReadFact::WasTruncated
            ));

            const std::uint8_t Appended[] = {5U, 6U};
            assert(Storage.AppendFile(
                Path.Value,
                SourceBufferView{Appended, sizeof(Appended)}
            ) == FileAppendStatus::Succeeded);

            const std::uint8_t Replacement = 9U;
            assert(Storage.WriteFileAt(
                Path.Value,
                StorageOffset{1U},
                SourceBufferView{&Replacement, 1U}
            ) == FileWriteAtStatus::Succeeded);

            std::uint8_t Complete[6U]{};
            const auto CompleteRead = Storage.ReadFileAt(
                Path.Value,
                StorageOffset{},
                DestinationBufferView{Complete, sizeof(Complete)}
            );

            assert(CompleteRead.Status == FileReadStatus::Succeeded);
            assert(CompleteRead.BytesTransferred == sizeof(Complete));
            assert(Complete[0U] == 1U);
            assert(Complete[1U] == 9U);
            assert(Complete[4U] == 5U);
            assert(Complete[5U] == 6U);

            const std::uint8_t Oversized[9U]{};
            assert(Storage.ReplaceFile(
                Path.Value,
                SourceBufferView{Oversized, sizeof(Oversized)}
            ) == FileReplaceStatus::FileTooLarge);

            assert(Storage.RenameEntry(
                Path.Value,
                Renamed.Value
            ) == FileRenameStatus::Succeeded);
            assert(Storage.RemoveFile(
                Renamed.Value
            ) == FileRemoveStatus::Succeeded);

            assert(Storage.CreateDirectory(
                Directory.Value
            ) == DirectoryCreateStatus::Succeeded);
            assert(Storage.RemoveDirectory(
                Directory.Value
            ) == DirectoryRemoveStatus::Succeeded);
        }

        assert(rmdir(Root) == 0);
    }


    void TestKeyValueStorage() {
        TestKeyValueStorage Storage;
        assert(Storage.Open("test"));

        constexpr auto Key = KeyView::Validate("payload");
        constexpr auto OtherKey = KeyView::Validate("other");
        constexpr auto NonAscii = KeyView::Validate("\xC3\xA9");

        static_assert(Key.Status == KeyValidationStatus::Succeeded);
        static_assert(OtherKey.Status == KeyValidationStatus::Succeeded);
        static_assert(NonAscii.Status == KeyValidationStatus::Succeeded);

        const std::uint8_t Value[] = {10U, 11U, 12U};
        assert(Storage.StoreValue(
            Key.Value,
            SourceBufferView{Value, sizeof(Value)}
        ) == KeyValueStoreStatus::Succeeded);

        std::uint8_t Truncated[2U]{};
        const auto Read = Storage.ReadValue(
            Key.Value,
            DestinationBufferView{Truncated, sizeof(Truncated)}
        );

        assert(Read.Status == KeyValueReadStatus::Succeeded);
        assert(Read.BytesTransferred == sizeof(Truncated));
        assert(Read.AvailableDataSize.RawValue == sizeof(Value));
        assert(HasFact(
            Read.Facts,
            ReadFact::WasTruncated
        ));

        assert(Storage.StoreValue(
            NonAscii.Value,
            SourceBufferView{Value, sizeof(Value)}
        ) == KeyValueStoreStatus::KeyNotRepresentable);

        assert(Storage.StoreValue(
            Key.Value,
            SourceBufferView{nullptr, 0U}
        ) == KeyValueStoreStatus::Succeeded);

        const auto EmptySize = Storage.GetValueSize(Key.Value);
        assert(EmptySize.Status == KeyValueSizeStatus::Succeeded);
        assert(EmptySize.Size.RawValue == 0U);

        assert(Storage.StoreValue(
            OtherKey.Value,
            SourceBufferView{Value, sizeof(Value)}
        ) == KeyValueStoreStatus::Succeeded);
        assert(Storage.ClearAllKeys() == KeyValueClearStatus::Succeeded);
        assert(Storage.GetValueSize(Key.Value).Status == KeyValueSizeStatus::NotFound);
        assert(Storage.GetValueSize(OtherKey.Value).Status == KeyValueSizeStatus::NotFound);

        Storage.Close();
    }

} // namespace


int main() {
    TestFileStorage();
    TestKeyValueStorage();
    return 0;
}
