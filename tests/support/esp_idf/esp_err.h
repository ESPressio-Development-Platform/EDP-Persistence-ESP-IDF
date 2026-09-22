#pragma once

using esp_err_t = int;

inline constexpr esp_err_t ESP_OK = 0;
inline constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 0x110d;
inline constexpr esp_err_t ESP_ERR_NVS_NEW_VERSION_FOUND = 0x1110;
