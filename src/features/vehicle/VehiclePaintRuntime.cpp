#include "VehiclePaintRuntime.hpp"

#include "../../game/GameState.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <utility>

namespace Tutones::Game::Paint
{
    bool GameTaskQueueAdapter::Enqueue(std::function<void()> task)
    {
        return Runtime::GameRuntime::Get().Enqueue(std::move(task));
    }

    VehicleHandle CurrentVehicleSource::CurrentVehicle() const noexcept
    {
        const auto snapshot = GameState::Get().Snapshot();
        if (!snapshot.nativeRuntimeReady || !snapshot.inVehicle || snapshot.vehicle == 0)
            return 0;
        return snapshot.vehicle;
    }

    VehiclePaintRuntime::VehiclePaintRuntime() noexcept
        : m_Controller(m_Backend),
          m_Service(m_Controller, m_TaskQueue, m_VehicleSource)
    {
    }

    VehiclePaintRuntime& VehiclePaintRuntime::Get() noexcept
    {
        static VehiclePaintRuntime instance;
        return instance;
    }

    bool VehiclePaintRuntime::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        if (QueueNextTick())
            return true;

        m_Running.store(false, std::memory_order_release);
        return false;
    }

    void VehiclePaintRuntime::Stop() noexcept
    {
        m_Running.store(false, std::memory_order_release);
    }

    bool VehiclePaintRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    VehiclePaintService& VehiclePaintRuntime::Service() noexcept
    {
        return m_Service;
    }

    PaintServiceSnapshot VehiclePaintRuntime::Snapshot() const noexcept
    {
        return m_Service.Snapshot();
    }

    bool VehiclePaintRuntime::QueueNextTick()
    {
        if (!IsRunning())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([this] {
            TickOnGameThread();
        });
    }

    void VehiclePaintRuntime::TickOnGameThread() noexcept
    {
        if (!IsRunning())
            return;

        m_Service.Tick();
        if (IsRunning() && !QueueNextTick())
            m_Running.store(false, std::memory_order_release);
    }
}
