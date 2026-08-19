#include "LscBypassRuntime.hpp"

#include "../../core/logging/Logger.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <cstdint>
#include <vector>

namespace Tutones::Game::Mods
{
    namespace
    {
        constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                hash += static_cast<std::uint8_t>(c);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        constexpr std::uint32_t CarmodShopHash = Joaat("carmod_shop");
    }

    LscBypassRuntime& LscBypassRuntime::Get() noexcept
    {
        static LscBypassRuntime instance;
        return instance;
    }

    bool LscBypassRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        auto& patches = Script::ScriptPatchRuntime::Get();
        if (!patches.Start())
        {
            m_Running.store(false, std::memory_order_release);
            TUTONES_LOG_ERROR("vehicle.lsc", "Script patch runtime failed to start");
            PublishSnapshot();
            return false;
        }

        m_CanUseVehiclePatch = patches.AddPatch(
            CarmodShopHash,
            Script::ScriptPointer(
                "CanUseVehiclePatch",
                "2D ? ? ? ? 38 ? 5D ? ? ? 56 ? ? 71 2E ? ? 5D").Add(5),
            std::vector<std::uint8_t>{0x72, 0x2E, 0x03, 0x01});
        m_BlockMenuOptionPatch = patches.AddPatch(
            CarmodShopHash,
            Script::ScriptPointer(
                "BlockMenuOptionPatch",
                "2D ? ? ? ? 38 ? 5D ? ? ? 5D ? ? ? 56").Add(5),
            std::vector<std::uint8_t>{0x71, 0x2E, 0x01, 0x01});

        if (m_CanUseVehiclePatch == 0 || m_BlockMenuOptionPatch == 0)
        {
            if (m_CanUseVehiclePatch != 0)
                patches.RemovePatch(m_CanUseVehiclePatch);
            if (m_BlockMenuOptionPatch != 0)
                patches.RemovePatch(m_BlockMenuOptionPatch);
            m_CanUseVehiclePatch = 0;
            m_BlockMenuOptionPatch = 0;
            patches.Stop();
            m_Running.store(false, std::memory_order_release);
            TUTONES_LOG_ERROR("vehicle.lsc", "Could not register the two carmod_shop restriction patches");
            PublishSnapshot();
            return false;
        }

        if (!QueueNextTick())
        {
            patches.RemovePatch(m_CanUseVehiclePatch);
            patches.RemovePatch(m_BlockMenuOptionPatch);
            m_CanUseVehiclePatch = 0;
            m_BlockMenuOptionPatch = 0;
            patches.Stop();
            m_Running.store(false, std::memory_order_release);
            TUTONES_LOG_ERROR("vehicle.lsc", "LSC bypass runtime failed to queue its first GTA script-thread tick");
            PublishSnapshot();
            return false;
        }

        TUTONES_LOG_INFO("vehicle.lsc", "Registered current Enhanced carmod_shop restriction patches");
        return true;
    }

    void LscBypassRuntime::Stop() noexcept
    {
        const bool wasRunning = m_Running.exchange(false, std::memory_order_acq_rel);
        auto& patches = Script::ScriptPatchRuntime::Get();

        if (m_CanUseVehiclePatch != 0)
        {
            static_cast<void>(patches.SetPatchEnabled(m_CanUseVehiclePatch, false));
            patches.RemovePatch(m_CanUseVehiclePatch);
        }
        if (m_BlockMenuOptionPatch != 0)
        {
            static_cast<void>(patches.SetPatchEnabled(m_BlockMenuOptionPatch, false));
            patches.RemovePatch(m_BlockMenuOptionPatch);
        }
        m_CanUseVehiclePatch = 0;
        m_BlockMenuOptionPatch = 0;
        patches.Stop();
        PublishSnapshot();

        if (wasRunning)
            TUTONES_LOG_INFO("vehicle.lsc", "LSC restriction bypass runtime stopped");
    }

    void LscBypassRuntime::SetEnabled(bool enabled) noexcept
    {
        m_Enabled.store(enabled, std::memory_order_release);
    }

    bool LscBypassRuntime::Enabled() const noexcept
    {
        return m_Enabled.load(std::memory_order_acquire);
    }

    bool LscBypassRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    LscBypassSnapshot LscBypassRuntime::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    bool LscBypassRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;
        return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
    }

    void LscBypassRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        auto& patches = Script::ScriptPatchRuntime::Get();
        const auto canUse = patches.Status(m_CanUseVehiclePatch);
        const auto blockOption = patches.Status(m_BlockMenuOptionPatch);
        const bool supported = patches.HookActive() && canUse.supported && blockOption.supported;
        const bool shouldEnable = Enabled() && supported;

        static_cast<void>(patches.SetPatchEnabled(m_CanUseVehiclePatch, shouldEnable));
        static_cast<void>(patches.SetPatchEnabled(m_BlockMenuOptionPatch, shouldEnable));
        PublishSnapshot();

        if (IsRunning() && !QueueNextTick())
        {
            m_Running.store(false, std::memory_order_release);
            static_cast<void>(patches.SetPatchEnabled(m_CanUseVehiclePatch, false));
            static_cast<void>(patches.SetPatchEnabled(m_BlockMenuOptionPatch, false));
            patches.RemovePatch(m_CanUseVehiclePatch);
            patches.RemovePatch(m_BlockMenuOptionPatch);
            m_CanUseVehiclePatch = 0;
            m_BlockMenuOptionPatch = 0;
            patches.Stop();
            PublishSnapshot();
            TUTONES_LOG_ERROR("vehicle.lsc", "LSC bypass runtime lost its GTA script-thread scheduling slot and stopped");
        }
    }

    void LscBypassRuntime::PublishSnapshot() noexcept
    {
        auto& patches = Script::ScriptPatchRuntime::Get();
        const auto canUse = patches.Status(m_CanUseVehiclePatch);
        const auto blockOption = patches.Status(m_BlockMenuOptionPatch);

        LscBypassSnapshot next{};
        next.running = IsRunning();
        next.enabled = Enabled();
        next.hookActive = patches.HookActive();
        next.programLoaded = Script::ScriptRuntime::Get().FindProgram(CarmodShopHash) != nullptr;
        next.canUseVehicleSupported = canUse.supported;
        next.blockMenuOptionSupported = blockOption.supported;
        next.applied = canUse.active && blockOption.active;

        std::scoped_lock lock(m_Mutex);
        m_Snapshot = next;
    }
}
