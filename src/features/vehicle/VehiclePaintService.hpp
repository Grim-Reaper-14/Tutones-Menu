#pragma once

#include "../../game/vehicle/VehiclePaintController.hpp"

#include <chrono>
#include <functional>
#include <mutex>

namespace Tutones::Game::Paint
{
    class IGameTaskQueue
    {
    public:
        virtual ~IGameTaskQueue() = default;
        virtual bool Enqueue(std::function<void()> task) = 0;
    };

    class ICurrentVehicleSource
    {
    public:
        virtual ~ICurrentVehicleSource() = default;
        [[nodiscard]] virtual VehicleHandle CurrentVehicle() const noexcept = 0;
    };

    class VehiclePaintService final
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr auto RefreshInterval = std::chrono::milliseconds{250};

        VehiclePaintService(
            VehiclePaintController& controller,
            IGameTaskQueue& taskQueue,
            ICurrentVehicleSource& vehicleSource) noexcept
            : m_Controller(controller),
              m_TaskQueue(taskQueue),
              m_VehicleSource(vehicleSource)
        {
        }

        void Tick() noexcept;
        void TickAt(Clock::time_point now) noexcept;

        [[nodiscard]] bool QueuePrimary(PaintChoice choice);
        [[nodiscard]] bool QueueSecondary(PaintChoice choice);
        [[nodiscard]] bool QueuePearlescent(int colorIndex);
        [[nodiscard]] bool QueueWheel(int colorIndex);

        [[nodiscard]] PaintServiceSnapshot Snapshot() const noexcept;

    private:
        bool QueueOperation(PaintOperation operation, std::function<bool(VehicleHandle)> apply);
        bool Refresh(VehicleHandle vehicle) noexcept;
        void RecordOperation(PaintOperation operation, bool success, bool stale) noexcept;
        void ClearPaintSnapshot() noexcept;

        VehiclePaintController& m_Controller;
        IGameTaskQueue& m_TaskQueue;
        ICurrentVehicleSource& m_VehicleSource;

        mutable std::mutex m_Mutex;
        PaintServiceSnapshot m_Snapshot{};
        VehicleHandle m_LastVehicle{};
        Clock::time_point m_NextRefresh{};
    };
}
