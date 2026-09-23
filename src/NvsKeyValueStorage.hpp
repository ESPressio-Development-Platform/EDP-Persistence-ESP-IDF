#pragma once

#include <nvs.h>

#include <ESPressio_Persistence.hpp>
#include <memory/ByteOperationsContract.hpp>

namespace ESPressio::Persistence::EspIdf {

    namespace Framework = ESPressio::System::CompositionFramework;


    /// Result of opening the NVS namespace owned by one provider instance.
    enum class NvsOpenStatus : std::uint8_t {
        Succeeded = 0U,
        ProviderFailure = 1U
    };


    /// Adapts one ESP-IDF NVS namespace to the EDP KeyValueStorage contract.
    ///
    /// @tparam TBindingTag Distinguishes independently selectable NVS namespaces.
    /// @tparam TByteOperationsProvider Supplies EDP-Memory raw byte-copy operations used by truncated reads.
    template<
        class TBindingTag,
        class TByteOperationsProvider
    >
    class NvsKeyValueStorage final : public Framework::Provider<
        Domain,
        Framework::Offers<
            Framework::Offer<
                KeyValueStorage,
                Framework::PropertyValue<KeyValueAccessMode, AccessMode::ReadWrite>,
                Framework::PropertyValue<KeyValueRetention, RetentionLevel::PowerLoss>,
                Framework::PropertyValue<KeyCaseSensitivity, TextCaseSensitivity::CaseSensitive>,
                Framework::PropertyValue<KeyValueMediaRemovability, MediaRemovability::Fixed>,
                Framework::PropertyValue<MaximumKeyBytes, std::size_t{15U}>,
                Framework::PropertyValue<MaximumKeyValueSize, StorageSize{512U}>,
                Framework::PropertyValue<KeyEnumerationSupport, Support::Unsupported>,
                Framework::PropertyValue<ReadValueAtSupport, Support::Unsupported>,
                Framework::PropertyValue<ClearAllSupport, Support::Supported>,
                Framework::PropertyValue<KeyValueCapacityReportingSupport, Support::Unsupported>,
                Framework::PropertyValue<KeyValueInvocationConcurrency, InvocationConcurrency::CallerSerialized>,
                Framework::PropertyValue<KeyValueFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<KeyValueInterruptionAtomicity, InterruptionAtomicity::PowerLoss>,
                Framework::PropertyValue<ClearAllFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<ClearAllInterruptionAtomicity, InterruptionAtomicity::None>
            >
        >,
        Framework::Contract<
            Framework::Requirement<
                ESPressio::Memory::ByteOperations,
                Framework::RequirementScope::ExternalDomain,
                Framework::ExactlyProviders<1U>
            >
        >
    > {
    private:

        static_assert(
            sizeof(
                ESPressio::Memory::Detail::ByteOperationsProviderTraits<
                    TByteOperationsProvider
                >
            ) > 0U,
            "ESP-IDF NvsKeyValueStorage requires an EDP-Memory ByteOperations provider"
        );


        // Bound dependencies.

        /// Open ESP-IDF NVS handle.
        nvs_handle_t Handle_;

        /// Non-owning EDP-Memory byte-operation provider used for bounded raw copies.
        const TByteOperationsProvider* ByteOperations_;

        /// Bounded scratch space used only when the caller requests a truncated blob read.
        mutable std::uint8_t ReadScratch_[512U];

        /// Indicates whether Handle_ is currently open.
        bool IsReady_;

        // Native key representation helpers.

        /// Result of converting an EDP key to the native NVS key representation.
        enum class KeyCopyStatus : std::uint8_t {
            Succeeded = 0U,
            TooLong = 1U,
            NotRepresentable = 2U
        };

        /// Copies an EDP key into NVS's documented ASCII key representation.
        [[nodiscard]] static KeyCopyStatus CopyKey(
            KeyView Key,
            char (&Buffer)[16U]
        ) noexcept {
            if (Key.Size() > 15U) {
                return KeyCopyStatus::TooLong;
            }

            for (std::size_t Index = 0U; Index < Key.Size(); ++Index) {
                const auto Byte = static_cast<unsigned char>(Key.Data()[Index]);

                if (Byte > 0x7FU) {
                    return KeyCopyStatus::NotRepresentable;
                }

                Buffer[Index] = Key.Data()[Index];
            }

            Buffer[Key.Size()] = '\0';
            return KeyCopyStatus::Succeeded;
        }

    public:

        // Lifecycle controlled by Bootstrap/application wiring.

        /// Constructs an unopened provider using one caller-owned ByteOperations provider.
        explicit NvsKeyValueStorage(
            const TByteOperationsProvider& ByteOperations
        ) noexcept
            : Handle_(0U),
              ByteOperations_(&ByteOperations),
              IsReady_(false) {}

        /// Opens one caller-selected namespace in an initialized NVS partition.
        [[nodiscard]] NvsOpenStatus Open(
            const char* Namespace,
            const char* Partition = NVS_DEFAULT_PART_NAME
        ) noexcept {
            if (IsReady_) {
                return NvsOpenStatus::Succeeded;
            }

            IsReady_ = nvs_open_from_partition(
                Partition,
                Namespace,
                NVS_READWRITE,
                &Handle_
            ) == ESP_OK;

            return IsReady_
                ? NvsOpenStatus::Succeeded
                : NvsOpenStatus::ProviderFailure;
        }

        /// Closes the NVS handle.
        void Close() noexcept {
            if (!IsReady_) {
                return;
            }

            nvs_close(Handle_);
            Handle_ = 0U;
            IsReady_ = false;
        }

        // KeyValueStorage contract.

        /// Reports whether the NVS namespace is open.
        [[nodiscard]] bool IsKeyValueStorageReady() const noexcept {
            return IsReady_;
        }

        /// Returns the complete blob size for one key.
        [[nodiscard]] KeyValueSizeResult GetValueSize(KeyView Key) const noexcept {
            if (!IsReady_) {
                return {KeyValueSizeStatus::NotReady, StorageSize{}};
            }

            char NativeKey[16U];

            const auto KeyStatus = CopyKey(
                Key,
                NativeKey
            );

            if (KeyStatus == KeyCopyStatus::TooLong) {
                return {KeyValueSizeStatus::KeyTooLong, StorageSize{}};
            }

            if (KeyStatus == KeyCopyStatus::NotRepresentable) {
                return {KeyValueSizeStatus::KeyNotRepresentable, StorageSize{}};
            }

            std::size_t Size = 0U;
            const auto Result = nvs_get_blob(
                Handle_,
                NativeKey,
                nullptr,
                &Size
            );

            if (Result == ESP_ERR_NVS_NOT_FOUND) {
                return {KeyValueSizeStatus::NotFound, StorageSize{}};
            }

            if (Result != ESP_OK) {
                return {KeyValueSizeStatus::IoFailure, StorageSize{}};
            }

            if (Size > 512U) {
                return {KeyValueSizeStatus::ProviderFailure, StorageSize{}};
            }

            return {KeyValueSizeStatus::Succeeded, StorageSize{Size}};
        }

        /// Reads as much of one blob as the caller destination can hold.
        [[nodiscard]] KeyValueReadResult ReadValue(
            KeyView Key,
            DestinationBufferView Destination
        ) const noexcept {
            if (!IsReady_) {
                return {KeyValueReadStatus::NotReady, 0U, 0U, StorageSize{}};
            }

            char NativeKey[16U];

            const auto KeyStatus = CopyKey(
                Key,
                NativeKey
            );

            if (KeyStatus == KeyCopyStatus::TooLong) {
                return {KeyValueReadStatus::KeyTooLong, 0U, 0U, StorageSize{}};
            }

            if (KeyStatus == KeyCopyStatus::NotRepresentable) {
                return {KeyValueReadStatus::KeyNotRepresentable, 0U, 0U, StorageSize{}};
            }

            std::size_t CompleteSize = 0U;
            auto Result = nvs_get_blob(
                Handle_,
                NativeKey,
                nullptr,
                &CompleteSize
            );

            if (Result == ESP_ERR_NVS_NOT_FOUND) {
                return {KeyValueReadStatus::NotFound, 0U, 0U, StorageSize{}};
            }

            if (Result != ESP_OK) {
                return {KeyValueReadStatus::IoFailure, 0U, 0U, StorageSize{}};
            }

            if (CompleteSize > 512U) {
                return {KeyValueReadStatus::ProviderFailure, 0U, 0U, StorageSize{}};
            }

            const auto TransferSize = CompleteSize < Destination.Capacity ? CompleteSize : Destination.Capacity;

            if (TransferSize != 0U) {
                if (CompleteSize <= Destination.Capacity) {
                    std::size_t RequestedSize = CompleteSize;
                    Result = nvs_get_blob(
                        Handle_,
                        NativeKey,
                        Destination.Address,
                        &RequestedSize
                    );

                    if (Result != ESP_OK || RequestedSize != CompleteSize) {
                        return {KeyValueReadStatus::IoFailure, 0U, 0U, StorageSize{}};
                    }
                } else {
                    std::size_t RequestedSize = CompleteSize;
                    Result = nvs_get_blob(
                        Handle_,
                        NativeKey,
                        ReadScratch_,
                        &RequestedSize
                    );

                    if (Result != ESP_OK || RequestedSize != CompleteSize) {
                        return {KeyValueReadStatus::IoFailure, 0U, 0U, StorageSize{}};
                    }

                    ByteOperations_->CopyBytes(
                        Destination.Address,
                        ReadScratch_,
                        TransferSize
                    );
                }
            }

            std::uint8_t Facts = static_cast<std::uint8_t>(ReadFact::AvailableDataSizeIsKnown);

            if (CompleteSize > Destination.Capacity) {
                Facts |= static_cast<std::uint8_t>(ReadFact::WasTruncated);
            } else if (CompleteSize < Destination.Capacity) {
                Facts |= static_cast<std::uint8_t>(ReadFact::IsSmallerThanAvailableBuffer);
            }

            return {
                KeyValueReadStatus::Succeeded,
                Facts,
                TransferSize,
                StorageSize{CompleteSize}
            };
        }

        /// Stores and commits one complete opaque blob.
        [[nodiscard]] KeyValueStoreStatus StoreValue(
            KeyView Key,
            SourceBufferView Source
        ) noexcept {
            if (!IsReady_) {
                return KeyValueStoreStatus::NotReady;
            }

            char NativeKey[16U];

            const auto KeyStatus = CopyKey(
                Key,
                NativeKey
            );

            if (KeyStatus == KeyCopyStatus::TooLong) {
                return KeyValueStoreStatus::KeyTooLong;
            }

            if (KeyStatus == KeyCopyStatus::NotRepresentable) {
                return KeyValueStoreStatus::KeyNotRepresentable;
            }

            if (Source.Size > 512U) {
                return KeyValueStoreStatus::ValueTooLarge;
            }

            static constexpr std::uint8_t EmptyValueStorage = 0U;
            const auto* Storage = Source.Size == 0U
                ? static_cast<const void*>(&EmptyValueStorage)
                : Source.Address;

            if (nvs_set_blob(
                Handle_,
                NativeKey,
                Storage,
                Source.Size
            ) != ESP_OK) {
                return KeyValueStoreStatus::IoFailure;
            }

            return nvs_commit(Handle_) == ESP_OK ? KeyValueStoreStatus::Succeeded : KeyValueStoreStatus::IoFailure;
        }

        /// Removes and commits one key.
        [[nodiscard]] KeyValueRemoveStatus RemoveKey(KeyView Key) noexcept {
            if (!IsReady_) {
                return KeyValueRemoveStatus::NotReady;
            }

            char NativeKey[16U];

            const auto KeyStatus = CopyKey(
                Key,
                NativeKey
            );

            if (KeyStatus == KeyCopyStatus::TooLong) {
                return KeyValueRemoveStatus::KeyTooLong;
            }

            if (KeyStatus == KeyCopyStatus::NotRepresentable) {
                return KeyValueRemoveStatus::KeyNotRepresentable;
            }

            const auto Result = nvs_erase_key(
                Handle_,
                NativeKey
            );

            if (Result == ESP_ERR_NVS_NOT_FOUND) {
                return KeyValueRemoveStatus::NotFound;
            }

            if (Result != ESP_OK) {
                return KeyValueRemoveStatus::IoFailure;
            }

            return nvs_commit(Handle_) == ESP_OK ? KeyValueRemoveStatus::Succeeded : KeyValueRemoveStatus::IoFailure;
        }

        /// Removes and commits all keys in the bound namespace.
        [[nodiscard]] KeyValueClearStatus ClearAllKeys() noexcept {
            if (!IsReady_) {
                return KeyValueClearStatus::NotReady;
            }

            if (nvs_erase_all(Handle_) != ESP_OK) {
                return KeyValueClearStatus::IoFailure;
            }

            return nvs_commit(Handle_) == ESP_OK ? KeyValueClearStatus::Succeeded : KeyValueClearStatus::IoFailure;
        }

    };

} // ESPressio::Persistence::EspIdf
