#pragma once

#include <cstring>

#include <nvs.h>

#include <ESPressio_Persistence.hpp>

namespace ESPressio::Persistence::EspIdf {

    namespace Framework = ESPressio::System::CompositionFramework;


    /// TBindingTag distinguishes independently selectable NVS namespaces in Composition.
    template<class TBindingTag>
    class NvsKeyValueStorage final : public Framework::Provider<
        Domain,
        Framework::Provides<
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
                Framework::PropertyValue<KeyValueFailurePreservation, FailurePreservation::PreservesCommittedState>,
                Framework::PropertyValue<KeyValueInterruptionAtomicity, InterruptionAtomicity::PowerLoss>,
                Framework::PropertyValue<ClearAllFailurePreservation, FailurePreservation::MayModify>,
                Framework::PropertyValue<ClearAllInterruptionAtomicity, InterruptionAtomicity::PowerLoss>
            >
        >
    > {
    private:

        // Bound NVS namespace.

        /// Open ESP-IDF NVS handle.
        nvs_handle_t Handle_;

        /// Bounded scratch space used only when the caller requests a truncated blob read.
        mutable std::uint8_t ReadScratch_[512U];

        /// Indicates whether Handle_ is currently open.
        bool IsReady_;

        /// Copies a validated EDP key into NVS's null-terminated key representation.
        [[nodiscard]] static bool CopyKey(
            KeyView Key,
            char (&Buffer)[16U]
        ) noexcept {
            if (Key.Size() > 15U) {
                return false;
            }

            std::memcpy(
                Buffer,
                Key.Data(),
                Key.Size()
            );
            Buffer[Key.Size()] = '\0';
            return true;
        }

    public:

        // Lifecycle controlled by Bootstrap/application wiring.

        /// Constructs an unopened NVS provider.
        NvsKeyValueStorage() noexcept
            : Handle_(0U),
              IsReady_(false) {}

        /// Opens one caller-selected namespace in an initialized NVS partition.
        [[nodiscard]] bool Open(
            const char* Namespace,
            const char* Partition = NVS_DEFAULT_PART_NAME
        ) noexcept {
            if (IsReady_) {
                return true;
            }

            IsReady_ = nvs_open_from_partition(
                Partition,
                Namespace,
                NVS_READWRITE,
                &Handle_
            ) == ESP_OK;
            return IsReady_;
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

            if (!CopyKey(Key, NativeKey)) {
                return {KeyValueSizeStatus::KeyTooLong, StorageSize{}};
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

            if (!CopyKey(Key, NativeKey)) {
                return {KeyValueReadStatus::KeyTooLong, 0U, 0U, StorageSize{}};
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

                    std::memcpy(
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

            if (!CopyKey(Key, NativeKey)) {
                return KeyValueStoreStatus::KeyTooLong;
            }

            if (Source.Size > 512U) {
                return KeyValueStoreStatus::ValueTooLarge;
            }

            if (nvs_set_blob(Handle_, NativeKey, Source.Address, Source.Size) != ESP_OK) {
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

            if (!CopyKey(Key, NativeKey)) {
                return KeyValueRemoveStatus::KeyTooLong;
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
