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
    struct PersonalVehicleEntry final
    {
        int id{-1};
        Hash model{};
        std::string displayName;
        std::string plate;
        std::string garage;
    };

    struct PersonalVehicleSnapshot final
    {
        std::vector<PersonalVehicleEntry> vehicles;
        std::vector<std::string> garages;
        std::size_t sourceArraySize{};
        std::uint64_t revision{};
        bool running{};
        bool scriptGlobalsReady{};
        bool nativeReady{};
    };

    class PersonalVehicleRuntime final
    {
    public:
        static PersonalVehicleRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] PersonalVehicleSnapshot Snapshot() const;

    private:
        using Clock = std::chrono::steady_clock;

        PersonalVehicleRuntime() = default;
        ~PersonalVehicleRuntime() = default;
        PersonalVehicleRuntime(const PersonalVehicleRuntime&) = delete;
        PersonalVehicleRuntime& operator=(const PersonalVehicleRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void RefreshOnGameThread() noexcept;
        void PublishUnavailable(bool globalsReady, bool nativeReady) noexcept;

        std::atomic<bool> m_Running{false};
        mutable std::mutex m_Mutex;
        PersonalVehicleSnapshot m_Snapshot;
        Clock::time_point m_NextRefresh{};
    };
}
