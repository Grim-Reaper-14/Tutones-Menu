#pragma once

#include "../../game/Natives.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

namespace Tutones::Game::Mods
{
    enum class VehicleModAction : unsigned char
    {
        None,
        Repair,
        Clean,
        FlipUpright,
        SetMod,
        RemoveMod,
        ToggleMod,
        SetWheelType,
    };

    struct VehicleModificationSnapshot final
    {
        Vehicle vehicle{};
        int observedModType{};
        int modCount{};
        int currentMod{-1};
        bool customTires{};
        int wheelType{};
        bool turbo{};
        bool tireSmoke{};
        bool xenon{};
        bool valid{};

        VehicleModAction lastAction{VehicleModAction::None};
        bool lastActionSucceeded{};
        bool lastActionRejectedAsStale{};
    };

    class VehicleModificationRuntime final
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr auto RefreshInterval = std::chrono::milliseconds{250};

        static VehicleModificationRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;

        void SetObservedModType(int modType) noexcept;
        [[nodiscard]] VehicleModificationSnapshot Snapshot() const noexcept;

        [[nodiscard]] bool QueueRepair();
        [[nodiscard]] bool QueueClean();
        [[nodiscard]] bool QueueFlipUpright();
        [[nodiscard]] bool QueueSetMod(int modType, int modIndex, bool customTires);
        [[nodiscard]] bool QueueRemoveMod(int modType);
        [[nodiscard]] bool QueueToggleMod(int modType, bool enabled);
        [[nodiscard]] bool QueueWheelType(int wheelType);

    private:
        VehicleModificationRuntime() = default;
        ~VehicleModificationRuntime() = default;
        VehicleModificationRuntime(const VehicleModificationRuntime&) = delete;
        VehicleModificationRuntime& operator=(const VehicleModificationRuntime&) = delete;

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept;
        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        bool Refresh(Vehicle vehicle) noexcept;
        bool QueueVehicleOperation(VehicleModAction action, std::function<bool(Vehicle)> apply);
        void RecordAction(VehicleModAction action, bool success, bool stale) noexcept;
        void ClearSnapshot() noexcept;

        std::atomic<bool> m_Running{false};
        std::atomic<int> m_ObservedModType{11};
        Vehicle m_LastVehicle{};
        int m_LastObservedModType{-1};
        Clock::time_point m_NextRefresh{};

        mutable std::mutex m_Mutex;
        VehicleModificationSnapshot m_Snapshot{};
    };
}
