#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Stats.hpp"
#include "../../game/native/NativeRegistry.hpp"
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
    namespace AutoShopEnhanced173
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

        inline constexpr std::uint32_t TunerPlanningHash = Joaat("tuner_planning");
        inline constexpr std::size_t PlanningReloadLocalA = 406;
        inline constexpr std::size_t PlanningReloadLocalB = 408;
        inline constexpr int PlanningReloadValue = 2;
        inline constexpr int ContractCount = 8;

        inline constexpr std::array<const char*, ContractCount> ContractNames{
            "Union Depository",
            "Superdollar Deal",
            "Bank Contract",
            "ECU Job",
            "Prison Contract",
            "Agency Deal",
            "Lost Contract",
            "Data Contract",
        };

        [[nodiscard]] constexpr int PrepMaskForContract(int contractIndex) noexcept
        {
            return contractIndex == 1 ? 4351 : 12543;
        }
    }

    [[nodiscard]] inline const char* AutoShopContractName(int contractIndex) noexcept
    {
        if (contractIndex < 0 || contractIndex >= AutoShopEnhanced173::ContractCount)
            return "Unknown";
        return AutoShopEnhanced173::ContractNames[static_cast<std::size_t>(contractIndex)];
    }

    struct AutoShopContractSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool nativeReady{};
        bool planningRunning{};
        int currentContract{-1};
        int prepMask{-1};
        std::string message{"Ready"};
    };

    class AutoShopContractRuntime final
    {
    public:
        static AutoShopContractRuntime& Get() noexcept
        {
            static AutoShopContractRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRefresh()
        {
            return Queue("Reading Auto Shop contract state", [this] {
                AutoShopContractSnapshot state;
                if (!CaptureState(state))
                {
                    Finish(false, std::move(state), "Unable to read Auto Shop contract stats");
                    return;
                }

                Finish(true, std::move(state), "Auto Shop contract state refreshed");
            });
        }

        [[nodiscard]] bool QueueReadyFinale(int contractIndex)
        {
            if (contractIndex < 0 || contractIndex >= AutoShopEnhanced173::ContractCount)
                return false;

            return Queue("Preparing selected Auto Shop contract finale", [this, contractIndex] {
                AutoShopContractSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online and wait for the native backend before using Auto Shop heist tools");
                    return;
                }

                const int prepMask = AutoShopEnhanced173::PrepMaskForContract(contractIndex);
                const auto characterIndex = Stats::GetCharIndex();
                if (!characterIndex)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to resolve the active GTA Online character slot");
                    return;
                }

                const auto originalCurrent = Stats::GetInt("MPX_TUNER_CURRENT", *characterIndex);
                const auto originalPrep = Stats::GetInt("MPX_TUNER_GEN_BS", *characterIndex);
                if (!originalCurrent || !originalPrep)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Unable to capture the original Auto Shop contract state");
                    return;
                }

                const bool currentWritten = Stats::SetInt("MPX_TUNER_CURRENT", contractIndex, *characterIndex);
                const bool prepWritten = Stats::SetInt("MPX_TUNER_GEN_BS", prepMask, *characterIndex);
                const auto currentReadback = Stats::GetInt("MPX_TUNER_CURRENT", *characterIndex);
                const auto prepReadback = Stats::GetInt("MPX_TUNER_GEN_BS", *characterIndex);

                if (!currentWritten || !prepWritten
                    || !currentReadback || *currentReadback != contractIndex
                    || !prepReadback || *prepReadback != prepMask)
                {
                    const bool restored = RestoreContractStats(
                        *characterIndex,
                        *originalCurrent,
                        *originalPrep);
                    CaptureState(state);
                    Finish(false, std::move(state),
                        restored
                            ? "Auto Shop stat write failed verification; original contract state restored"
                            : "Auto Shop stat write failed verification and rollback could not be confirmed");
                    return;
                }

                bool planningRunning = false;
                bool boardReloaded = false;
                auto& scripts = Script::ScriptRuntime::Get();
                if (scripts.IsReady())
                {
                    if (auto* thread = scripts.FindThread(AutoShopEnhanced173::TunerPlanningHash);
                        IsCompatiblePlanningThread(thread))
                    {
                        planningRunning = true;
                        boardReloaded = ReloadPlanningBoard(thread);
                        if (!boardReloaded)
                        {
                            CaptureState(state);
                            Finish(false, std::move(state), "Contract preps were written, but the active planning board failed to reload");
                            return;
                        }
                    }
                }

                CaptureState(state);
                state.currentContract = contractIndex;
                state.prepMask = prepMask;
                state.planningRunning = planningRunning;

                TUTONES_LOG_INFO(
                    "heist.autoshop",
                    std::string("Readied Auto Shop contract for finale: ")
                        + AutoShopContractName(contractIndex)
                        + " contract=" + std::to_string(contractIndex)
                        + " prepMask=" + std::to_string(prepMask));

                if (boardReloaded)
                {
                    Finish(true, std::move(state),
                        std::string(AutoShopContractName(contractIndex)) + " is ready for the finale; planning board reloaded");
                }
                else
                {
                    Finish(true, std::move(state),
                        std::string(AutoShopContractName(contractIndex))
                            + " prep state is complete; open/re-enter the Auto Shop planning board, then use Reload Planning Board");
                }
            });
        }

        [[nodiscard]] bool QueueReloadPlanning()
        {
            return Queue("Reloading Auto Shop planning board", [this] {
                AutoShopContractSnapshot state;
                if (!RequireOnlineAndNatives(state))
                {
                    Finish(false, std::move(state), "Join GTA Online before reloading the Auto Shop planning board");
                    return;
                }

                auto& scripts = Script::ScriptRuntime::Get();
                if (!scripts.IsReady())
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Shared script runtime is unavailable");
                    return;
                }

                auto* thread = scripts.FindThread(AutoShopEnhanced173::TunerPlanningHash);
                if (!thread)
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "Open the Auto Shop planning board first (tuner_planning is not running)");
                    return;
                }
                if (!IsCompatiblePlanningThread(thread))
                {
                    CaptureState(state);
                    Finish(false, std::move(state), "tuner_planning layout is incompatible; reload was blocked safely");
                    return;
                }

                const bool success = ReloadPlanningBoard(thread);
                CaptureState(state);
                Finish(success, std::move(state),
                    success ? "Auto Shop planning board reload requested" : "Auto Shop planning board reload failed");
            });
        }

        [[nodiscard]] AutoShopContractSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            AutoShopContractSnapshot state = m_Snapshot;
            state.pending = m_Pending.load(std::memory_order_acquire);
            return state;
        }

    private:
        AutoShopContractRuntime() = default;
        AutoShopContractRuntime(const AutoShopContractRuntime&) = delete;
        AutoShopContractRuntime& operator=(const AutoShopContractRuntime&) = delete;

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

            AutoShopContractSnapshot state;
            Finish(false, std::move(state), "GTA script-thread queue is unavailable");
            return false;
        }

        [[nodiscard]] bool RequireOnlineAndNatives(AutoShopContractSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();
            return state.sessionStarted && state.nativeReady;
        }

        [[nodiscard]] bool CaptureState(AutoShopContractSnapshot& state) const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            state.sessionStarted = sessionStarted && *sessionStarted;
            state.nativeReady = Native::NativeRegistry::Get().CanInvokeOnCurrentThread();

            auto& scripts = Script::ScriptRuntime::Get();
            if (scripts.IsReady())
            {
                const auto* thread = scripts.FindThread(AutoShopEnhanced173::TunerPlanningHash);
                state.planningRunning = IsCompatiblePlanningThread(thread);
            }

            if (!state.sessionStarted || !state.nativeReady)
                return false;

            const auto current = Stats::GetInt("MPX_TUNER_CURRENT");
            const auto prep = Stats::GetInt("MPX_TUNER_GEN_BS");
            if (!current || !prep)
                return false;

            state.currentContract = *current;
            state.prepMask = *prep;
            return true;
        }

        [[nodiscard]] bool ReloadPlanningBoard(Types::ScriptThread* thread) const noexcept
        {
            if (!IsCompatiblePlanningThread(thread))
                return false;

            int* reloadA = Script::ScriptLocal(thread, AutoShopEnhanced173::PlanningReloadLocalA).As<int>();
            int* reloadB = Script::ScriptLocal(thread, AutoShopEnhanced173::PlanningReloadLocalB).As<int>();
            if (!reloadA || !reloadB)
                return false;

            const int originalA = *reloadA;
            const int originalB = *reloadB;
            *reloadA = AutoShopEnhanced173::PlanningReloadValue;
            *reloadB = AutoShopEnhanced173::PlanningReloadValue;
            const bool verified = *reloadA == AutoShopEnhanced173::PlanningReloadValue
                && *reloadB == AutoShopEnhanced173::PlanningReloadValue;
            if (!verified)
            {
                *reloadA = originalA;
                *reloadB = originalB;
            }
            return verified;
        }

        [[nodiscard]] static bool IsCompatiblePlanningThread(const Types::ScriptThread* thread) noexcept
        {
            return thread
                && thread->context.threadId != 0
                && thread->scriptHash == AutoShopEnhanced173::TunerPlanningHash
                && thread->context.state != Types::ScriptThreadState::Killed
                && thread->stack
                && static_cast<std::size_t>(thread->context.stackSize)
                    > AutoShopEnhanced173::PlanningReloadLocalB;
        }

        [[nodiscard]] static bool RestoreContractStats(
            int characterIndex,
            int originalCurrent,
            int originalPrep) noexcept
        {
            const bool currentRestored = Stats::SetInt(
                "MPX_TUNER_CURRENT",
                originalCurrent,
                characterIndex);
            const bool prepRestored = Stats::SetInt(
                "MPX_TUNER_GEN_BS",
                originalPrep,
                characterIndex);
            const auto currentReadback = Stats::GetInt("MPX_TUNER_CURRENT", characterIndex);
            const auto prepReadback = Stats::GetInt("MPX_TUNER_GEN_BS", characterIndex);
            return currentRestored
                && prepRestored
                && currentReadback && *currentReadback == originalCurrent
                && prepReadback && *prepReadback == originalPrep;
        }

        void Finish(bool success, AutoShopContractSnapshot state, std::string message) noexcept
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
        AutoShopContractSnapshot m_Snapshot{};
    };
}
