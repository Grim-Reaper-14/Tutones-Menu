#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/GameState.hpp"
#include "../../game/Natives.hpp"
#include "../../game/script/ScriptFunction.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../game/types/VehicleRewardData.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::PersonalVehicles
{
    namespace SavePersonalVehicleDetail
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

    struct SavePersonalVehicleSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    // AM_MP_VEHICLE_REWARD's GiveVehicleReward function is a multi-step script
    // function. A false return means the script has more work to do after yielding;
    // it is not an invocation failure. Tutones therefore continues the function at a
    // controlled cadence until GTA explicitly reports ControlStatus == 3.
    //
    // Once ControlStatus reaches 3 the GTA garage selector owns the transaction. At
    // that point Tutones stops re-entering GiveVehicleReward and only observes the
    // reward locals. This preserves the continuation contract needed to open the UI
    // without recreating the old rapid open/close loop while the player is choosing.
    class SavePersonalVehicleRuntime final
    {
    public:
        static SavePersonalVehicleRuntime& Get() noexcept
        {
            static SavePersonalVehicleRuntime instance;
            return instance;
        }

        bool QueueSaveCurrent()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            ResetFlowState();
            SetPending("Save Personal Vehicle queued");

            if (Runtime::GameRuntime::Get().Enqueue([this] { BeginOnGameThread(); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            SetResult(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] SavePersonalVehicleSnapshot Snapshot() const
        {
            SavePersonalVehicleSnapshot out;
            out.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            out.haveResult = m_HaveResult;
            out.lastSucceeded = m_LastSucceeded;
            out.message = m_Message;
            return out;
        }

    private:
        using Clock = std::chrono::steady_clock;

        static constexpr std::uint32_t FreemodeHash = SavePersonalVehicleDetail::Joaat("freemode");
        static constexpr std::uint32_t VehicleRewardHash = SavePersonalVehicleDetail::Joaat("am_mp_vehicle_reward");
        static constexpr std::size_t FreemodeGeneralGlobal = 2733326;
        static constexpr std::size_t PersonalVehicleIndexOffset = 301;

        static constexpr auto FlowTimeout = std::chrono::seconds(60);
        static constexpr auto ContinuationDelay = std::chrono::milliseconds(50);
        static constexpr auto CompletedWithoutSelectorDelay = std::chrono::milliseconds(200);
        static constexpr int MaxContinuationCalls = 120;

        static constexpr std::array<std::uint32_t, 7> BlacklistedModels{
            SavePersonalVehicleDetail::Joaat("rcbandito"),
            SavePersonalVehicleDetail::Joaat("minitank"),
            SavePersonalVehicleDetail::Joaat("thruster"),
            SavePersonalVehicleDetail::Joaat("terbyte"),
            SavePersonalVehicleDetail::Joaat("avenger"),
            SavePersonalVehicleDetail::Joaat("policet3"),
            SavePersonalVehicleDetail::Joaat("brickade2")
        };

        SavePersonalVehicleRuntime() = default;

        void BeginOnGameThread()
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, "Join GTA Online before saving a personal vehicle");

            const auto state = GameState::Get().Snapshot();
            if (!state.inVehicle || state.vehicle == 0)
                return Finish(false, "Enter the vehicle you want to save first");

            const auto model = Natives::GetEntityModel(state.vehicle);
            if (!model || *model == 0)
                return Finish(false, "Could not read the current vehicle model");

            for (const auto blocked : BlacklistedModels)
            {
                if (*model == blocked)
                    return Finish(false, "This vehicle is blocked from personal-garage acquisition");
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, "Shared script runtime is unavailable");

            auto** globals = scripts.Globals();
            if (globals)
            {
                if (int* personalVehicle = Script::ScriptGlobal(FreemodeGeneralGlobal)
                        .At(PersonalVehicleIndexOffset)
                        .As<int>(globals))
                {
                    if (*personalVehicle == state.vehicle)
                        return Finish(false, "This vehicle is already your active personal vehicle");
                }
            }

            auto* freemode = scripts.FindThread(FreemodeHash);
            if (!freemode || !freemode->stack)
                return Finish(false, "Freemode script is unavailable");

            static Script::ScriptFunction isVehicleValidForPv(
                FreemodeHash,
                Script::ScriptPointer("IsVehicleValidForPV", "5D ? ? ? 2A 06 56 13 00 38 00").Add(1).Rip());

            const auto valid = isVehicleValidForPv.TryCall<bool>(*model);
            if (!valid || !*valid)
                return Finish(false, "GTA rejected this model as a personal vehicle");

            m_Thread = scripts.FindThread(VehicleRewardHash);
            if (!m_Thread || !m_Thread->stack)
                return Finish(false, "GTA vehicle-reward script is not active; try again in Freemode");

            auto* rewardData = Types::VehicleRewardData::Get(m_Thread);
            auto* vehicleMenuData = Script::ScriptLocal(m_Thread, 195).As<int>();
            if (!rewardData || !vehicleMenuData)
                return Finish(false, "Vehicle-reward script locals are unavailable");

            if (rewardData->controlStatus == 3)
                return Finish(false, "GTA already has a vehicle-reward selector active");

            // Start from a clean reward transaction. These values are never cleared
            // again until GTA has actually owned and closed the selector.
            rewardData->transactionStatus = 0;
            rewardData->garage = 0;
            rewardData->garageOffset = 0;
            rewardData->controlStatus = 0;

            m_Vehicle = state.vehicle;
            m_Deadline = Clock::now() + FlowTimeout;
            m_NextCallAt = Clock::now();

            TUTONES_LOG_INFO(
                "vehicle.savepv",
                std::string("Starting paced AM_MP_VEHICLE_REWARD flow; threadId=")
                    + std::to_string(m_Thread->context.threadId)
                    + " vehicle=" + std::to_string(m_Vehicle));

            SetPending("Opening GTA personal-garage selector...");
            ContinueRewardFlow();
        }

        void ContinueRewardFlow()
        {
            if (!ValidateLiveFlow())
                return;

            auto* rewardData = Types::VehicleRewardData::Get(m_Thread);
            auto* vehicleMenuData = Script::ScriptLocal(m_Thread, 195).As<int>();
            if (!rewardData || !vehicleMenuData)
                return Finish(false, "Vehicle-reward script locals are unavailable");

            // ControlStatus == 3 is the only reliable signal that GTA's garage
            // selector really opened. Transaction/Garage locals can become non-zero
            // earlier while the script is only preparing the frontend.
            if (rewardData->controlStatus == 3)
            {
                if (!m_SelectorObserved)
                {
                    m_SelectorObserved = true;
                    TUTONES_LOG_INFO("vehicle.savepv", "GTA personal-garage selector entered ControlStatus 3");
                }

                LogState("selector", -1, rewardData);
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueContinueStep();
            }

            if (m_SelectorObserved)
            {
                // GTA previously owned the selector and has now left ControlStatus 3.
                // Match the reward-script cleanup contract only after that transition.
                LogState("complete", -1, rewardData);
                rewardData->transactionStatus = 0;
                rewardData->garage = 0;
                rewardData->garageOffset = 0;
                rewardData->controlStatus = 0;
                return Finish(true, "GTA personal-garage selector closed cleanly");
            }

            const auto now = Clock::now();
            if (now < m_NextCallAt)
                return QueueContinueStep();

            if (m_ContinuationCalls >= MaxContinuationCalls)
                return Finish(false, "GTA did not enter the personal-garage selector");

            static Script::ScriptFunction giveVehicleReward(
                VehicleRewardHash,
                Script::ScriptPointer("GiveVehicleReward", "2D 0C 1E 00 00"));

            ++m_ContinuationCalls;
            const auto callResult = giveVehicleReward.TryCallOnThread<bool>(
                m_Thread,
                m_Vehicle,
                vehicleMenuData,
                &rewardData->transactionStatus,
                &rewardData->garage,
                &rewardData->garageOffset,
                &rewardData->controlStatus,
                false,
                true,
                true,
                false,
                std::int32_t{0},
                std::int32_t{-1});

            // nullopt means the VM call itself could not be made. A contained false
            // value is normal for this yielding script function and must continue.
            if (!callResult)
                return Finish(false, "GiveVehicleReward script function could not be invoked");

            LogState("continuation", *callResult, rewardData);

            if (rewardData->controlStatus == 3)
            {
                m_SelectorObserved = true;
                TUTONES_LOG_INFO("vehicle.savepv", "GTA personal-garage selector entered ControlStatus 3");
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueContinueStep();
            }

            // A true return before ControlStatus 3 is not treated as visible selector
            // completion. Give GTA a quiet interval and continue until the selector is
            // actually observed or the bounded startup budget expires.
            m_NextCallAt = now + (*callResult ? CompletedWithoutSelectorDelay : ContinuationDelay);

            if (RewardPreparationStatePresent(*rewardData))
                SetPending("GTA vehicle-reward flow is preparing the garage selector...");
            else
                SetPending("Starting GTA personal-garage selector...");

            QueueContinueStep();
        }

        [[nodiscard]] bool ValidateLiveFlow()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return false;

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Online session ended during garage save");
                return false;
            }

            if (Clock::now() >= m_Deadline)
            {
                Finish(false, "Personal-garage save flow timed out");
                return false;
            }

            const auto state = GameState::Get().Snapshot();
            if (!state.inVehicle || state.vehicle == 0 || state.vehicle != m_Vehicle)
            {
                Finish(false, "Stay in the vehicle until the garage save flow finishes");
                return false;
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
            {
                Finish(false, "Shared script runtime became unavailable");
                return false;
            }

            auto* liveReward = scripts.FindThread(VehicleRewardHash);
            if (!m_Thread || !m_Thread->stack || m_Thread->scriptHash != VehicleRewardHash)
            {
                Finish(false, "Cached vehicle-reward thread became invalid");
                return false;
            }

            if (liveReward != m_Thread)
            {
                TUTONES_LOG_WARN(
                    "vehicle.savepv",
                    "AM_MP_VEHICLE_REWARD thread identity changed during garage selector");
                Finish(false, "GTA replaced the vehicle-reward thread during garage save");
                return false;
            }

            return true;
        }

        [[nodiscard]] static bool RewardPreparationStatePresent(const Types::VehicleRewardData& data) noexcept
        {
            return data.transactionStatus != 0
                || data.garage != 0
                || data.garageOffset != 0;
        }

        void QueueContinueStep()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            // GameRuntime processes only the queue generation that existed when the
            // scheduler tick began, so this requeue is guaranteed to wait for a later
            // scheduler generation instead of recursively spinning in this callback.
            if (!Runtime::GameRuntime::Get().Enqueue([this] { ContinueRewardFlow(); }))
                Finish(false, "Lost game-thread scheduling during garage save");
        }

        void LogState(const char* phase, int callResult, const Types::VehicleRewardData* data)
        {
            if (!data)
                return;

            if (m_HaveLoggedState
                && callResult == m_LastCallResult
                && data->transactionStatus == m_LastTransactionStatus
                && data->garage == m_LastGarage
                && data->garageOffset == m_LastGarageOffset
                && data->controlStatus == m_LastControlStatus)
            {
                return;
            }

            m_HaveLoggedState = true;
            m_LastCallResult = callResult;
            m_LastTransactionStatus = data->transactionStatus;
            m_LastGarage = data->garage;
            m_LastGarageOffset = data->garageOffset;
            m_LastControlStatus = data->controlStatus;

            TUTONES_LOG_INFO(
                "vehicle.savepv",
                std::string(phase)
                    + ": callResult=" + std::to_string(callResult)
                    + " transaction=" + std::to_string(data->transactionStatus)
                    + " garage=" + std::to_string(data->garage)
                    + " garageOffset=" + std::to_string(data->garageOffset)
                    + " control=" + std::to_string(data->controlStatus)
                    + " continuation=" + std::to_string(m_ContinuationCalls));
        }

        void ResetFlowState() noexcept
        {
            m_Thread = nullptr;
            m_Vehicle = 0;
            m_Deadline = {};
            m_NextCallAt = {};
            m_SelectorObserved = false;
            m_ContinuationCalls = 0;
            m_HaveLoggedState = false;
            m_LastCallResult = -2;
            m_LastTransactionStatus = 0;
            m_LastGarage = 0;
            m_LastGarageOffset = 0;
            m_LastControlStatus = 0;
        }

        void Finish(bool success, std::string message)
        {
            TUTONES_LOG_INFO(
                "vehicle.savepv",
                std::string("Garage save finished: ") + (success ? "success - " : "failed - ") + message);

            ResetFlowState();
            m_Pending.store(false, std::memory_order_release);
            SetResult(success, std::move(message));
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void SetResult(bool success, std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = true;
            m_LastSucceeded = success;
            m_Message = std::move(message);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};

        Types::ScriptThread* m_Thread{};
        Vehicle m_Vehicle{};
        Clock::time_point m_Deadline{};
        Clock::time_point m_NextCallAt{};
        bool m_SelectorObserved{};
        int m_ContinuationCalls{};

        bool m_HaveLoggedState{};
        int m_LastCallResult{-2};
        std::int32_t m_LastTransactionStatus{};
        std::int32_t m_LastGarage{};
        std::int32_t m_LastGarageOffset{};
        std::int32_t m_LastControlStatus{};
    };
}
