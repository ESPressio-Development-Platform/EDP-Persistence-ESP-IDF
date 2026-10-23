#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "LittleFsVfsMount.hpp"
#include "LittleFsVfsBinding.hpp"
#include <memory/ByteOperationsProvider.hpp>

namespace {
    namespace E=ESPressio::Persistence::EspIdf;
    namespace P=ESPressio::Persistence;

    struct State final {
        int RegisterCalls{0};
        int UnregisterCalls{0};
        esp_err_t NextRegister{ESP_OK};
        esp_err_t NextUnregister{ESP_OK};
        bool AutoFormat{true};
        bool ReadOnly{true};
        bool MountSkipped{true};
        bool GrowOnMount{true};
        const char* MountName{nullptr};
        const char* PartitionName{nullptr};
    };

    State Simulator{};
}

extern "C" esp_err_t esp_vfs_littlefs_register(
    const esp_vfs_littlefs_conf_t* config
) {
    ++Simulator.RegisterCalls;
    Simulator.AutoFormat=config->format_if_mount_failed;
    Simulator.ReadOnly=config->read_only;
    Simulator.MountSkipped=config->dont_mount;
    Simulator.GrowOnMount=config->grow_on_mount;
    Simulator.MountName=config->base_path;
    Simulator.PartitionName=config->partition_label;
    return Simulator.NextRegister;
}

extern "C" esp_err_t esp_vfs_littlefs_unregister(
    const char*
) {
    ++Simulator.UnregisterCalls;
    return Simulator.NextUnregister;
}

struct BindingTag final {};
using Bytes=ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
using Concrete=E::LittleFsConservativeFileStorage<BindingTag,Bytes>;
using Offered=typename Concrete::CompositionOffers::template PropertiesFor<P::FileStorage>;

static_assert(
    Offered::template Value<P::FileRetention> ==
        P::RetentionLevel::Restart
);
static_assert(
    Offered::template Value<P::FileInterruptionAtomicity> ==
        P::InterruptionAtomicity::None
);
static_assert(
    Offered::template Value<P::RenameInterruptionAtomicity> ==
        P::InterruptionAtomicity::None
);
static_assert(
    Offered::template Value<P::FileFailurePreservation> ==
        P::FailurePreservation::MayModify
);

int main() {
    using Status=E::LittleFsMountStatus;
    static_assert(!std::is_copy_constructible_v<E::LittleFsVfsMount>);
    static_assert(!std::is_move_constructible_v<E::LittleFsVfsMount>);
    E::LittleFsVfsMount mount{};
    assert(!mount.IsMounted());
    assert(mount.Unmount()==Status::NotMounted);
    assert(mount.Mount({nullptr,"log"})==Status::InvalidConfiguration);
    assert(mount.Mount({"missing-slash","log"})==Status::InvalidConfiguration);
    assert(mount.Mount({"/log",nullptr})==Status::InvalidConfiguration);
    assert(mount.Mount({"/log",""})==Status::InvalidConfiguration);
    assert(Simulator.RegisterCalls==0);
    Simulator.NextRegister=ESP_FAIL;
    assert(mount.Mount({"/logs","persist"})==Status::ProviderFailure);
    assert(!mount.IsMounted());
    assert(Simulator.RegisterCalls==1);
    assert(Simulator.AutoFormat==false);
    assert(Simulator.MountSkipped==false);
    assert(Simulator.ReadOnly==false);
    assert(Simulator.GrowOnMount==false);
    Simulator.NextRegister=ESP_OK;
    assert(mount.Mount({"/logs","persist",true,false})==Status::Succeeded);
    assert(mount.IsMounted());
    assert(Simulator.ReadOnly==true);
    assert(std::strcmp(Simulator.MountName,"/logs")==0);
    assert(std::strcmp(Simulator.PartitionName,"persist")==0);
    assert(mount.Mount({"/another","persist"})==Status::AlreadyMounted);
    Simulator.NextUnregister=ESP_FAIL;
    assert(mount.Unmount()==Status::ProviderFailure);
    assert(mount.IsMounted());
    Simulator.NextUnregister=ESP_OK;
    assert(mount.Unmount()==Status::Succeeded);
    assert(!mount.IsMounted());
    assert(mount.Unmount()==Status::NotMounted);
    {
        E::LittleFsVfsMount autoCleanup{};
        assert(autoCleanup.Mount({"/logs","persist"})==Status::Succeeded);
    }
    assert(Simulator.UnregisterCalls==3);
}
