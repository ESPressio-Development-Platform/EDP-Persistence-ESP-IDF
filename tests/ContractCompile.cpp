#include <memory/ByteOperationsProvider.hpp>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    struct ContractBinding final {};

    using ContractByteOperationsProvider =
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider;

    using ContractVfsProfile =
        ESPressio::Persistence::EspIdf::VfsBindingProfile<
            ESPressio::Persistence::RetentionLevel::Restart,
            ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
            ESPressio::Persistence::MediaRemovability::Fixed,
            255U,
            255U,
            0x7FFFFFFFULL
        >;


    static_assert([]() consteval {
        ESPressio::Persistence::ValidatePersistenceProvider<
            ESPressio::Persistence::EspIdf::VfsFileStorage<
                ContractBinding,
                ContractVfsProfile,
                ContractByteOperationsProvider
            >
        >();
        ESPressio::Persistence::ValidatePersistenceProvider<
            ESPressio::Persistence::EspIdf::NvsKeyValueStorage<
                ContractBinding,
                ContractByteOperationsProvider
            >
        >();
        return true;
    }());

} // namespace
