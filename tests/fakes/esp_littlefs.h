#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
typedef struct {
 const char* base_path;
 const char* partition_label;
 const void* partition;
 unsigned format_if_mount_failed : 1;
 unsigned read_only : 1;
 unsigned dont_mount : 1;
 unsigned grow_on_mount : 1;
} esp_vfs_littlefs_conf_t;
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* conf);
esp_err_t esp_vfs_littlefs_unregister(const char* label);
#ifdef __cplusplus
}
#endif
