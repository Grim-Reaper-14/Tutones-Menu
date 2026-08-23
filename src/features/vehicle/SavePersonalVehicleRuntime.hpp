#pragma once

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

            m_Thread = nullptr;
            m_SelectorObserved = false;
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
        static constexpr auto FlowTimeout = std::chrono::seconds(45);

        static constexpr std::array<std::uint32_t, 7> BlacklistedModels{
            SavePersonalVehicleDetail::Joaat("rcbandito"), SavePersonalVehicleDetail::Joaat("minitank"),
            SavePersonalVehicleDetail::Joaat("thruster"), SavePersonalVehicleDetail::Joaat("terbyte"),
            SavePersonalVehicleDetail::Joaat("avenger"), SavePersonalVehicleDetail::Joaat("policet3"),
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
            const auto valid = isVehicleValidForPv.TryCall<std::int32_t>(*model);
            if (!valid || *valid == 0)
                return Finish(false, "GTA rejected this model as a personal vehicle");

            // YimMenuV2 first reuses AM_MP_VEHICLE_REWARD when Freemode already owns it.
            // Tutones does not yet start arbitrary GTA scripts, so bind the live reward
            // thread and keep that identity for the complete selector transaction.
            m_Thread = scripts.FindThread(VehicleRewardHash);
            if (!m_Thread || !m_Thread->stack)
                return Finish(false, "GTA vehicle-reward script is not active; try again in Freemode");

            m_Vehicle = state.vehicle;
            m_Deadline = Clock::now() + FlowTimeout;
            m_SelectorObserved = false;
            SetPending("Opening GTA personal-garage selector...");

            // Match Yim's yielding state machine: do not spin the reward function here.
            // Queue the first reward call so it executes on the following scheduler tick.
            QueueNextRewardStep();
        }

        void RunRewardStep()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
                return Finish(false, "Online session ended during garage save");
            if (Clock::now() >= m_Deadline)
                return Finish(false, "Personal-garage save flow timed out");

            const auto state = GameState::Get().Snapshot();
            if (!state.inVehicle || state.vehicle == 0 || state.vehicle != m_Vehicle)
                return Finish(false, "Stay in the vehicle until the garage save flow finishes");

            auto& scripts = Script::ScriptRuntime::Get();
            auto* reward = scripts.FindThread(VehicleRewardHash);
            if (!reward || !reward->stack)
                return Finish(false, "GTA vehicle-reward script became unavailable");

            // If Freemode recycled the thread object during the flow, rebind before
            // touching locals. This keeps all pointers from the same live reward stack.
            if (reward != m_Thread)
                m_Thread = reward;

            auto* rewardData = Types::VehicleRewardData::Get(m_Thread);
            auto* vehicleMenuData = Script::ScriptLocal(m_Thread, 195).As<int>();
            if (!rewardData || !vehicleMenuData)
                return Finish(false, "Vehicle-reward script locals are unavailable");

            static Script::ScriptFunction giveVehicleReward(
                VehicleRewardHash,
                Script::ScriptPointer("GiveVehicleReward", "2D 0C 1E 00 00"));

            // Keep YimMenuV2's exact argument order and boolean semantics.
            const auto callResult = giveVehicleReward.TryCall<std::int32_t>(
                m_Vehicle,
                vehicleMenuData,
                &rewardData->transactionStatus,
                &rewardData->garage,
                &rewardData->garageOffset,
                &rewardData->controlStatus,
                std::int32_t{0},
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{0},
                0,
                -1);

            if (!callResult)
                return Finish(false, "GiveVehicleReward script function could not be invoked");

            // Yim only inspects ControlStatus after GiveVehicleReward returns true.
            // False means GTA is still servicing the selector; leave every local alone.
            if (*callResult == 0)
            {
                SetPending(m_SelectorObserved
                    ? "GTA garage selector active - choose a garage and slot"
                    : "Waiting for GTA garage selector...");
                return QueueNextRewardStep();
            }

            if (rewardData->controlStatus == 3)
            {
                m_SelectorObserved = true;
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueNextRewardStep();
            }

            // This is Yim's completion path. It intentionally treats both a successful
            // save and backing out of GTA's selector as completion of the script flow.
            rewardData->transactionStatus = 0;
            rewardData->garage = 0;
            rewardData->garageOffset = 0;
            rewardData->controlStatus = 0;
            Finish(true, "GTA personal-garage selector completed");
        }

        void QueueNextRewardStep()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { RunRewardStep(); }))
                Finish(false, "Lost game-thread scheduling during garage save");
        }

        void Finish(bool success, std::string message)
        {
            m_Thread = nullptr;
            m_Vehicle = 0;
            m_Deadline = {};
            m_SelectorObserved = false;
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
        bool m_SelectorObserved{};
    };
}
