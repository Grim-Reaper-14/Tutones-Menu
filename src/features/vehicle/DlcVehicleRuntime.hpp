#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/script/ScriptPatchRuntime.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Tutones::Game::VehicleFeatures
{
    struct DlcVehicleSnapshot final
    {
        bool running{};
        bool enabled{};
        bool hookActive{};
        bool programLoaded{};
        bool vehicleAvailabilitySupported{};
        bool priceGateSupported{};
        bool purchaseGateSupported{};
        bool applied{};
    };

    // Enhanced appinternet script patches derived from the current decompiled
    // vehicle website flow. This does not patch the game executable; the shared
    // ScriptPatchRuntime shadows only the matching script bytecode while the
    // appinternet program executes and restores the original program on disable.
    class DlcVehicleRuntime final
    {
    public:
        static DlcVehicleRuntime& Get() noexcept
        {
            static DlcVehicleRuntime instance;
            return instance;
        }

        bool Start()
        {
            bool expected = false;
            if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            auto& patches = Script::ScriptPatchRuntime::Get();
            if (!patches.Start())
            {
                m_Running.store(false, std::memory_order_release);
                TUTONES_LOG_ERROR("vehicle.dlc", "Script patch runtime failed to start");
                PublishSnapshot();
                return false;
            }

            // Current Enhanced appinternet paths, mirrored from the decompiled
            // vehicle availability/price/purchase checks used by YimMenuV2.
            m_VehicleAvailabilityPatch = patches.AddPatch(
                AppInternetHash,
                Script::ScriptPointer("VehiclePOSIXPatch", "59 ? ? 72 2E 02 01"),
                std::vector<std::uint8_t>{0x2B, 0x00, 0x00});
            m_PriceGatePatch = patches.AddPatch(
                AppInternetHash,
                Script::ScriptPointer("GetVehiclePricePatch", "56 ? ? 70 2E 04 01 38 01"),
                std::vector<std::uint8_t>{0x55});
            m_PurchaseGatePatch = patches.AddPatch(
                AppInternetHash,
                Script::ScriptPointer("BuyVehiclePatch", "5D ? ? ? 06 56 ? ? 38 00 25 ? 50").Add(5),
                std::vector<std::uint8_t>{0x55});

            if (m_VehicleAvailabilityPatch == 0 || m_PriceGatePatch == 0 || m_PurchaseGatePatch == 0)
            {
                CleanupPatches();
                patches.Stop();
                m_Running.store(false, std::memory_order_release);
                TUTONES_LOG_ERROR("vehicle.dlc", "Could not register all appinternet DLC vehicle patches");
                PublishSnapshot();
                return false;
            }

            if (!QueueNextTick())
            {
                CleanupPatches();
                patches.Stop();
                m_Running.store(false, std::memory_order_release);
                TUTONES_LOG_ERROR("vehicle.dlc", "DLC vehicle runtime failed to queue its first GTA script-thread tick");
                PublishSnapshot();
                return false;
            }

            TUTONES_LOG_INFO("vehicle.dlc", "Registered Enhanced appinternet vehicle availability patches");
            return true;
        }

        void Stop() noexcept
        {
            const bool wasRunning = m_Running.exchange(false, std::memory_order_acq_rel);
            auto& patches = Script::ScriptPatchRuntime::Get();
            CleanupPatches();
            patches.Stop();
            PublishSnapshot();
            if (wasRunning)
                TUTONES_LOG_INFO("vehicle.dlc", "DLC vehicle website runtime stopped");
        }

        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled.store(enabled, std::memory_order_release);
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_Running.load(std::memory_order_acquire);
        }

        [[nodiscard]] DlcVehicleSnapshot Snapshot() const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            return m_Snapshot;
        }

    private:
        DlcVehicleRuntime() = default;

        [[nodiscard]] static constexpr std::uint32_t Joaat(const char* text) noexcept
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

        static constexpr std::uint32_t AppInternetHash = Joaat("appinternet");

        bool QueueNextTick()
        {
            if (!IsRunning())
                return false;
            return Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); });
        }

        void TickOnGameThread() noexcept
        {
            if (!IsRunning())
                return;

            auto& patches = Script::ScriptPatchRuntime::Get();
            const auto availability = patches.Status(m_VehicleAvailabilityPatch);
            const auto price = patches.Status(m_PriceGatePatch);
            const auto purchase = patches.Status(m_PurchaseGatePatch);
            const bool supported = patches.HookActive()
                && availability.supported
                && price.supported
                && purchase.supported;
            const bool shouldEnable = Enabled() && supported;

            static_cast<void>(patches.SetPatchEnabled(m_VehicleAvailabilityPatch, shouldEnable));
            static_cast<void>(patches.SetPatchEnabled(m_PriceGatePatch, shouldEnable));
            static_cast<void>(patches.SetPatchEnabled(m_PurchaseGatePatch, shouldEnable));
            PublishSnapshot();

            if (IsRunning() && !QueueNextTick())
            {
                m_Running.store(false, std::memory_order_release);
                CleanupPatches();
                patches.Stop();
                PublishSnapshot();
                TUTONES_LOG_ERROR("vehicle.dlc", "DLC vehicle runtime lost its GTA script-thread scheduling slot and stopped");
            }
        }

        void CleanupPatches() noexcept
        {
            auto& patches = Script::ScriptPatchRuntime::Get();
            if (m_VehicleAvailabilityPatch != 0)
            {
                static_cast<void>(patches.SetPatchEnabled(m_VehicleAvailabilityPatch, false));
                patches.RemovePatch(m_VehicleAvailabilityPatch);
            }
            if (m_PriceGatePatch != 0)
            {
                static_cast<void>(patches.SetPatchEnabled(m_PriceGatePatch, false));
                patches.RemovePatch(m_PriceGatePatch);
            }
            if (m_PurchaseGatePatch != 0)
            {
                static_cast<void>(patches.SetPatchEnabled(m_PurchaseGatePatch, false));
                patches.RemovePatch(m_PurchaseGatePatch);
            }
            m_VehicleAvailabilityPatch = 0;
            m_PriceGatePatch = 0;
            m_PurchaseGatePatch = 0;
        }

        void PublishSnapshot() noexcept
        {
            auto& patches = Script::ScriptPatchRuntime::Get();
            const auto availability = patches.Status(m_VehicleAvailabilityPatch);
            const auto price = patches.Status(m_PriceGatePatch);
            const auto purchase = patches.Status(m_PurchaseGatePatch);

            DlcVehicleSnapshot next{};
            next.running = IsRunning();
            next.enabled = Enabled();
            next.hookActive = patches.HookActive();
            next.programLoaded = Script::ScriptRuntime::Get().FindProgram(AppInternetHash) != nullptr;
            next.vehicleAvailabilitySupported = availability.supported;
            next.priceGateSupported = price.supported;
            next.purchaseGateSupported = purchase.supported;
            next.applied = availability.active && price.active && purchase.active;

            std::scoped_lock lock(m_Mutex);
            m_Snapshot = next;
        }

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Enabled{false};
        Script::ScriptPatchHandle m_VehicleAvailabilityPatch{};
        Script::ScriptPatchHandle m_PriceGatePatch{};
        Script::ScriptPatchHandle m_PurchaseGatePatch{};
        mutable std::mutex m_Mutex;
        DlcVehicleSnapshot m_Snapshot{};
    };
}
