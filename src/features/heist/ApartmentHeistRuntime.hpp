#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
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
    namespace ApartmentHeistEnhanced173
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

        inline constexpr std::uint32_t MissionControllerHash = Joaat("fm_mission_controller");
        inline constexpr std::size_t CutScratchGlobal = 1936406;
        inline constexpr std::size_t CutScratchOffset = 1;
        inline constexpr std::size_t CutFinalGlobal = 1938374;
        inline constexpr std::size_t CutFinalOffset = 3008;
        inline constexpr std::size_t HackingStageLocal = 12263;
        inline constexpr std::size_t HackingFlagsLocal = 10233;
        inline constexpr std::size_t DrillingLocal = 10538;
        inline constexpr std::size_t SwipePlayerArrayBase = 32785;
        inline constexpr std::size_t SwipePlayerEntrySize = 294;
        inline constexpr std::size_t SwipePlayerOffset = 143;
        inline constexpr std::size_t SwipeStageLocal = 64655;
        inline constexpr int MaxCut = 100;
        using CutArray = std::array<int, 4>;
    }

    struct ApartmentHeistSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool controllerRunning{};
        bool setupReady{};
        bool cutsReady{};
        int planningStage{};
        ApartmentHeistEnhanced173::CutArray cuts{{0, 0, 0, 0}};
        std::string message{"Press Refresh Apartment State"};
    };

    class ApartmentHeistRuntime final
    {
    public:
        static ApartmentHeistRuntime& Get() noexcept
        {
            static ApartmentHeistRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Refreshing Apartment heist state", [this] {
                ApartmentHeistSnapshot state;
                const bool success = CaptureState(state);
                Finish(success, std::move(state), success ? "Apartment heist state refreshed" : "Apartment heist state unavailable");
            });
        }

        [[nodiscard]] bool QueueCompleteSetup()
        {
            return Queue("Completing current Apartment setup", [this] {
                ApartmentHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                    return Finish(false, std::move(state), "Join GTA Online before changing Apartment setup");

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                    return Finish(false, std::move(state), "Active GTA Online character is unavailable");

                const auto original = Stats::GetInt("MPX_HEIST_PLANNING_STAGE", *characterIndex);
                if (!original)
                    return Finish(false, std::move(state), "Apartment planning stat is unavailable");

                if (!Stats::SetInt("MPX_HEIST_PLANNING_STAGE", -1, *characterIndex))
                    return Finish(false, std::move(state), "Apartment setup write failed");

                const auto readback = Stats::GetInt("MPX_HEIST_PLANNING_STAGE", *characterIndex);
                if (!readback || *readback != -1)
                {
                    static_cast<void>(Stats::SetInt("MPX_HEIST_PLANNING_STAGE", *original, *characterIndex));
                    CaptureState(state);
                    return Finish(false, std::move(state), "Apartment setup verification failed; original state restored");
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.apartment", "Completed current Apartment heist planning stage");
                Finish(true, std::move(state), "Current Apartment setup completed");
            });
        }

        [[nodiscard]] bool QueueCuts(const ApartmentHeistEnhanced173::CutArray& requested)
        {
            using namespace ApartmentHeistEnhanced173;
            for (const int cut : requested)
            {
                if (cut < 0 || cut > MaxCut)
                    return false;
            }

            return Queue("Applying Apartment cuts", [this, requested] {
                ApartmentHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                    return Finish(false, std::move(state), "Join GTA Online before changing Apartment cuts");

                auto** globals = Script::ScriptRuntime::Get().Globals();
                state.globalsReady = globals != nullptr;
                if (!globals)
                    return Finish(false, std::move(state), "Apartment cut globals are unavailable");

                const auto scratch = Script::ScriptGlobal(CutScratchGlobal).At(CutScratchOffset);
                const auto finalCuts = Script::ScriptGlobal(CutFinalGlobal).At(CutFinalOffset);
                std::array<int, 8> originals{};
                for (std::size_t i = 0; i < 4; ++i)
                {
                    auto* a = scratch.At(i, 1).As<std::int32_t>(globals);
                    auto* b = finalCuts.At(i, 1).As<std::int32_t>(globals);
                    if (!a || !b)
                        return Finish(false, std::move(state), "Apartment cut globals could not be resolved");
                    originals[i] = *a;
                    originals[4 + i] = *b;
                }

                const int total = requested[0] + requested[1] + requested[2] + requested[3];
                const CutArray scratchValues{{100 - total, requested[1], requested[2], requested[3]}};
                bool verified = true;
                for (std::size_t i = 0; i < 4; ++i)
                {
                    auto* a = scratch.At(i, 1).As<std::int32_t>(globals);
                    auto* b = finalCuts.At(i, 1).As<std::int32_t>(globals);
                    if (!a || !b)
                    {
                        verified = false;
                        break;
                    }
                    *a = scratchValues[i];
                    *b = requested[i];
                    verified = verified && *a == scratchValues[i] && *b == requested[i];
                }

                if (!verified)
                {
                    for (std::size_t i = 0; i < 4; ++i)
                    {
                        if (auto* a = scratch.At(i, 1).As<std::int32_t>(globals))
                            *a = originals[i];
                        if (auto* b = finalCuts.At(i, 1).As<std::int32_t>(globals))
                            *b = originals[4 + i];
                    }
                    CaptureState(state);
                    return Finish(false, std::move(state), "Apartment cut verification failed; original cuts restored");
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.apartment", "Applied Apartment heist cuts");
                Finish(true, std::move(state), "Apartment cuts applied");
            });
        }

        [[nodiscard]] bool QueueSkipHacking()
        {
            using namespace ApartmentHeistEnhanced173;
            return QueueControllerAction("Skipping Apartment hacking", [this](Types::ScriptThread* thread, ApartmentHeistSnapshot& state) {
                auto* stage = Script::ScriptLocal(thread, HackingStageLocal).As<int>();
                auto* flags = Script::ScriptLocal(thread, HackingFlagsLocal).As<int>();
                if (!stage || !flags)
                    return Finish(false, std::move(state), "Apartment hacking locals are unavailable");
                const int originalStage = *stage;
                const int originalFlags = *flags;
                *stage = 7;
                *flags |= (1 << 9);
                if (*stage != 7 || ((*flags >> 9) & 1) == 0)
                {
                    *stage = originalStage;
                    *flags = originalFlags;
                    return Finish(false, std::move(state), "Apartment hacking skip failed verification");
                }
                CaptureState(state);
                Finish(true, std::move(state), "Hacking skipped");
            });
        }

        [[nodiscard]] bool QueueSkipDrilling()
        {
            using namespace ApartmentHeistEnhanced173;
            return QueueControllerAction("Skipping Apartment drilling", [this](Types::ScriptThread* thread, ApartmentHeistSnapshot& state) {
                auto* progress = Script::ScriptLocal(thread, DrillingLocal).As<float>();
                if (!progress)
                    return Finish(false, std::move(state), "Apartment drilling local is unavailable");
                const float original = *progress;
                *progress = 100.0f;
                if (*progress != 100.0f)
                {
                    *progress = original;
                    return Finish(false, std::move(state), "Apartment drilling skip failed verification");
                }
                CaptureState(state);
                Finish(true, std::move(state), "Drilling skipped");
            });
        }

        [[nodiscard]] bool QueueSkipSwiping()
        {
            using namespace ApartmentHeistEnhanced173;
            return QueueControllerAction("Skipping Apartment card swipe", [this](Types::ScriptThread* thread, ApartmentHeistSnapshot& state) {
                const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
                if (!playerId || *playerId < 0 || *playerId >= 32)
                    return Finish(false, std::move(state), "PLAYER_ID is unavailable");

                const std::size_t playerLocal = SwipePlayerArrayBase + 1 + (static_cast<std::size_t>(*playerId) * SwipePlayerEntrySize) + SwipePlayerOffset;
                auto* swipe = Script::ScriptLocal(thread, playerLocal).As<int>();
                auto* stage = Script::ScriptLocal(thread, SwipeStageLocal).As<int>();
                if (!swipe || !stage)
                    return Finish(false, std::move(state), "Apartment card-swipe locals are unavailable");

                const int originalSwipe = *swipe;
                const int originalStage = *stage;
                *swipe = 8;
                *stage = 5;
                if (*swipe != 8 || *stage != 5)
                {
                    *swipe = originalSwipe;
                    *stage = originalStage;
                    return Finish(false, std::move(state), "Apartment card-swipe skip failed verification");
                }
                CaptureState(state);
                Finish(true, std::move(state), "Card swipe skipped");
            });
        }

        [[nodiscard]] ApartmentHeistSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            ApartmentHeistSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        ApartmentHeistRuntime() = default;

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
            ApartmentHeistSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        template<typename Callback>
        [[nodiscard]] bool QueueControllerAction(std::string pendingMessage, Callback&& callback)
        {
            return Queue(std::move(pendingMessage), [this, callback = std::forward<Callback>(callback)]() mutable {
                ApartmentHeistSnapshot state;
                if (!RequireOnlineAndNatives(state))
                    return Finish(false, std::move(state), "Join GTA Online before using Apartment mission controls");
                auto& scripts = Script::ScriptRuntime::Get();
                auto* thread = scripts.FindThread(ApartmentHeistEnhanced173::MissionControllerHash);
                if (!thread || !thread->stack || thread->context.state == Types::ScriptThreadState::Killed)
                    return Finish(false, std::move(state), "Start an Apartment heist mission first");
                state.controllerRunning = true;
                callback(thread, state);
            });
        }

        [[nodiscard]] bool RequireOnlineAndNatives(ApartmentHeistSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(ApartmentHeistSnapshot& state) const noexcept
        {
            using namespace ApartmentHeistEnhanced173;
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            state.globalsReady = globals != nullptr;
            if (auto* thread = scripts.FindThread(MissionControllerHash))
                state.controllerRunning = thread->stack && thread->context.state != Types::ScriptThreadState::Killed;

            if (state.sessionStarted && state.nativeReady)
            {
                if (const auto characterIndex = Stats::GetCharIndex())
                {
                    if (const auto stage = Stats::GetInt("MPX_HEIST_PLANNING_STAGE", *characterIndex))
                    {
                        state.planningStage = *stage;
                        state.setupReady = true;
                    }
                }
            }

            if (globals)
            {
                const auto finalCuts = Script::ScriptGlobal(CutFinalGlobal).At(CutFinalOffset);
                state.cutsReady = true;
                for (std::size_t i = 0; i < state.cuts.size(); ++i)
                {
                    auto* cut = finalCuts.At(i, 1).As<std::int32_t>(globals);
                    if (!cut)
                    {
                        state.cutsReady = false;
                        break;
                    }
                    state.cuts[i] = *cut;
                }
            }

            return state.setupReady || state.cutsReady || state.controllerRunning;
        }

        void Finish(bool success, ApartmentHeistSnapshot state, std::string message) noexcept
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
        ApartmentHeistSnapshot m_Snapshot{};
    };
}
