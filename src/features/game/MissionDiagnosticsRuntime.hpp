#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::MissionDiagnostics
{
    namespace Enhanced173
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

        inline constexpr std::uint32_t MissionLaunchHash = Joaat("am_mission_launch");
        inline constexpr std::size_t StateBaseLocal = 121;
        inline constexpr std::size_t FlagsLocal = StateBaseLocal;
        inline constexpr std::size_t LaunchStateLocal = StateBaseLocal + 2;
        inline constexpr std::size_t MissionVariantLocal = StateBaseLocal + 5;
        inline constexpr std::size_t EntityPhaseLocal = StateBaseLocal + 6;
    }

    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool scriptRuntimeReady{};
        bool threadFound{};
        bool stackReady{};
        bool programLoaded{};
        std::uint32_t threadId{};
        std::uint32_t programCounter{};
        std::uint32_t stackSize{};
        std::uint32_t flags{};
        int launchState{-1};
        int missionVariant{-1};
        int entityPhase{-1};
        std::string message{"Ready"};
    };

    class Runtime final
    {
    public:
        static Runtime& Get() noexcept
        {
            static Runtime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced mission-launch state");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                auto& scripts = Script::ScriptRuntime::Get();
                state.scriptRuntimeReady = scripts.IsReady();
                if (!state.scriptRuntimeReady)
                    return Finish(false, std::move(state), "Shared Enhanced script runtime is unavailable");

                const auto* thread = scripts.FindThread(Enhanced173::MissionLaunchHash);
                if (!thread)
                    return Finish(true, std::move(state), "am_mission_launch is not running");

                state.threadFound = true;
                state.stackReady = thread->stack != nullptr;
                state.threadId = thread->context.threadId;
                state.programCounter = thread->context.programCounter;
                state.stackSize = thread->context.stackSize;
                state.programLoaded = scripts.FindProgram(Enhanced173::MissionLaunchHash) != nullptr;
                if (!state.stackReady)
                    return Finish(false, std::move(state), "am_mission_launch thread has no readable stack");

                const auto flags = scripts.ReadLocalRaw(Enhanced173::MissionLaunchHash, Enhanced173::FlagsLocal);
                const auto launchState = scripts.ReadLocalRaw(Enhanced173::MissionLaunchHash, Enhanced173::LaunchStateLocal);
                const auto missionVariant = scripts.ReadLocalRaw(Enhanced173::MissionLaunchHash, Enhanced173::MissionVariantLocal);
                const auto entityPhase = scripts.ReadLocalRaw(Enhanced173::MissionLaunchHash, Enhanced173::EntityPhaseLocal);
                if (!flags || !launchState || !missionVariant || !entityPhase)
                    return Finish(false, std::move(state), "Unable to read validated am_mission_launch locals");

                state.flags = static_cast<std::uint32_t>(*flags);
                state.launchState = static_cast<int>(static_cast<std::int64_t>(*launchState));
                state.missionVariant = static_cast<int>(static_cast<std::int64_t>(*missionVariant));
                state.entityPhase = static_cast<int>(static_cast<std::int64_t>(*entityPhase));

                const bool launchStateValid = state.launchState >= 0 && state.launchState <= 7;
                const bool entityPhaseValid = state.entityPhase >= 0 && state.entityPhase <= 4;
                if (!launchStateValid || !entityPhaseValid)
                    return Finish(false, std::move(state), "Enhanced mission local layout validation failed");

                TUTONES_LOG_DEBUG("mission.diagnostics", "Enhanced mission launch state refreshed");
                Finish(true, std::move(state), "Enhanced mission launch state refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] Snapshot GetSnapshot() const
        {
            Snapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_State.haveResult;
            snapshot.lastSucceeded = m_State.lastSucceeded;
            snapshot.sessionStarted = m_State.sessionStarted;
            snapshot.scriptRuntimeReady = m_State.scriptRuntimeReady;
            snapshot.threadFound = m_State.threadFound;
            snapshot.stackReady = m_State.stackReady;
            snapshot.programLoaded = m_State.programLoaded;
            snapshot.threadId = m_State.threadId;
            snapshot.programCounter = m_State.programCounter;
            snapshot.stackSize = m_State.stackSize;
            snapshot.flags = m_State.flags;
            snapshot.launchState = m_State.launchState;
            snapshot.missionVariant = m_State.missionVariant;
            snapshot.entityPhase = m_State.entityPhase;
            snapshot.message = m_State.message;
            return snapshot;
        }

    private:
        Runtime() = default;

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, Snapshot state, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                state.pending = false;
                state.haveResult = true;
                state.lastSucceeded = success;
                state.message = std::move(message);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        Snapshot m_State{};
    };
}
