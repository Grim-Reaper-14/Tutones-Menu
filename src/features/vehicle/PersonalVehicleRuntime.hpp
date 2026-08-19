#pragma once

#include "../../game/Natives.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Tutones::Game::PersonalVehicles
{
    enum class PersonalVehicleAction : std::uint8_t
    {
        None,
        Repair,
        Request,
    };

    struct PersonalVehicleEntry final
    {
        int id{-1};
        Hash model{};
        std::string displayName;
        std::string plate;
        std::string garage;
        bool destroyed{};
        bool insured{};
        bool impounded{};
    };

    struct PersonalVehicleSnapshot final
    {
        std::vector<PersonalVehicleEntry> vehicles;
        std::vector<std::string> garages;
        std::size_t sourceArraySize{};
        std::uint64_t revision{};
        int currentVehicleId{-1};
        int requestedVehicleId{-1};
        int lastActionVehicleId{-1};
        PersonalVehicleAction lastAction{PersonalVehicleAction::None};
        bool lastActionSucceeded{};
        bool actionPending{};
        bool running{};
        bool scriptGlobalsReady{};
        bool nativeReady{};
        bool sessionStarted{};
        bool requestSupported{};
    };

    class PersonalVehicleRuntime final
    {
    public:
        static PersonalVehicleRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] PersonalVehicleSnapshot Snapshot() const;
        bool QueueRepair(int vehicleId);
        bool QueueRequest(int vehicleId);

    private:
        using Clock = std::chrono::steady_clock;

        enum class RequestStage : std::uint8_t
        {
            Idle,
            WaitingForDespawn,
            WaitBeforeRequest,
            WaitBeforeLocalClear,
        };

        PersonalVehicleRuntime() = default;
        ~PersonalVehicleRuntime() = default;
        PersonalVehicleRuntime(const PersonalVehicleRuntime&) = delete;
        PersonalVehicleRuntime& operator=(const PersonalVehicleRuntime&) = delete;

        bool QueueNextTick();
        bool QueueAction(PersonalVehicleAction action, int vehicleId);
        void TickOnGameThread() noexcept;
        void ProcessActionsOnGameThread(Clock::time_point now) noexcept;
        bool BeginRequestOnGameThread(int vehicleId, Clock::time_point now) noexcept;
        void ContinueRequestOnGameThread(Clock::time_point now) noexcept;
        bool RepairVehicleOnGameThread(int vehicleId, bool requireRepairable) noexcept;
        void RecordAction(PersonalVehicleAction action, int vehicleId, bool success) noexcept;
        void RefreshOnGameThread() noexcept;
        void PublishUnavailable(bool globalsReady, bool nativeReady) noexcept;

        std::atomic<bool> m_Running{false};
        mutable std::mutex m_Mutex;
        PersonalVehicleSnapshot m_Snapshot;
        PersonalVehicleAction m_QueuedAction{PersonalVehicleAction::None};
        int m_QueuedVehicleId{-1};
        bool m_ActionBusy{};
        RequestStage m_RequestStage{RequestStage::Idle};
        int m_RequestVehicleId{-1};
        Clock::time_point m_RequestDeadline{};
        Clock::time_point m_RequestStageReady{};
        Clock::time_point m_NextRefresh{};
    };
}
