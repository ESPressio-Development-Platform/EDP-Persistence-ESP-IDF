#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    struct ContractBinding final {};


    static_assert([]() consteval {
        ESPressio::Persistence::ValidatePersistenceProvider<
            ESPressio::Persistence::EspIdf::VfsFileStorage<ContractBinding>
        >();
        ESPressio::Persistence::ValidatePersistenceProvider<
            ESPressio::Persistence::EspIdf::NvsKeyValueStorage<ContractBinding>
        >();
        return true;
    }());

} // namespace
