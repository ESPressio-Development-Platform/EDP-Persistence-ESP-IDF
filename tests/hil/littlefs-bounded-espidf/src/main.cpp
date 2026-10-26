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

#if defined(EDP_TEST_LOWSPACE)
#include <esp_littlefs.h>
#endif
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

#if defined(EDP_TEST_LOWSPACE)
    // This test-only workload uses explicit finite bounds. Filler files are
    // never log records, never acknowledged and never part of the retention
    // window. It does not alter any production FileStorage capability.
    static constexpr std::uint32_t MaximumPaddingFiles = 96U;
    static constexpr std::size_t PartitionBytes = 0x90000U;

    /// Distinguishes a collision from an uncommitted successor write failure.
    enum class CandidateStatus : std::uint8_t {
        Committed = 0U,
        ExistingName = 1U,
        StorageFailure = 2U,
        InvalidReadback = 3U
    };

    /// Tests immutable-file admission without ever opening an existing name.
    [[nodiscard]] static CandidateStatus TryCreateCandidate(
        Storage& files,
        std::uint32_t generation
    ) {
        char filename[16U]{};
        const auto path = GenerationPath(generation, filename);
        const auto previous = files.ReadFileAt(
            path,
            P::StorageOffset{},
            P::DestinationBufferView{Destination.data(), 1U}
        );
        if (previous.Status != P::FileReadStatus::NotFound) {
            return CandidateStatus::ExistingName;
        }

        Source.fill(static_cast<std::uint8_t>(generation));
        if (files.ReplaceFile(
            path,
            P::SourceBufferView{Source.data(), Source.size()}
        ) != P::FileReplaceStatus::Succeeded) {
            return CandidateStatus::StorageFailure;
        }

        return VerifyGeneration(files, generation)
            ? CandidateStatus::Committed
            : CandidateStatus::InvalidReadback;
    }

    /// Reports the actual test partition usage without advertising it as an
    /// EDP-Persistence capacity-capability or a future write reservation.
    static void PrintPhysicalUsage(const char* label) {
        std::size_t total = 0U;
        std::size_t used = 0U;
        Require(
            esp_littlefs_info("edplfs", &total, &used) == ESP_OK,
            "LOWSPACE_INFO_UNAVAILABLE"
        );
        Require(total == PartitionBytes && used <= total,
            "LOWSPACE_INFO_INCONSISTENT");
        std::printf(
            "EDP_BND|LOWSPACE_USAGE|stage=%s|total=%u|used=%u|free=%u\n",
            label,
            static_cast<unsigned>(total),
            static_cast<unsigned>(used),
            static_cast<unsigned>(total - used)
        );
        std::fflush(stdout);
    }

    /// Exposes the physical exhaustion boundary while retaining g1 and g2.
    /// No new record is acknowledged until both a complete write AND an
    /// independent exact replay have succeeded. No retained name is reused.

    /// On a later boot, first verify g1/g2 (done at the caller), then reclaim
    /// only unprotected test filler and the unacknowledged torn candidate.
    /// Exactly one new immutable successor is acknowledged before g1 retires.
    static void RecoverFreeSpaceAndRoll(Storage& files) {
        for (std::uint32_t index = 0U;
             index < MaximumPaddingFiles;
             ++index) {
            char filename[16U]{};
            const auto length = std::snprintf(
                filename,
                sizeof(filename),
                "pad%03u.bin",
                static_cast<unsigned>(index)
            );
            Require(length > 0 &&
                length < static_cast<int>(sizeof(filename)),
                "LOWSPACE_RECLAIM_NAME");
            const auto path = P::FilePathView::Validate(
                filename,
                static_cast<std::size_t>(length)
            );
            Require(path.Status == P::FilePathValidationStatus::Succeeded,
                "LOWSPACE_RECLAIM_PATH");
            const auto result = files.RemoveFile(path.Value);
            Require(result == P::FileRemoveStatus::Succeeded ||
                result == P::FileRemoveStatus::NotFound,
                "LOWSPACE_RECLAIM_FILLER_FAILED");
            if (index % 16U == 15U) {
                vTaskDelay(pdMS_TO_TICKS(1U));
            }
        }

        PrintPhysicalUsage("after_filler_reclaim");
        Require(VerifyGeneration(files, 1U) && VerifyGeneration(files, 2U),
            "LOWSPACE_RECLAIM_CHANGED_PROTECTED");

        char uncommittedName[16U]{};
        const auto uncommittedPath = GenerationPath(3U, uncommittedName);
        // No ACK(3) was ever emitted. A failed Create may have left a
        // complete, truncated or absent disposable candidate. Never
        // truncate any still-retained filename; only g3 is unprotected.
        const auto candidateRemove = files.RemoveFile(uncommittedPath);
        Require(candidateRemove == P::FileRemoveStatus::Succeeded ||
            candidateRemove == P::FileRemoveStatus::NotFound,
            "LOWSPACE_FAILED_CANDIDATE_CANNOT_BE_RETIRED");
        Require(VerifyGeneration(files, 1U) && VerifyGeneration(files, 2U),
            "LOWSPACE_CANDIDATE_CLEANUP_DAMAGED_PROTECTED");

        const auto successor = TryCreateCandidate(files, 3U);
        Require(successor == CandidateStatus::Committed,
            "LOWSPACE_SUCCESSOR_FAILED_AFTER_RECLAIM");
        std::printf("EDP_BND|ACK|gen=3\n");
        std::fflush(stdout);

        char retiredName[16U]{};
        const auto retiredPath = GenerationPath(1U, retiredName);
        Require(files.RemoveFile(retiredPath) ==
            P::FileRemoveStatus::Succeeded,
            "LOWSPACE_RETIRED_OLDEST_REMOVE_FAILED");

        const auto gone = files.ReadFileAt(
            retiredPath,
            P::StorageOffset{},
            P::DestinationBufferView{Destination.data(), 1U}
        );
        Require(gone.Status == P::FileReadStatus::NotFound,
            "LOWSPACE_RETIRED_OLDEST_STILL_PRESENT");
        Require(VerifyGeneration(files, 2U) && VerifyGeneration(files, 3U),
            "LOWSPACE_ROLLED_WINDOW_INVALID");
        PrintPhysicalUsage("rolled");
        std::printf(
            "EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n"
        );
        std::fflush(stdout);
    }

    static void RunLowSpaceExhaustion(Storage& files) {
        Require(
            TryCreateCandidate(files, 2U) == CandidateStatus::ExistingName,
            "LOWSPACE_COLLISION_NOT_REJECTED"
        );
        Require(VerifyGeneration(files, 1U) && VerifyGeneration(files, 2U),
            "LOWSPACE_COLLISION_CHANGED_PROTECTED");
        std::printf("EDP_BND|LOWSPACE_COLLISION_REFUSED|gen=2\n");
        PrintPhysicalUsage("initial");

        bool sawPhysicalFailure = false;
        std::uint32_t padsCommitted = 0U;
        for (std::uint32_t index = 0U;
             index < MaximumPaddingFiles;
             ++index) {
            char filename[16U]{};
            const auto length = std::snprintf(
                filename,
                sizeof(filename),
                "pad%03u.bin",
                static_cast<unsigned>(index)
            );
            Require(length > 0 &&
                length < static_cast<int>(sizeof(filename)),
                "LOWSPACE_PAD_NAME");
            const auto path = P::FilePathView::Validate(
                filename,
                static_cast<std::size_t>(length)
            );
            Require(path.Status == P::FilePathValidationStatus::Succeeded,
                "LOWSPACE_PAD_PATH");

            // Unique disposable physical pressure only. Never a protected
            // or acknowledged EDLS/EDLG record.
            Source.fill(0xA5U);
            const auto result = files.ReplaceFile(
                path.Value,
                P::SourceBufferView{Source.data(), Source.size()}
            );
            if (result != P::FileReplaceStatus::Succeeded) {
                sawPhysicalFailure = true;
                std::printf(
                    "EDP_BND|LOWSPACE_FILL_REFUSED|pad=%u|status=%u\n",
                    static_cast<unsigned>(index),
                    static_cast<unsigned>(result)
                );
                std::fflush(stdout);
                break;
            }

            ++padsCommitted;
            if (padsCommitted % 8U == 0U) {
                PrintPhysicalUsage("filling");
            }
        }

        Require(sawPhysicalFailure, "LOWSPACE_NO_EXHAUSTION_WITHIN_BOUND");
        PrintPhysicalUsage("exhausted");
        Require(VerifyGeneration(files, 1U) && VerifyGeneration(files, 2U),
            "LOWSPACE_FILL_CHANGED_PROTECTED");

        std::printf("EDP_BND|LOWSPACE_CANDIDATE_ENTER|gen=3\n");
        std::fflush(stdout);
        const auto candidate = TryCreateCandidate(files, 3U);
        // A full/complete candidate could legitimately be committed if the
        // filesystem still admits it; that is not an exhaustion-failure test.
        // Do not falsely report a fail-closed proof in such a case.
        Require(candidate == CandidateStatus::StorageFailure,
            "LOWSPACE_CANDIDATE_NOT_FAILED_AS_EXPECTED");
        Require(VerifyGeneration(files, 1U) && VerifyGeneration(files, 2U),
            "LOWSPACE_FAILED_WRITE_CHANGED_PROTECTED");
        PrintPhysicalUsage("failed_candidate");
        std::printf(
            "EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=%u|no_ack_3=1\n",
            static_cast<unsigned>(padsCommitted)
        );
        std::fflush(stdout);
#if defined(EDP_TEST_LOWSPACE_RECLAIM)
        // Separate test-only scenario: the host intentionally interrupts
        // the finite low-space cleanup itself. Preserve g1/g2 until a
        // separately verified successor becomes committed. This does not
        // attest which internal LittleFS erase/GC instruction was cut.
        for (std::uint32_t index = 0U;
             index < MaximumPaddingFiles;
             ++index) {
            char filename[16U]{};
            const auto nameLength = std::snprintf(
                filename,
                sizeof(filename),
                "pad%03u.bin",
                static_cast<unsigned>(index)
            );
            Require(nameLength > 0 &&
                nameLength < static_cast<int>(sizeof(filename)),
                "LOWSPACE_RECLAIM_CUT_NAME");
            const auto path = P::FilePathView::Validate(
                filename,
                static_cast<std::size_t>(nameLength)
            );
            Require(path.Status == P::FilePathValidationStatus::Succeeded,
                "LOWSPACE_RECLAIM_CUT_PATH");
            std::printf(
                "EDP_BND|LOWSPACE_RECLAIM_ENTER|pad=%u\n",
                static_cast<unsigned>(index)
            );
            std::fflush(stdout);
            const auto result = files.RemoveFile(path.Value);
            Require(result == P::FileRemoveStatus::Succeeded ||
                result == P::FileRemoveStatus::NotFound,
                "LOWSPACE_RECLAIM_CUT_FAILURE");
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        Halt("LOWSPACE_RECLAIM_FINISHED_WITHOUT_POWER_CUT");
#else
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
#endif
    }
#endif

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
#if defined(EDP_TEST_LOWSPACE)
        // An unacknowledged failed new file can be partial OR complete;
        // neither changes the independently acknowledged g1/g2 window.
        Require(VerifyGeneration(storage, 1U) &&
            VerifyGeneration(storage, 2U),
            "LOWSPACE_PROTECTED_WINDOW_LOST");
        const bool candidateValid = VerifyGeneration(storage, 3U);
        std::printf(
            "EDP_BND|LOWSPACE_REPLAY|acknowledged=2|candidate_valid=%u\n",
            candidateValid ? 1U : 0U
        );
        std::printf(
            "EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n"
        );
        std::fflush(stdout);
        RecoverFreeSpaceAndRoll(storage);
#else
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
#endif
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
#if defined(EDP_TEST_LOWSPACE)
    RunLowSpaceExhaustion(storage);
#else
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
#endif
}
