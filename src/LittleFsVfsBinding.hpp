#pragma once

#include "VfsFileStorage.hpp"

namespace ESPressio::Persistence::EspIdf {

    /// Conservative initial LittleFS VFS binding characteristics, pending
    /// physical qualification of PowerLoss commit/rename/replace behaviour.
    ///
    /// No new FileStorage implementation is created. Existing
    /// VfsFileStorage remains the ONLY owner of all file operations.
    ///
    /// This profile is NOT a declaration of strict archive durability:
    /// FileRetention is initially Restart and interrupted file/rename/
    /// append/write-at operations retain their conservative None claims.
    using LittleFsConservativeBindingProfile = VfsBindingProfile<
        RetentionLevel::Restart,
        TextCaseSensitivity::CaseSensitive,
        MediaRemovability::Fixed,
        96U,
        63U,
        262144U,
        InvocationConcurrency::CallerSerialized
    >;


    /// An application-selectable existing FileStorage binding over LittleFS.
    ///
    /// @tparam TBindingTag Unique application binding marker.
    /// @tparam TByteOperationsProvider Existing injected EDP-Memory ByteOperations.
    template<class TBindingTag, class TByteOperationsProvider>
    using LittleFsConservativeFileStorage = VfsFileStorage<
        TBindingTag,
        LittleFsConservativeBindingProfile,
        TByteOperationsProvider
    >;

} // namespace ESPressio::Persistence::EspIdf
