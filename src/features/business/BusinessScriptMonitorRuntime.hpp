#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Business
{
    namespace BusinessScriptMonitorDetail
    {
        [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
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
    }

    struct BusinessScriptMonitorSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};

        bool businessHubRunning{};
        bool bunkerRunning{};
        bool acidLabRunning{};
        bool autoShopRunning{};
        bool bailOfficeRunning{};
        bool casinoRunning{};
        bool carWashRunning{};
        bool luxuryShowroomRunning{};

        bool hangarRunning{};
        bool vehicleCargoRunning{};
        std::string message{"Ready"};
    };

    class BusinessScriptMonitorRuntime final
    {
    public:
        static constexpr std::uint32_t BusinessHubScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_business_hub");
        static constexpr std::uint32_t BunkerScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_bunker");
        static constexpr std::uint32_t AcidLabScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_acid_lab");
        static constexpr std::uint32_t AutoShopScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_auto_shop");
        static constexpr std::uint32_t BailOfficeScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_bail_office");
        static constexpr std::uint32_t CasinoScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_casino");
        static constexpr std::uint32_t CarWashScriptHash = BusinessScriptMonitorDetail::Joaat("am_mp_carwash_launch");
        static constexpr std::uint32_t LuxuryShowroomScriptHash = BusinessScriptMonitorDetail::Joaat("am_luxury_showroom");
        static constexpr std::uint32_t HangarScriptHash = BusinessScriptMonitorDetail::Joaat("gb_smuggler");
        static constexpr std::uint32_t VehicleCargoScriptHash = BusinessScriptMonitorDetail::Joaat("gb_vehicle_export");

        static BusinessScriptMonitorRuntime& Get() noexcept
        {
            static BusinessScriptMonitorRuntime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Refreshing Enhanced business script state");
            if (Runtime::GameRuntime::Get().Enqueue([this] {
                bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                if (!sessionStarted || !*sessionStarted)
                    return Finish(false, {}, "Join GTA Online before checking business scripts");

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                    return Finish(false, {}, "Shared Enhanced script runtime is unavailable");

                BusinessScriptMonitorSnapshot state;
                const auto running = [&](std::uint32_t hash) noexcept {
                    const auto* thread = scripts.FindThread(hash);
                    return thread && thread->stack;
                };

                state.businessHubRunning = running(BusinessHubScriptHash);
                state.bunkerRunning = running(BunkerScriptHash);
                state.acidLabRunning = running(AcidLabScriptHash);
                state.autoShopRunning = running(AutoShopScriptHash);
                state.bailOfficeRunning = running(BailOfficeScriptHash);
                state.casinoRunning = running(CasinoScriptHash);
                state.carWashRunning = running(CarWashScriptHash);
                state.luxuryShowroomRunning = running(LuxuryShowroomScriptHash);
                state.hangarRunning = running(HangarScriptHash);
                state.vehicleCargoRunning = running(VehicleCargoScriptHash);

                TUTONES_LOG_DEBUG(
                    "business.scripts",
                    std::string("Enhanced business scripts: hub=") + (state.businessHubRunning ? "running" : "idle")
                        + " bunker=" + (state.bunkerRunning ? "running" : "idle")
                        + " acid=" + (state.acidLabRunning ? "running" : "idle")
                        + " autoshop=" + (state.autoShopRunning ? "running" : "idle")
                        + " bail=" + (state.bailOfficeRunning ? "running" : "idle")
                        + " casino=" + (state.casinoRunning ? "running" : "idle")
                        + " carwash=" + (state.carWashRunning ? "running" : "idle")
                        + " showroom=" + (state.luxuryShowroomRunning ? "running" : "idle")
                        + " hangar=" + (state.hangarRunning ? "running" : "idle")
                        + " vehicle_cargo=" + (state.vehicleCargoRunning ? "running" : "idle"));

                Finish(true, state, "Enhanced business script state refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] BusinessScriptMonitorSnapshot Snapshot() const
        {
            BusinessScriptMonitorSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_State.haveResult;
            snapshot.lastSucceeded = m_State.lastSucceeded;
            snapshot.businessHubRunning = m_State.businessHubRunning;
            snapshot.bunkerRunning = m_State.bunkerRunning;
            snapshot.acidLabRunning = m_State.acidLabRunning;
            snapshot.autoShopRunning = m_State.autoShopRunning;
            snapshot.bailOfficeRunning = m_State.bailOfficeRunning;
            snapshot.casinoRunning = m_State.casinoRunning;
            snapshot.carWashRunning = m_State.carWashRunning;
            snapshot.luxuryShowroomRunning = m_State.luxuryShowroomRunning;
            snapshot.hangarRunning = m_State.hangarRunning;
            snapshot.vehicleCargoRunning = m_State.vehicleCargoRunning;
            snapshot.message = m_State.message;
            return snapshot;
        }

    private:
        BusinessScriptMonitorRuntime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, BusinessScriptMonitorSnapshot state, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                state.haveResult = true;
                state.lastSucceeded = success;
                state.pending = false;
                state.message = std::move(message);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        BusinessScriptMonitorSnapshot m_State{};
    };
}
