#include <type_traits>

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

    /// VFS profile proving that an explicitly concurrent binding is selectable.
    using ConcurrentReadVfsProfile =
        ESPressio::Persistence::EspIdf::VfsBindingProfile<
            ESPressio::Persistence::RetentionLevel::Restart,
            ESPressio::Persistence::TextCaseSensitivity::CaseSensitive,
            ESPressio::Persistence::MediaRemovability::Fixed,
            255U,
            255U,
            0x7FFFFFFFULL,
            ESPressio::Persistence::InvocationConcurrency::ConcurrentReads
        >;

    /// FileStorage provider whose mounted VFS explicitly guarantees concurrent reads.
    using ConcurrentReadFileStorageProvider =
        ESPressio::Persistence::EspIdf::VfsFileStorage<
            ContractBinding,
            ConcurrentReadVfsProfile,
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

    /// Persistence Composition containing the explicitly concurrent-read VFS provider.
    using ConcurrentReadPersistenceComposition = Framework::Composition<
        ESPressio::Persistence::Domain,
        ConcurrentReadFileStorageProvider
    >;

    /// Consumer requirement equivalent to EDP-Localisation FilePackSource's concurrency gate.
    using ConcurrentReadFileStorageNeed = Framework::Need<
        ESPressio::Persistence::FileStorage,
        Framework::AtLeast<
            ESPressio::Persistence::FileInvocationConcurrency,
            ESPressio::Persistence::InvocationConcurrency::ConcurrentReads
        >
    >;

    /// Provider selected only if the advertised binding guarantee meets ConcurrentReads.
    using SelectedConcurrentReadProvider =
        typename ConcurrentReadPersistenceComposition::template ProviderSatisfying<
            ConcurrentReadFileStorageNeed
        >;

    /// Complete architecture proving the ConcurrentReads provider's Memory dependency is satisfied.
    using ConcurrentReadArchitecture = Framework::Architecture<
        ContractMemoryComposition,
        ConcurrentReadPersistenceComposition
    >;


    /// Complete architecture proving the Persistence -> Memory dependency is satisfied.
    using ContractArchitecture = Framework::Architecture<
        ContractMemoryComposition,
        ContractPersistenceComposition
    >;


    static_assert(ContractArchitecture::IsValid);
    static_assert(ConcurrentReadArchitecture::IsValid);

    static_assert(
        std::is_same_v<
            SelectedConcurrentReadProvider,
            ConcurrentReadFileStorageProvider
        >
    );

    static_assert(
        ESPressio::Persistence::EspIdf::Detail::BindingConcurrency<
            ContractVfsProfile
        >() == ESPressio::Persistence::InvocationConcurrency::CallerSerialized
    );

    static_assert(
        ESPressio::Persistence::EspIdf::Detail::BindingConcurrency<
            ConcurrentReadVfsProfile
        >() == ESPressio::Persistence::InvocationConcurrency::ConcurrentReads
    );

    static_assert([]() consteval {
        ESPressio::Persistence::ValidatePersistenceProvider<
            ContractFileStorageProvider
        >();
        ESPressio::Persistence::ValidatePersistenceProvider<
            ConcurrentReadFileStorageProvider
        >();
        ESPressio::Persistence::ValidatePersistenceProvider<
            ContractKeyValueStorageProvider
        >();
        return true;
    }());

} // namespace
