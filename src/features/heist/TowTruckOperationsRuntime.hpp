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

namespace Tutones::Game::Heist
{
    namespace TowTruckEnhanced173
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

        inline constexpr std::uint32_t ControllerTowingHash = Joaat("controller_towing");
        inline constexpr std::uint32_t TowTruckWorkRewardHash =
            Joaat("SERVICE_EARN_AMBIENT_JOB_TOW_TRUCK_WORK");
        inline constexpr std::uint32_t SalvageVehicleRewardHash =
            Joaat("SERVICE_EARN_SALVAGE_VEHICLE");
        inline constexpr std::uint32_t SalvageYardSellRewardHash =
            Joaat("SERVICE_EARN_SALVAGE_YARD_SELL_VEH");
    }

    struct TowTruckOperationsSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool scriptRuntimeReady{};
        bool towingControllerRunning{};
        std::string message{"Press Refresh Tow Truck Runtime"};
    };

    class TowTruckOperationsRuntime final
    {
    public:
        static TowTruckOperationsRuntime& Get() noexcept
        {
            static TowTruckOperationsRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced Tow Truck runtime");
            if (Runtime::GameRuntime::Get().Enqueue([this] {
                TowTruckOperationsSnapshot state;
                const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                state.sessionStarted = sessionStarted && *sessionStarted;
                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading Tow Truck runtime state");

                auto& scripts = Script::ScriptRuntime::Get();
                state.scriptRuntimeReady = scripts.IsReady();
                if (!state.scriptRuntimeReady)
                    return Finish(false, std::move(state), "Shared Enhanced script runtime is unavailable");

                if (const auto* thread = scripts.FindThread(TowTruckEnhanced173::ControllerTowingHash))
                {
                    state.towingControllerRunning = thread->stack
                        && thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }

                TUTONES_LOG_DEBUG(
                    "heist.towtruck",
                    std::string("Tow Truck controller_towing=")
                        + (state.towingControllerRunning ? "running" : "idle"));
                Finish(true, std::move(state), "Enhanced Tow Truck runtime refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] TowTruckOperationsSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            TowTruckOperationsSnapshot state = m_State;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        TowTruckOperationsRuntime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, TowTruckOperationsSnapshot state, std::string message)
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);
            {
                std::scoped_lock lock(m_Mutex);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        TowTruckOperationsSnapshot m_State{};
    };
}
