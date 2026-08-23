#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/GameState.hpp"
#include "../../game/Natives.hpp"
#include "../../game/script/ScriptFunction.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptLocal.hpp"
#include "../../game/script/ScriptPointer.hpp"
#include "../../game/script/ScriptRuntime.hpp"
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
                if (*model == blocked)
                    return Finish(false, "This vehicle is blocked from personal-garage acquisition");

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
            auto* reward = scripts.FindThread(VehicleRewardHash);
            if (!freemode || !freemode->stack)
                return Finish(false, "Freemode script is unavailable");
            if (!reward || !reward->stack)
                return Finish(false, "GTA vehicle-reward script is not active; try again in Freemode");

            static Script::ScriptFunction isVehicleValidForPv(
                FreemodeHash,
                Script::ScriptPointer("IsVehicleValidForPV", "5D ? ? ? 2A 06 56 13 00 38 00").Add(1).Rip());
            const auto valid = isVehicleValidForPv.TryCall<std::int32_t>(*model);
            if (!valid || *valid == 0)
                return Finish(false, "GTA rejected this model as a personal vehicle");

            m_Vehicle = state.vehicle;
            m_Deadline = Clock::now() + FlowTimeout;
            m_SelectorObserved = false;
            SetPending("Opening GTA personal-garage selector...");
            RunRewardStep();
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

            auto* transactionStatus = Script::ScriptLocal(reward, 151).As<int>();
            auto* garage = Script::ScriptLocal(reward, 152).As<int>();
            auto* garageOffset = Script::ScriptLocal(reward, 153).As<int>();
            auto* controlStatus = Script::ScriptLocal(reward, 154).As<int>();
            auto* vehicleMenuData = Script::ScriptLocal(reward, 195).As<int>();
            if (!transactionStatus || !garage || !garageOffset || !controlStatus || !vehicleMenuData)
                return Finish(false, "Vehicle-reward script locals are unavailable");

            static Script::ScriptFunction giveVehicleReward(
                VehicleRewardHash,
                Script::ScriptPointer("GiveVehicleReward", "2D 0C 1E 00 00"));

            // Match YimMenuV2's current Enhanced flow: GiveVehicleReward is invoked
            // again on every script tick while controlStatus == 3. GTA's reward script
            // owns the selector lifecycle; repeatedly servicing it keeps the garage
            // selector alive instead of letting it flash for one frame and disappear.
            const auto called = giveVehicleReward.TryCall<std::int32_t>(
                m_Vehicle,
                vehicleMenuData,
                transactionStatus,
                garage,
                garageOffset,
                controlStatus,
                std::int32_t{0},
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{0},
                0,
                -1);

            if (!called)
                return Finish(false, "GiveVehicleReward script function could not be invoked");

            if (*controlStatus == 3)
            {
                m_SelectorObserved = true;
                SetPending("GTA garage selector active - choose a garage and slot");
                return QueueNextRewardStep();
            }

            // Before the selector reaches status 3, keep servicing the reward function
            // for a few ticks just as Yim's fiber loop does.
            if (!m_SelectorObserved
                && *transactionStatus == 0
                && *garage == 0
                && *garageOffset == 0)
            {
                SetPending("Waiting for GTA garage selector...");
                return QueueNextRewardStep();
            }

            const bool accepted = *transactionStatus != 0 || *garage != 0 || *garageOffset != 0;
            *transactionStatus = 0;
            *garage = 0;
            *garageOffset = 0;
            *controlStatus = 0;
            Finish(accepted, accepted
                ? "Personal-garage save flow completed"
                : "Garage selector closed without saving the vehicle");
        }

        void QueueNextRewardStep()
        {
            if (!Runtime::GameRuntime::Get().Enqueue([this] { RunRewardStep(); }))
                Finish(false, "Lost game-thread scheduling during garage save");
        }

        void Finish(bool success, std::string message)
        {
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
        Vehicle m_Vehicle{};
        Clock::time_point m_Deadline{};
        bool m_SelectorObserved{};
    };
}
