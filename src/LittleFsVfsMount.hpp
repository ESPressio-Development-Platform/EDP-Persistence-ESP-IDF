#pragma once

#include <cstdint>

#include <esp_littlefs.h>

namespace ESPressio::Persistence::EspIdf {

    /// Result of binding an existing LittleFS partition to ESP-IDF VFS.
    enum class LittleFsMountStatus : std::uint8_t {
        Succeeded = 0U,
        AlreadyMounted = 1U,
        NotMounted = 2U,
        InvalidConfiguration = 3U,
        ProviderFailure = 4U
    };


    /// Caller-owned stable names and conservative read/write mount options.
    /// Formatting/recovery is deliberately absent from this configuration.
    struct LittleFsMountSettings final {

        /// Absolute ESP-IDF VFS mount root, e.g. "/edp-journal".
        const char* BasePath{nullptr};

        /// Non-empty dedicated native LittleFS partition label.
        const char* PartitionLabel{nullptr};

        /// Whether the mounted partition is visible for read-only operations.
        bool ReadOnly{false};

        /// Whether to grow an existing filesystem to partition capacity.
        /// Leave disabled unless the application has explicitly approved it.
        bool GrowOnMount{false};
    };


    /// Owns an existing ESP-IDF LittleFS VFS mount, not its bytes or storage.
    ///
    /// This narrow concrete owner reuses the upstream joltwallet/littlefs
    /// component and deliberately does NOT duplicate EDP-Persistence's
    /// existing VfsFileStorage FileStorage implementation. Application
    /// Bootstrap retains stable strings and releases all VfsFileStorage
    /// instances before Unmount. Never format an unknown/failed mount.
    ///
    /// A mount does not itself grant FileInterruptionAtomicity or any
    /// provider-specific PowerLoss claim; qualify the complete FileStorage
    /// binding independently against real flash interruption.
    class LittleFsVfsMount final {
    private:

        // Native binding lifecycle.
        /// Stable caller-owned label retained solely for native unmount.
        const char* PartitionLabel_{nullptr};

        /// Whether this specific owner registered the VFS mount.
        bool Mounted_{false};

    public:

        /// Constructs one initially unmounted native LittleFS owner.
        LittleFsVfsMount() noexcept = default;

        /// Noncopyable because only one owner may release a given mount.
        LittleFsVfsMount(const LittleFsVfsMount&) = delete;

        /// No assignment can duplicate native mount ownership.
        LittleFsVfsMount& operator=(const LittleFsVfsMount&) = delete;

        /// Nonmovable to avoid moving the lifetime of caller-owned strings.
        LittleFsVfsMount(LittleFsVfsMount&&) = delete;

        /// No assignment may transfer native mount ownership implicitly.
        LittleFsVfsMount& operator=(LittleFsVfsMount&&) = delete;

        /// Performs best-effort final unregister if Bootstrap omitted it.
        ~LittleFsVfsMount() noexcept {
            if (Mounted_) {
                (void)esp_vfs_littlefs_unregister(PartitionLabel_);
            }
        }

        /// Registers an existing LittleFS partition without formatting it.
        ///
        /// @returns ProviderFailure if the underlying filesystem cannot
        ///          mount; this MUST NOT trigger format/repair automatically.
        [[nodiscard]] LittleFsMountStatus Mount(
            const LittleFsMountSettings& Settings
        ) noexcept {
            if (Mounted_) {
                return LittleFsMountStatus::AlreadyMounted;
            }
            if (Settings.BasePath == nullptr ||
                Settings.BasePath[0] != '/' ||
                Settings.BasePath[1] == '\0' ||
                Settings.PartitionLabel == nullptr ||
                Settings.PartitionLabel[0] == '\0') {
                return LittleFsMountStatus::InvalidConfiguration;
            }

            esp_vfs_littlefs_conf_t Native{};
            Native.base_path = Settings.BasePath;
            Native.partition_label = Settings.PartitionLabel;
            Native.partition = nullptr;
            Native.format_if_mount_failed = false;
            Native.read_only = Settings.ReadOnly;
            Native.dont_mount = false;
            Native.grow_on_mount = Settings.GrowOnMount;

            if (esp_vfs_littlefs_register(&Native) != ESP_OK) {
                return LittleFsMountStatus::ProviderFailure;
            }

            PartitionLabel_ = Settings.PartitionLabel;
            Mounted_ = true;
            return LittleFsMountStatus::Succeeded;
        }

        /// Indicates whether this owner still considers the VFS mounted.
        [[nodiscard]] bool IsMounted() const noexcept {
            return Mounted_;
        }

        /// Unregisters after every dependent FileStorage operation has ended.
        [[nodiscard]] LittleFsMountStatus Unmount() noexcept {
            if (!Mounted_) {
                return LittleFsMountStatus::NotMounted;
            }
            if (esp_vfs_littlefs_unregister(PartitionLabel_) != ESP_OK) {
                return LittleFsMountStatus::ProviderFailure;
            }
            Mounted_ = false;
            PartitionLabel_ = nullptr;
            return LittleFsMountStatus::Succeeded;
        }
    };

} // namespace ESPressio::Persistence::EspIdf
