// Experimental immutable, generation-named file probe on native ESP-IDF LittleFS.
// No EDP-Persistence FileStorage operation guarantees are upgraded here.
// GPIO17 real VDD relay remains solely owned by the Policy-approved RPI400 host.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <memory/ByteOperationsProvider.hpp>
#include <LittleFsVfsMount.hpp>
#include <LittleFsVfsBinding.hpp>

namespace P = ESPressio::Persistence;
namespace E = P::EspIdf;
struct ImmutableBinding final {};
using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
using Storage = E::LittleFsConservativeFileStorage<ImmutableBinding,Bytes>;

constexpr auto SeedPath=P::FilePathView::Validate("seed.txt");
static_assert(SeedPath.Status==P::FilePathValidationStatus::Succeeded);
static std::array<std::uint8_t,8192U> Source{};
static std::array<std::uint8_t,8192U> Destination{};

[[noreturn]] static void Halt(const char* why) {
    std::printf("EDP_BND|HALT|%s\n",why);
    std::fflush(stdout);
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
}
static void Require(bool ok, const char* reason) {
    if(!ok)Halt(reason);
}
static auto GenerationPath(std::uint32_t generation, char (&name)[16U]) {
    const int n=std::snprintf(name,sizeof(name),"g%04u.bin",generation);
    Require(n>0&&n<static_cast<int>(sizeof(name)),"NAME_UNREPRESENTABLE");
    const auto validated=P::FilePathView::Validate(name,static_cast<std::size_t>(n));
    Require(validated.Status==P::FilePathValidationStatus::Succeeded,
        "INVALID_FILE_PATH");
    return validated.Value;
}
static bool VerifyGeneration(Storage& files, std::uint32_t generation) {
    char filename[16U]{};
    const auto path=GenerationPath(generation,filename);
    const auto result=files.ReadFileAt(
        path,
        P::StorageOffset{},
        P::DestinationBufferView{Destination.data(),Destination.size()}
    );
    if(result.Status!=P::FileReadStatus::Succeeded||
       result.BytesTransferred!=Destination.size()) {
        std::printf("EDP_BND|INVALID|gen=%u|status=%u|size=%u\n",
            generation,static_cast<unsigned>(result.Status),
            static_cast<unsigned>(result.BytesTransferred));
        std::fflush(stdout);
        return false;
    }
    for(const auto value:Destination) {
        if(value!=static_cast<std::uint8_t>(generation)) {
            std::printf("EDP_BND|INVALID|gen=%u|reason=CONTENT\n",generation);
            std::fflush(stdout);
            return false;
        }
    }
    std::printf("EDP_BND|VALID|gen=%u|bytes=8192\n",generation);
    std::fflush(stdout);
    return true;
}
static void WriteFreshGeneration(Storage& files,std::uint32_t generation) {
    char filename[16U]{};
    const auto path=GenerationPath(generation,filename);
    // An existing name must NEVER be truncated, even if its record is invalid.
    const auto previous=files.ReadFileAt(
        path,P::StorageOffset{},P::DestinationBufferView{Destination.data(),1U});
    Require(previous.Status==P::FileReadStatus::NotFound,
        "REFUSE_IN_PLACE_REPLACEMENT");
    Source.fill(static_cast<std::uint8_t>(generation));
    const auto result=files.ReplaceFile(
        path,P::SourceBufferView{Source.data(),Source.size()}
    );
    Require(result==P::FileReplaceStatus::Succeeded,"IMMUTABLE_WRITE_FAILED");
    Require(VerifyGeneration(files,generation),"IMMUTABLE_READBACK_FAILED");
}
extern "C" void app_main() {
    const auto reset=esp_reset_reason();
    std::printf("EDP_BND|BOOT|reset=%u\n",static_cast<unsigned>(reset));
    std::fflush(stdout);
    Require(nvs_flash_init()==ESP_OK,"NVS_INIT");
    nvs_handle_t phase{};
    Require(nvs_open("edpbndp26",NVS_READWRITE,&phase)==ESP_OK,"PHASE_OPEN");
    std::uint8_t armed=0U;
    const auto phaseStatus=nvs_get_u8(phase,"armed",&armed);
    Require(phaseStatus==ESP_OK||
        phaseStatus==ESP_ERR_NVS_NOT_FOUND,"PHASE_READ");

    E::LittleFsVfsMount mount{};
    Require(mount.Mount({"/edplfs","edplfs",false,false})==
        E::LittleFsMountStatus::Succeeded,"MOUNT_FAILED");
    static Bytes byteOperations{};
    Storage storage("/edplfs",byteOperations);
    Require(storage.IsFileStorageReady(),"FILESTORAGE_NOT_READY");
    std::array<std::uint8_t,4U> seed{};
    const auto seedResult=storage.ReadFileAt(
        SeedPath.Value,P::StorageOffset{},
        P::DestinationBufferView{seed.data(),seed.size()});
    Require(seedResult.Status==P::FileReadStatus::Succeeded &&
        seedResult.BytesTransferred==4U &&
        seed[0U]=='E'&&seed[1U]=='D'&&
        seed[2U]=='P'&&seed[3U]=='1',"BAD_PARTITION_SEED");

    if(phaseStatus==ESP_OK&&armed==1U) {
        // Application reset reasons are diagnostics, NOT electrical VDD
        // qualification. Only the independent GPIO17 host can attest a cut.
        std::printf("EDP_BND|RESET_OBSERVED|reason=%u|por=%u\n",
            static_cast<unsigned>(reset),
            reset==ESP_RST_POWERON ? 1U : 0U);
        std::fflush(stdout);
        std::uint32_t penultimate=0U;
        std::uint32_t latest=0U;
        std::size_t present=0U;
        // Test-specific finite upper bound. Deleted older names are expected
        // to be missing, and a torn unacknowledged trailing candidate MAY
        // exist. The newest 2 independently checked files MUST be adjacent.
        for(std::uint32_t g=1U;g<=64U;++g) {
            if(VerifyGeneration(storage,g)) {
                penultimate=latest;
                latest=g;
                ++present;
            }
        }
        Require(latest>=3U && penultimate==latest-1U,
            "PROTECTED_TWO_NOT_CONTIGUOUS");
        Require(present>=2U && present<=4U,
            "RETENTION_COUNT_UNBOUNDED");
        std::printf(
            "EDP_BND|RECOVERED|newest=%u|older=%u|valid=%u|contiguous=1\n",
            latest,penultimate,static_cast<unsigned>(present));
        // This asserts *only* that the protected history replayed; host
        // must separately validate its own controller OFF/ON provenance.
        std::printf("EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n");
        std::fflush(stdout);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000U));
    }

    Require(phaseStatus==ESP_ERR_NVS_NOT_FOUND,"PHASE_NOT_EMPTY");
    WriteFreshGeneration(storage,1U);
    WriteFreshGeneration(storage,2U);
    std::printf("EDP_BND|BASELINE_READY|gen=2|wait_ms=12000\n");
    std::fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(12000U));
    Require(nvs_set_u8(phase,"armed",1U)==ESP_OK &&
        nvs_commit(phase)==ESP_OK,"PHASE_STORE");
    nvs_close(phase);
    std::printf("EDP_BND|ACK|gen=2\n");
    std::fflush(stdout);
    std::printf("EDP_BND|RESOURCE|heap_after_mount=%u|min_free=%u\n",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()));
    std::fflush(stdout);
    for(std::uint32_t g=3U;g<=64U;++g) {
        std::printf("EDP_BND|WRITE_ENTER|gen=%u\n",g);
        std::fflush(stdout);
        WriteFreshGeneration(storage,g);
        std::printf("EDP_BND|ACK|gen=%u|heap=%u|min_heap=%u\n",
            g,static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(esp_get_minimum_free_heap_size()));
        std::fflush(stdout);
        // Reclamation is permitted ONLY after the successor is fully
        // committed/read back. Exactly the previous oldest generation is
        // now logically retired; never delete g or g-1 while retained.
        const auto retired=g-2U;
        char oldName[16U]{};
        const auto oldPath=GenerationPath(retired,oldName);
        std::printf("EDP_BND|RECLAIM_ENTER|retired=%u\n",retired);
        std::fflush(stdout);
        Require(storage.RemoveFile(oldPath)==P::FileRemoveStatus::Succeeded,
            "RECLAIM_FAILED");
        std::printf("EDP_BND|RECLAIMED|retired=%u\n",retired);
        std::fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(60U));
    }
    Halt("NO_POWER_CUT_IN_RECLAMATION_CAMPAIGN");
}
