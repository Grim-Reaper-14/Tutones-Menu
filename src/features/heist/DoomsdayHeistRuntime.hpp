#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::Heist
{
    namespace DoomsdayHeistEnhanced173
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

        inline constexpr std::uint32_t PlanningHash = Joaat("gb_gang_ops_planning");
        inline constexpr std::size_t PlanningGlobal = 1969071;
        inline constexpr std::size_t CutsOffsetA = 812;
        inline constexpr std::size_t CutsOffsetB = 50;
        inline constexpr int ActCount = 3;
        inline constexpr int MaxCut = 100;

        inline constexpr std::array<const char*, ActCount> ActNames{
            "Act I - The Data Breaches",
            "Act II - The Bogdan Problem",
            "Act III - The Doomsday Scenario",
        };

        struct SetupState final
        {
            int missionProgress{};
            int heistStatus{};
            int notifications{1557};
        };

        inline constexpr std::array<SetupState, ActCount> SetupStates{{
            {503, -229383, 1557},
            {240, -229378, 1557},
            {16368, -229380, 1557},
        }};

        using CutArray = std::array<int, 4>;

        [[nodiscard]] inline const char* ActName(int act) noexcept
        {
            if (act < 0 || act >= ActCount)
                return "Unknown / custom state";
            return ActNames[static_cast<std::size_t>(act)];
        }

        [[nodiscard]] inline int DetectAct(int missionProgress, int heistStatus) noexcept
        {
            for (int act = 0; act < ActCount; ++act)
            {
                const auto& setup = SetupStates[static_cast<std::size_t>(act)];
                if (setup.missionProgress == missionProgress && setup.heistStatus == heistStatus)
                    return act;
            }
            return -1;
        }
    }

    struct DoomsdayHeistSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool planningRunning{};
        bool setupReady{};
        bool cutsReady{};
        int act{-1};
        int missionProgress{};
        int heistStatus{};
        int notifications{};
        DoomsdayHeistEnhanced173::CutArray cuts{{0, 0, 0, 0}};
        std::string message{"Press Refresh Doomsday State"};
    };

    class DoomsdayHeistRuntime final
    {
    public:
        static DoomsdayHeistRuntime& Get() noexcept
        {
            static DoomsdayHeistRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Enhanced Doomsday planning state", [this] {
                DoomsdayHeistSnapshot state;
                const bool success = CaptureState(state);
                Finish(
                    success,
                    std::move(state),
                    success
                        ? "Doomsday planning state refreshed"
                        : "Unable to read the Enhanced Doomsday planning state");
            });
        }

        [[nodiscard]] bool QueueSetup(int act)
        {
            using namespace DoomsdayHeistEnhanced173;
            if (act < 0 || act >= ActCount)
                return false;

            return Queue("Applying verified Doomsday setup state", [this, act] {
                DoomsdayHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before changing Doomsday setup state");
                    return;
                }

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to resolve the active GTA Online character slot");
                    return;
                }

                const auto& setup = SetupStates[static_cast<std::size_t>(act)];
                std::array<StatWrite, 3> writes{{
                    {"MPX_GANGOPS_FLOW_MISSION_PROG", setup.missionProgress, 0},
                    {"MPX_GANGOPS_HEIST_STATUS", setup.heistStatus, 0},
                    {"MPX_GANGOPS_FLOW_NOTIFICATIONS", setup.notifications, 0},
                }};

                for (auto& write : writes)
                {
                    const auto original = Stats::GetInt(write.name, *characterIndex);
                    if (!original)
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), std::string("Unable to capture original Doomsday stat: ") + write.name);
                        return;
                    }
                    write.original = *original;
                }

                const char* failedStat{};
                for (const auto& write : writes)
                {
                    if (!Stats::SetInt(write.name, write.value, *characterIndex))
                    {
                        failedStat = write.name;
                        break;
                    }
                    const auto readback = Stats::GetInt(write.name, *characterIndex);
                    if (!readback || *readback != write.value)
                    {
                        failedStat = write.name;
                        break;
                    }
                }

                if (failedStat)
                {
                    const bool restored = RestoreStats(writes, *characterIndex);
                    CaptureState(state);
                    Finish(
                        false,
                        std::move(state),
                        restored
                            ? std::string("Doomsday setup write failed verification at ") + failedStat + "; original state restored"
                            : std::string("Doomsday setup write failed at ") + failedStat + "; rollback could not be fully verified");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.doomsday", std::string("Applied ") + ActName(act));
                Finish(
                    true,
                    std::move(state),
                    state.planningRunning
                        ? "Doomsday setup applied; close and reopen the Facility planning screen to redraw it"
                        : "Doomsday setup applied; open the Facility planning screen to load the new state");
            });
        }

        [[nodiscard]] bool QueueCuts(const DoomsdayHeistEnhanced173::CutArray& requested)
        {
            using namespace DoomsdayHeistEnhanced173;
            for (const int cut : requested)
            {
                if (cut < 0 || cut > MaxCut)
                    return false;
            }

            return Queue("Applying Doomsday player cuts", [this, requested] {
                DoomsdayHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online before changing Doomsday cuts");
                    return;
                }

                auto** globals = Script::ScriptRuntime::Get().Globals();
                state.globalsReady = globals != nullptr;
                if (!globals)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Doomsday cut globals are unavailable");
                    return;
                }

                auto base = Script::ScriptGlobal(PlanningGlobal).At(CutsOffsetA).At(CutsOffsetB);
                CutArray originals{};
                for (std::size_t index = 0; index < requested.size(); ++index)
                {
                    auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        CaptureState(state);
                        Finish(false, std::move(state), "Unable to resolve Doomsday cut globals");
                        return;
                    }
                    originals[index] = *cut;
                }

                bool verified = true;
                for (std::size_t index = 0; index < requested.size(); ++index)
                {
                    auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        verified = false;
                        break;
                    }
                    *cut = requested[index];
                    verified = *cut == requested[index] && verified;
                }

                if (!verified)
                {
                    for (std::size_t index = 0; index < originals.size(); ++index)
                    {
                        if (auto* cut = base.At(index, 1).As<std::int32_t>(globals))
                            *cut = originals[index];
                    }
                    CaptureState(state);
                    Finish(false, std::move(state), "Doomsday cut write failed verification; original cuts restored");
                    return;
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.doomsday", "Applied Doomsday player cuts with read-back verification");
                Finish(true, std::move(state), "Doomsday player cuts applied");
            });
        }

        [[nodiscard]] DoomsdayHeistSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            DoomsdayHeistSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        struct StatWrite final
        {
            const char* name{};
            int value{};
            int original{};
        };

        DoomsdayHeistRuntime() = default;
        DoomsdayHeistRuntime(const DoomsdayHeistRuntime&) = delete;
        DoomsdayHeistRuntime& operator=(const DoomsdayHeistRuntime&) = delete;

        template<typename Callback>
        [[nodiscard]] bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = false;
                m_Snapshot.lastSucceeded = false;
                m_Snapshot.message = std::move(pendingMessage);
            }

            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            DoomsdayHeistSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(DoomsdayHeistSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(DoomsdayHeistSnapshot& state) const noexcept
        {
            using namespace DoomsdayHeistEnhanced173;

            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (scripts.IsReady())
            {
                if (const auto* thread = scripts.FindThread(PlanningHash))
                {
                    state.planningRunning = thread->context.threadId != 0
                        && thread->context.state != Types::ScriptThreadState::Killed;
                }
            }

            if (!state.sessionStarted || !state.nativeReady)
                return false;

            const auto characterIndex = Stats::GetCharIndex();
            if (characterIndex)
            {
                const auto missionProgress = Stats::GetInt("MPX_GANGOPS_FLOW_MISSION_PROG", *characterIndex);
                const auto heistStatus = Stats::GetInt("MPX_GANGOPS_HEIST_STATUS", *characterIndex);
                const auto notifications = Stats::GetInt("MPX_GANGOPS_FLOW_NOTIFICATIONS", *characterIndex);
                state.setupReady = missionProgress && heistStatus && notifications;
                if (state.setupReady)
                {
                    state.missionProgress = *missionProgress;
                    state.heistStatus = *heistStatus;
                    state.notifications = *notifications;
                    state.act = DetectAct(*missionProgress, *heistStatus);
                }
            }

            if (globals)
            {
                auto base = Script::ScriptGlobal(PlanningGlobal).At(CutsOffsetA).At(CutsOffsetB);
                state.cutsReady = true;
                for (std::size_t index = 0; index < state.cuts.size(); ++index)
                {
                    const auto* cut = base.At(index, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        state.cutsReady = false;
                        break;
                    }
                    state.cuts[index] = *cut;
                }
            }

            return state.setupReady || state.cutsReady;
        }

        template<std::size_t Count>
        [[nodiscard]] static bool RestoreStats(
            const std::array<StatWrite, Count>& writes,
            int characterIndex) noexcept
        {
            bool restored = true;
            for (const auto& write : writes)
                restored = Stats::SetInt(write.name, write.original, characterIndex) && restored;

            for (const auto& write : writes)
            {
                const auto readback = Stats::GetInt(write.name, characterIndex);
                restored = readback && *readback == write.original && restored;
            }
            return restored;
        }

        void Finish(bool success, DoomsdayHeistSnapshot state, std::string message) noexcept
        {
            state.pending = false;
            state.haveResult = true;
            state.lastSucceeded = success;
            state.message = std::move(message);
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        DoomsdayHeistSnapshot m_Snapshot{};
    };
}
