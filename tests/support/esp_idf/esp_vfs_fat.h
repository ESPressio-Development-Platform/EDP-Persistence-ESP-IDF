#pragma once

#include <cstddef>

#include <esp_err.h>
#include <wear_levelling.h>


struct esp_vfs_fat_mount_config_t final {
    bool format_if_mount_failed;
    int max_files;
    std::size_t allocation_unit_size;
};


inline esp_err_t esp_vfs_fat_spiflash_mount_rw_wl(
    const char* BasePath,
    const char* PartitionLabel,
    const esp_vfs_fat_mount_config_t* Configuration,
    wl_handle_t* Handle
) noexcept {
    static_cast<void>(BasePath);
    static_cast<void>(PartitionLabel);
    static_cast<void>(Configuration);

    if (Handle != nullptr) {
        *Handle = 1;
    }

    return ESP_OK;
}


inline esp_err_t esp_vfs_fat_spiflash_unmount_rw_wl(
    const char* BasePath,
    wl_handle_t Handle
) noexcept {
    static_cast<void>(BasePath);
    static_cast<void>(Handle);
    return ESP_OK;
}
