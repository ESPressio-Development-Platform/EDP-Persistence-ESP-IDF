// ESP-IDF native LittleFS VFS and existing EDP-Persistence FileStorage proof.
// The RPI400 GPIO17 relay is owned EXCLUSIVELY by the host approved daemon.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <memory/ByteOperationsProvider.hpp>
#include <LittleFsVfsMount.hpp>
#include <LittleFsVfsBinding.hpp>

namespace P=ESPressio::Persistence;
namespace E=P::EspIdf;

struct BindingTag final {};
using Bytes=ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
using Storage=E::LittleFsConservativeFileStorage<BindingTag,Bytes>;
constexpr auto JournalPath=P::FilePathView::Validate("journal.bin");
constexpr auto SeedPath=P::FilePathView::Validate("seed.txt");
static_assert(JournalPath.Status==P::FilePathValidationStatus::Succeeded);
static_assert(SeedPath.Status==P::FilePathValidationStatus::Succeeded);
static std::array<std::uint8_t,8192U> Source{};
static std::array<std::uint8_t,8192U> Destination{};

[[noreturn]] void Halt(const char* why) {
    std::printf("EDP_LFS|HALT|%s\n",why);
    std::fflush(stdout);
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
}
void Require(bool good,const char* why) {
    if(!good)Halt(why);
}
void MakeGeneration(std::uint8_t generation) {
    for(auto& c:Source)c=generation;
}
void VerifyFile(Storage& storage) {
    const auto read=storage.ReadFileAt(
        JournalPath.Value,
        P::StorageOffset{},
        P::DestinationBufferView{Destination.data(),Destination.size()}
    );
    Require(read.Status==P::FileReadStatus::Succeeded &&
        read.BytesTransferred==Destination.size(),"TORN_FILE_LENGTH");
    for(const auto octet:Destination) {
        if(octet!=Destination[0U])Halt("TORN_FILE_CONTENT");
    }
    Require(Destination[0U]>=1U,"INVALID_GENERATION");
    std::printf("EDP_LFS|RECOVERY|gen=%u|bytes=%u|complete=1\n",
        Destination[0U],static_cast<unsigned>(Destination.size()));
    std::fflush(stdout);
}
extern "C" void app_main() {
    const auto reset=esp_reset_reason();
    std::printf("EDP_LFS|BOOT|reset=%u\n",static_cast<unsigned>(reset));
    std::fflush(stdout);
    Require(nvs_flash_init()==ESP_OK,"NVS_INIT");
    nvs_handle_t phase{};
    Require(nvs_open("edplfsphase",NVS_READWRITE,&phase)==ESP_OK,
        "PHASE_OPEN");
    std::uint8_t armed{};
    const auto native=nvs_get_u8(phase,"armed",&armed);
    Require(native==ESP_OK || native==ESP_ERR_NVS_NOT_FOUND,
        "PHASE_INVALID");

    // LittleFsVfsMount is conservative and never autoformats any partition.
    E::LittleFsVfsMount mounted{};
    const auto mountStatus=mounted.Mount({
        "/edplfs",
        "edplfs",
        false,
        false
    });
    Require(mountStatus==E::LittleFsMountStatus::Succeeded,
        "NO_LITTLEFS_MOUNT");
    static Bytes operations{};
    Storage storage("/edplfs",operations);
    Require(storage.IsFileStorageReady(),"VFS_NOT_READY");

    if(native==ESP_OK && armed==1U) {
        Require(reset==ESP_RST_POWERON,"NOT_GENUINE_POWER_ON");
        VerifyFile(storage);
        std::printf("EDP_LFS|PASS|ATOMIC_FILE_RECOVERY\n");
        std::fflush(stdout);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }

    Require(native==ESP_ERR_NVS_NOT_FOUND,"UNEXPECTED_PHASE");
    std::array<std::uint8_t,4U> seed{};
    const auto seedRead=storage.ReadFileAt(
        SeedPath.Value,
        P::StorageOffset{},
        P::DestinationBufferView{seed.data(),seed.size()}
    );
    Require(seedRead.Status==P::FileReadStatus::Succeeded &&
        seedRead.BytesTransferred==seed.size() &&
        seed[0U]=='E' && seed[1U]=='D' &&
        seed[2U]=='P' && seed[3U]=='1',"BAD_EXPLICIT_FS_IMAGE");

    MakeGeneration(1U);
    Require(storage.ReplaceFile(
        JournalPath.Value,
        P::SourceBufferView{Source.data(),Source.size()}
    )==P::FileReplaceStatus::Succeeded,"BASELINE_REPLACE");
    VerifyFile(storage);
    std::printf("EDP_LFS|BASELINE_STABLE|gen=1|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(phase,"armed",1U)==ESP_OK &&
        nvs_commit(phase)==ESP_OK,"PHASE_COMMIT");
    nvs_close(phase);

    std::printf("EDP_LFS|ACK|gen=1\n");
    std::fflush(stdout);
    for(std::uint32_t g=2U;g<=120U;++g) {
        MakeGeneration(static_cast<std::uint8_t>(g));
        std::printf("EDP_LFS|WRITE_ENTER|gen=%u\n",g);
        std::fflush(stdout);
        const auto status=storage.ReplaceFile(
            JournalPath.Value,
            P::SourceBufferView{Source.data(),Source.size()}
        );
        Require(status==P::FileReplaceStatus::Succeeded,"REPLACE_FAILURE");
        std::printf("EDP_LFS|ACK|gen=%u\n",g);
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(2U));
    }
    Halt("NO_HARD_POWER_CUT");
}
