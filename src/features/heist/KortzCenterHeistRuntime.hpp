#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
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
    namespace KortzCenterEnhanced
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

        inline constexpr std::uint32_t MissionControllerHash = Joaat("fm_mission_controller_v3");
        inline constexpr int TargetCount = 27;
        inline constexpr std::array<const char*, TargetCount> TargetNames{{
            "La Derniere Debauche", "Hare Oneself Think", "The Downfall Rome", "Brother Brother",
            "A Cast Characters", "Gone To Seed", "True Love", "Breathless", "Consumato",
            "I Hear Voices", "Winter, Nowhere in Particular", "The Girl With Pearl Necklace",
            "Chat on Fruit", "Pumpkin", "Twindifference", "Stacks Study V", "I, Fruit",
            "To Beat About Bush", "In Excess of Success", "Juiced", "A Winding Road Home",
            "Teckels", "Trust", "Until Death", "What Melons?", "The Outcome Endeavour", "Mi O Melee"
        }};

        [[nodiscard]] inline const char* TargetName(int target) noexcept
        {
            if (target < 0 || target >= TargetCount)
                return "Unknown";
            return TargetNames[static_cast<std::size_t>(target)];
        }

        enum class Action
        {
            SkipFingerprint,
            SkipSignalNodes,
            SkipDataCrack,
            CutGlass,
            DisableLaserGrid,
            TakePrimaryTarget,
            TakeSecondaryTarget,
        };
    }

    struct KortzCenterSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool globalsReady{};
        bool controllerRunning{};
        bool setupReady{};
        int target{};
        int generalBits{};
        int generalBits2{};
        int robberyProgress{};
        int scopingBits{};
        int poiBits{};
        std::string message{"Press Refresh Kortz State"};
    };

    class KortzCenterHeistRuntime final
    {
    public:
        static KortzCenterHeistRuntime& Get() noexcept
        {
            static KortzCenterHeistRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Refreshing Kortz Center state", [this] {
                KortzCenterSnapshot state;
                const bool success = CaptureState(state);
                Finish(success, std::move(state), success ? "Kortz Center state refreshed" : "Kortz Center state unavailable");
            });
        }

        [[nodiscard]] bool QueueSetup(int target)
        {
            using namespace KortzCenterEnhanced;
            if (target < 0 || target >= TargetCount)
                return false;

            return Queue("Completing Kortz Center setup", [this, target] {
                KortzCenterSnapshot state;
                if (!RequireOnlineAndNatives(state))
                    return Finish(false, std::move(state), "Join GTA Online before changing Kortz setup");

                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                    return Finish(false, std::move(state), "Active GTA Online character is unavailable");

                struct StatWrite final { const char* name; int value; int original; };
                std::array<StatWrite, 6> writes{{
                    {"MPX_K26_GENERAL_BS", -1, 0},
                    {"MPX_K26_GENERAL_BS2", -1, 0},
                    {"MPX_K26_ROBBERY_PROG", -1, 0},
                    {"MPX_K26_SCOPING_BS", -1, 0},
                    {"MPX_K26_POI_BS", -1, 0},
                    {"MPX_K26_HEIST_TARGET", target, 0},
                }};

                for (auto& write : writes)
                {
                    const auto original = Stats::GetInt(write.name, *characterIndex);
                    if (!original)
                        return Finish(false, std::move(state), std::string("Kortz stat unavailable: ") + write.name);
                    write.original = *original;
                }

                const char* failed{};
                for (const auto& write : writes)
                {
                    if (!Stats::SetInt(write.name, write.value, *characterIndex))
                    {
                        failed = write.name;
                        break;
                    }
                    const auto readback = Stats::GetInt(write.name, *characterIndex);
                    if (!readback || *readback != write.value)
                    {
                        failed = write.name;
                        break;
                    }
                }

                if (failed)
                {
                    bool restored = true;
                    for (const auto& write : writes)
                        restored = Stats::SetInt(write.name, write.original, *characterIndex) && restored;
                    CaptureState(state);
                    return Finish(false, std::move(state), restored ? "Kortz setup failed; original state restored" : "Kortz setup failed and rollback was incomplete");
                }

                CaptureState(state);
                TUTONES_LOG_INFO("heist.kortz", std::string("Completed Kortz Center setup for target ") + KortzCenterEnhanced::TargetName(target));
                Finish(true, std::move(state), "Kortz Center setup completed");
            });
        }

        [[nodiscard]] bool QueueAction(KortzCenterEnhanced::Action action)
        {
            using namespace KortzCenterEnhanced;
            return Queue("Applying Kortz Center mission control", [this, action] {
                KortzCenterSnapshot state;
                if (!RequireOnlineAndNatives(state))
                    return Finish(false, std::move(state), "Join GTA Online before using Kortz mission controls");

                auto& scripts = Script::ScriptRuntime::Get();
                auto* thread = scripts.FindThread(MissionControllerHash);
                if (!thread || !thread->stack || thread->context.state == Types::ScriptThreadState::Killed)
                    return Finish(false, std::move(state), "Start the Kortz Center finale first");
                state.controllerRunning = true;

                bool success = false;
                switch (action)
                {
                case Action::SkipFingerprint:
                    success = WriteLocalInt(thread, 26866, 5);
                    break;
                case Action::SkipSignalNodes:
                    success = WriteLocalInt(thread, 27914, 5);
                    break;
                case Action::SkipDataCrack:
                    success = WriteDataCrack(thread);
                    break;
                case Action::CutGlass:
                    success = WriteLocalFloat(thread, 32911, 100.0f);
                    break;
                case Action::DisableLaserGrid:
                    success = DisableLaserGrid(thread);
                    break;
                case Action::TakePrimaryTarget:
                    success = WriteLocalInt(thread, 29366, 10);
                    break;
                case Action::TakeSecondaryTarget:
                    success = WriteLocalInt(thread, 29366, 3);
                    break;
                }

                CaptureState(state);
                Finish(success, std::move(state), success ? "Kortz mission control applied" : "Kortz mission control failed verification");
            });
        }

        [[nodiscard]] KortzCenterSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            KortzCenterSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        KortzCenterHeistRuntime() = default;

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
            KortzCenterSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(KortzCenterSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] static bool WriteLocalInt(Types::ScriptThread* thread, std::size_t index, int value) noexcept
        {
            auto* local = Script::ScriptLocal(thread, index).As<int>();
            if (!local)
                return false;
            const int original = *local;
            *local = value;
            if (*local == value)
                return true;
            *local = original;
            return false;
        }

        [[nodiscard]] static bool WriteLocalFloat(Types::ScriptThread* thread, std::size_t index, float value) noexcept
        {
            auto* local = Script::ScriptLocal(thread, index).As<float>();
            if (!local)
                return false;
            const float original = *local;
            *local = value;
            if (*local == value)
                return true;
            *local = original;
            return false;
        }

        [[nodiscard]] static bool WriteDataCrack(Types::ScriptThread* thread) noexcept
        {
            std::array<int, 8> originals{};
            for (std::size_t i = 0; i < originals.size(); ++i)
            {
                const std::size_t index = 1388 + 1 + (i * 4);
                auto* local = Script::ScriptLocal(thread, index).As<int>();
                if (!local)
                    return false;
                originals[i] = *local;
            }
            for (std::size_t i = 0; i < originals.size(); ++i)
            {
                const std::size_t index = 1388 + 1 + (i * 4);
                auto* local = Script::ScriptLocal(thread, index).As<int>();
                if (!local)
                    return false;
                *local = 1;
                if (*local != 1)
                {
                    for (std::size_t j = 0; j <= i; ++j)
                    {
                        if (auto* rollback = Script::ScriptLocal(thread, 1388 + 1 + (j * 4)).As<int>())
                            *rollback = originals[j];
                    }
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static bool DisableLaserGrid(Types::ScriptThread* thread) noexcept
        {
            auto* local = Script::ScriptLocal(thread, 70416).As<int>();
            auto** globals = Script::ScriptRuntime::Get().Globals();
            auto* global = Script::ScriptGlobal(1935711).As<std::int32_t>(globals);
            if (!local || !global)
                return false;
            const int originalLocal = *local;
            const int originalGlobal = *global;
            *local = 4294784;
            *global |= 1;
            if (*local == 4294784 && ((*global) & 1) != 0)
                return true;
            *local = originalLocal;
            *global = originalGlobal;
            return false;
        }

        [[nodiscard]] bool CaptureState(KortzCenterSnapshot& state) const noexcept
        {
            using namespace KortzCenterEnhanced;
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            auto& scripts = Script::ScriptRuntime::Get();
            state.globalsReady = scripts.Globals() != nullptr;
            if (auto* thread = scripts.FindThread(MissionControllerHash))
                state.controllerRunning = thread->stack && thread->context.state != Types::ScriptThreadState::Killed;

            if (!state.sessionStarted || !state.nativeReady)
                return state.controllerRunning;

            const auto characterIndex = Stats::GetCharIndex();
            if (!characterIndex)
                return state.controllerRunning;

            const auto general = Stats::GetInt("MPX_K26_GENERAL_BS", *characterIndex);
            const auto general2 = Stats::GetInt("MPX_K26_GENERAL_BS2", *characterIndex);
            const auto progress = Stats::GetInt("MPX_K26_ROBBERY_PROG", *characterIndex);
            const auto scoping = Stats::GetInt("MPX_K26_SCOPING_BS", *characterIndex);
            const auto poi = Stats::GetInt("MPX_K26_POI_BS", *characterIndex);
            const auto target = Stats::GetInt("MPX_K26_HEIST_TARGET", *characterIndex);
            state.setupReady = general && general2 && progress && scoping && poi && target;
            if (state.setupReady)
            {
                state.generalBits = *general;
                state.generalBits2 = *general2;
                state.robberyProgress = *progress;
                state.scopingBits = *scoping;
                state.poiBits = *poi;
                state.target = *target;
            }
            return state.setupReady || state.controllerRunning;
        }

        void Finish(bool success, KortzCenterSnapshot state, std::string message) noexcept
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
        KortzCenterSnapshot m_Snapshot{};
    };
}
