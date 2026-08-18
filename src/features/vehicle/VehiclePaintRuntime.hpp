#pragma once

#include "VehiclePaintService.hpp"
#include "../../game/vehicle/TutonesVehiclePaintBackend.hpp"

#include <atomic>
#include <functional>

namespace Tutones::Game::Paint
{
    class GameTaskQueueAdapter final : public IGameTaskQueue
    {
    public:
        bool Enqueue(std::function<void()> task) override;
    };

    class CurrentVehicleSource final : public ICurrentVehicleSource
    {
    public:
        [[nodiscard]] VehicleHandle CurrentVehicle() const noexcept override;
    };

    class VehiclePaintRuntime final
    {
    public:
        static VehiclePaintRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;

        [[nodiscard]] VehiclePaintService& Service() noexcept;
        [[nodiscard]] PaintServiceSnapshot Snapshot() const noexcept;

    private:
        VehiclePaintRuntime() noexcept;
        ~VehiclePaintRuntime() = default;
        VehiclePaintRuntime(const VehiclePaintRuntime&) = delete;
        VehiclePaintRuntime& operator=(const VehiclePaintRuntime&) = delete;

        bool QueueNextTick();
        void TickOnGameThread() noexcept;

        TutonesVehiclePaintBackend m_Backend;
        VehiclePaintController m_Controller;
        GameTaskQueueAdapter m_TaskQueue;
        CurrentVehicleSource m_VehicleSource;
        VehiclePaintService m_Service;
        std::atomic<bool> m_Running{false};
    };
}
