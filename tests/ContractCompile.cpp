#include <memory/ByteOperationsProvider.hpp>

#include <ESPressio_Persistence_ESP_IDF.hpp>

namespace {

    namespace Framework = ESPressio::System::CompositionFramework;

    /// Logical provider identity used by the contract probe.
    struct ContractBinding final {};


    // Concrete contract-probe types.

    /// Standard C/C++ ByteOperations provider selected for architecture validation.
    using ContractByteOperationsProvider =
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider;

    /// Conservative VFS profile used only for compile-time contract validation.
    using ContractVfsProfile =
        ESPressio::Persistence::EspIdf::VfsBindingProfile<
            ESPressio::Persistence::RetentionLevel::Restart,
            ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
            ESPressio::Persistence::MediaRemovability::Fixed,
            255U,
            255U,
            0x7FFFFFFFULL
        >;

    /// Concrete FileStorage provider type validated by this probe.
    using ContractFileStorageProvider =
        ESPressio::Persistence::EspIdf::VfsFileStorage<
            ContractBinding,
            ContractVfsProfile,
            ContractByteOperationsProvider
        >;

    /// Concrete KeyValueStorage provider type validated by this probe.
    using ContractKeyValueStorageProvider =
        ESPressio::Persistence::EspIdf::NvsKeyValueStorage<
            ContractBinding,
            ContractByteOperationsProvider
        >;

    /// Memory-domain Composition supplying the cross-domain ByteOperations dependency.
    using ContractMemoryComposition = Framework::Composition<
        ESPressio::Memory::Domain,
        ContractByteOperationsProvider
    >;

    /// Persistence-domain Composition containing both concrete providers.
    using ContractPersistenceComposition = Framework::Composition<
        ESPressio::Persistence::Domain,
        ContractFileStorageProvider,
        ContractKeyValueStorageProvider
    >;

    /// Complete architecture proving the Persistence -> Memory dependency is satisfied.
    using ContractArchitecture = Framework::Architecture<
        ContractMemoryComposition,
        ContractPersistenceComposition
    >;


    static_assert(ContractArchitecture::IsValid);

    static_assert([]() consteval {
        ESPressio::Persistence::ValidatePersistenceProvider<
            ContractFileStorageProvider
        >();
        ESPressio::Persistence::ValidatePersistenceProvider<
            ContractKeyValueStorageProvider
        >();
        return true;
    }());

} // namespace
