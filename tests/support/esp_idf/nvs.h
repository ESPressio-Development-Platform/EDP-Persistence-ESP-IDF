#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <esp_err.h>

using nvs_handle_t = std::uint32_t;

inline constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
inline constexpr esp_err_t ESP_ERR_NVS_INVALID_HANDLE = 0x1107;
inline constexpr esp_err_t ESP_ERR_NVS_NOT_ENOUGH_SPACE = 0x1105;

inline constexpr int NVS_READONLY = 0;
inline constexpr int NVS_READWRITE = 1;
inline constexpr const char* NVS_DEFAULT_PART_NAME = "nvs";


namespace FakeNvs {

    struct Namespace final {
        std::map<std::string, std::vector<std::uint8_t>> Values;
    };

    inline std::map<std::string, Namespace> Namespaces;
    inline std::map<nvs_handle_t, std::string> Handles;
    inline nvs_handle_t NextHandle = 1U;

} // namespace FakeNvs


inline esp_err_t nvs_open_from_partition(
    const char* Partition,
    const char* Namespace,
    int OpenMode,
    nvs_handle_t* Handle
) {
    static_cast<void>(Partition);
    static_cast<void>(OpenMode);

    if (Namespace == nullptr || Handle == nullptr) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    const auto NativeHandle = FakeNvs::NextHandle++;
    FakeNvs::Handles[NativeHandle] = Namespace;
    FakeNvs::Namespaces.try_emplace(Namespace);
    *Handle = NativeHandle;
    return ESP_OK;
}


inline void nvs_close(nvs_handle_t Handle) {
    FakeNvs::Handles.erase(Handle);
}


inline esp_err_t nvs_get_blob(
    nvs_handle_t Handle,
    const char* Key,
    void* Destination,
    std::size_t* Length
) {
    const auto HandleIterator = FakeNvs::Handles.find(Handle);

    if (HandleIterator == FakeNvs::Handles.end() ||
        Key == nullptr ||
        Length == nullptr) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    auto& Values =
        FakeNvs::Namespaces[HandleIterator->second].Values;
    const auto Iterator = Values.find(std::string(Key));

    if (Iterator == Values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    const auto Required = Iterator->second.size();

    if (Destination == nullptr) {
        *Length = Required;
        return ESP_OK;
    }

    if (*Length < Required) {
        *Length = Required;
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }

    if (Required != 0U) {
        std::memcpy(
            Destination,
            Iterator->second.data(),
            Required
        );
    }

    *Length = Required;
    return ESP_OK;
}


inline esp_err_t nvs_set_blob(
    nvs_handle_t Handle,
    const char* Key,
    const void* Source,
    std::size_t Length
) {
    const auto HandleIterator = FakeNvs::Handles.find(Handle);

    if (HandleIterator == FakeNvs::Handles.end() ||
        Key == nullptr ||
        (Source == nullptr && Length != 0U)) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    auto& Value =
        FakeNvs::Namespaces[HandleIterator->second].Values[std::string(Key)];
    Value.clear();

    if (Length != 0U) {
        const auto* Bytes =
            static_cast<const std::uint8_t*>(Source);
        Value.assign(
            Bytes,
            Bytes + Length
        );
    }

    return ESP_OK;
}


inline esp_err_t nvs_commit(nvs_handle_t Handle) {
    return FakeNvs::Handles.contains(Handle)
        ? ESP_OK
        : ESP_ERR_NVS_INVALID_HANDLE;
}


inline esp_err_t nvs_erase_key(
    nvs_handle_t Handle,
    const char* Key
) {
    const auto HandleIterator = FakeNvs::Handles.find(Handle);

    if (HandleIterator == FakeNvs::Handles.end() || Key == nullptr) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    auto& Values =
        FakeNvs::Namespaces[HandleIterator->second].Values;

    return Values.erase(std::string(Key)) == 1U
        ? ESP_OK
        : ESP_ERR_NVS_NOT_FOUND;
}


inline esp_err_t nvs_erase_all(nvs_handle_t Handle) {
    const auto HandleIterator = FakeNvs::Handles.find(Handle);

    if (HandleIterator == FakeNvs::Handles.end()) {
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    FakeNvs::Namespaces[HandleIterator->second].Values.clear();
    return ESP_OK;
}
