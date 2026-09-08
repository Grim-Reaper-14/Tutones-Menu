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
    namespace HangarEnhanced173
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

        inline constexpr std::uint32_t SmugglerScriptHash = Joaat("gb_smuggler");
        inline constexpr std::uint32_t HackerTruckScriptHash = Joaat("apphackertruck");
        inline constexpr std::uint32_t BusinessHubScriptHash = Joaat("am_mp_business_hub");

        // Names observed in the Enhanced apphackertruck decompile. These are exposed
        // for diagnostics only; Tutones does not submit a transaction until its full
        // catalog contract and state machine are verified for the current build.
        inline constexpr std::uint32_t SourceMissionTransactionHash =
            Joaat("HANGAR_CONTRABAND_MISSION_0_t0_v0");
        inline constexpr std::uint32_t SourceMissionStatHash =
            Joaat("MP_STAT_HANGAR_CONTRABAND_MISSION_v0");
    }

    struct HangarSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool scriptRuntimeReady{};
        bool smugglerRunning{};
        bool hackerTruckRunning{};
        bool businessHubRunning{};
        std::string message{"Press Refresh Hangar Runtime"};
    };

    class HangarRuntime final
    {
    public:
        static HangarRuntime& Get() noexcept
        {
            static HangarRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced Hangar / Air-Freight runtime");
            if (Runtime::GameRuntime::Get().Enqueue([this] {
                HangarSnapshot state;
                const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
                state.sessionStarted = sessionStarted && *sessionStarted;
                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading Hangar runtime state");

                auto& scripts = Script::ScriptRuntime::Get();
                state.scriptRuntimeReady = scripts.IsReady();
                if (!state.scriptRuntimeReady)
                    return Finish(false, std::move(state), "Shared Enhanced script runtime is unavailable");

                const auto running = [&](std::uint32_t hash) noexcept {
                    const auto* thread = scripts.FindThread(hash);
                    return thread && thread->stack
                        && thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                };

                state.smugglerRunning = running(HangarEnhanced173::SmugglerScriptHash);
                state.hackerTruckRunning = running(HangarEnhanced173::HackerTruckScriptHash);
                state.businessHubRunning = running(HangarEnhanced173::BusinessHubScriptHash);

                TUTONES_LOG_DEBUG(
                    "business.hangar",
                    std::string("Hangar runtime: smuggler=") + (state.smugglerRunning ? "running" : "idle")
                        + " apphackertruck=" + (state.hackerTruckRunning ? "running" : "idle")
                        + " business_hub=" + (state.businessHubRunning ? "running" : "idle"));

                Finish(true, std::move(state), "Enhanced Hangar / Air-Freight runtime refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] HangarSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            HangarSnapshot state = m_State;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        HangarRuntime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, HangarSnapshot state, std::string message)
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
        HangarSnapshot m_State{};
    };
}
