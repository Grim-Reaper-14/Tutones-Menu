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

    // Clean vehicle-reward flow.
    //
    // Important difference from the previous implementation:
    // GiveVehicleReward is NOT reinvoked every scheduler tick. Re-entering the
    // script function continuously can reopen/reinitialize GTA's garage frontend
    // faster than the player can interact with it. We activate the flow once,
    // observe AM_MP_VEHICLE_REWARD's locals, and only retry after a quiet delay if
    // GTA never entered the selector state at all.
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
        static constexpr auto ActivationRetryDelay = std::chrono::milliseconds(750);
        static constexpr int MaxActivationAttempts = 3;

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

            // Clear stale values left by an earlier aborted attempt before beginning a
            // brand-new transaction. Do this once, never while GTA owns the selector.
            rewardData->transactionStatus = 0;
            rewardData->garage = 0;
            rewardData->garageOffset = 0;
            rewardData->controlStatus = 0;

            m_Vehicle = state.vehicle;
            m_Deadline = Clock::now() + FlowTimeout;

            TUTONES_LOG_INFO(
                "vehicle.savepv",
                std::string("Starting clean AM_MP_VEHICLE_REWARD flow; threadId=")
                    + std::to_string(m_Thread->context.threadId)
                    + " vehicle=" + std::to_string(m_Vehicle));

            SetPending("Opening GTA personal-garage selector...");
            ActivateSelector();
        }

        void ActivateSelector()
        {
            if (!ValidateLiveFlow())
                return;

            if (m_ActivationAttempts >= MaxActivationAttempts)
                return Finish(false, "GTA did not enter the personal-garage selector");

            auto* rewardData = Types::VehicleRewardData::Get(m_Thread);
            auto* vehicleMenuData = Script::ScriptLocal(m_Thread, 195).As<int>();
            if (!rewardData || !vehicleMenuData)
                return Finish(false, "Vehicle-reward script locals are unavailable");

            static Script::ScriptFunction giveVehicleReward(
                VehicleRewardHash,
                Script::ScriptPointer("GiveVehicleReward", "2D 0C 1E 00 00"));

            ++m_ActivationAttempts;
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

            if (!callResult)
                return Finish(false, "GiveVehicleReward script function could not be invoked");

            LogState("activation", *callResult, rewardData);

            // Latch ownership as soon as GTA exposes any transaction state. From this
            // point forward we only observe locals; we never re-enter GiveVehicleReward.
            if (SelectorStatePresent(*rewardData))
            {
                m_SelectorObserved = true;
                SetPending("GTA garage selector active - choose a garage and slot");
            }
            else
            {
                m_NextActivationAttempt = Clock::now() + ActivationRetryDelay;
                SetPending("Waiting for GTA garage selector...");
            }

            QueueObserveStep();
        }

        void ObserveSelector()
        {
            if (!ValidateLiveFlow())
                return;

            auto* rewardData = Types::VehicleRewardData::Get(m_Thread);
            if (!rewardData)
                return Finish(false, "Vehicle-reward script locals became unavailable");

            LogState("observe", -1, rewardData);

            if (rewardData->controlStatus == 3)
            {
                m_SelectorObserved = true;
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueObserveStep();
            }

            if (!m_SelectorObserved && SelectorStatePresent(*rewardData))
            {
                m_SelectorObserved = true;
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueObserveStep();
            }

            if (m_SelectorObserved)
            {
                // Once GTA has owned the selector, leaving ControlStatus 3 means the
                // player either confirmed a garage/slot or backed out. Match Yim's
                // cleanup contract, but do not call GiveVehicleReward again here.
                rewardData->transactionStatus = 0;
                rewardData->garage = 0;
                rewardData->garageOffset = 0;
                rewardData->controlStatus = 0;
                return Finish(true, "GTA personal-garage selector closed cleanly");
            }

            if (Clock::now() >= m_NextActivationAttempt)
            {
                if (m_ActivationAttempts >= MaxActivationAttempts)
                    return Finish(false, "GTA did not enter the personal-garage selector");

                // Retry only after a quiet interval and only when no selector/transaction
                // state was ever observed. This avoids the old rapid open/close loop.
                TUTONES_LOG_WARN(
                    "vehicle.savepv",
                    std::string("Selector did not start; controlled activation retry ")
                        + std::to_string(m_ActivationAttempts + 1)
                        + "/" + std::to_string(MaxActivationAttempts));
                return ActivateSelector();
            }

            QueueObserveStep();
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
                TUTONES_LOG_WARN("vehicle.savepv", "AM_MP_VEHICLE_REWARD thread identity changed during garage selector");
                Finish(false, "GTA replaced the vehicle-reward thread during garage save");
                return false;
            }

            return true;
        }

        [[nodiscard]] static bool SelectorStatePresent(const Types::VehicleRewardData& data) noexcept
        {
            return data.controlStatus == 3
                || data.transactionStatus != 0
                || data.garage != 0
                || data.garageOffset != 0;
        }

        void QueueObserveStep()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { ObserveSelector(); }))
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
                    + " activation=" + std::to_string(m_ActivationAttempts));
        }

        void ResetFlowState() noexcept
        {
            m_Thread = nullptr;
            m_Vehicle = 0;
            m_Deadline = {};
            m_NextActivationAttempt = {};
            m_SelectorObserved = false;
            m_ActivationAttempts = 0;
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
        Clock::time_point m_NextActivationAttempt{};
        bool m_SelectorObserved{};
        int m_ActivationAttempts{};

        bool m_HaveLoggedState{};
        int m_LastCallResult{-2};
        std::int32_t m_LastTransactionStatus{};
        std::int32_t m_LastGarage{};
        std::int32_t m_LastGarageOffset{};
        std::int32_t m_LastControlStatus{};
    };
}
