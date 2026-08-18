#include "VehiclePaintService.hpp"

#include <utility>

namespace Tutones::Game::Paint
{
    void VehiclePaintService::Tick() noexcept
    {
        TickAt(Clock::now());
    }

    void VehiclePaintService::TickAt(Clock::time_point now) noexcept
    {
        const VehicleHandle vehicle = m_VehicleSource.CurrentVehicle();
        if (vehicle == 0)
        {
            m_LastVehicle = 0;
            m_NextRefresh = {};
            ClearPaintSnapshot();
            return;
        }

        // Refresh immediately when entering/switching vehicles. Otherwise poll at a
        // modest cadence instead of issuing multiple GTA native calls every script tick.
        if (vehicle != m_LastVehicle || m_NextRefresh == Clock::time_point{} || now >= m_NextRefresh)
        {
            m_LastVehicle = vehicle;
            static_cast<void>(Refresh(vehicle));
            m_NextRefresh = now + RefreshInterval;
        }
    }

    bool VehiclePaintService::QueuePrimary(PaintChoice choice)
    {
        if (!PaintChoiceAllowed(PaintTarget::Primary, choice))
            return false;

        return QueueOperation(PaintOperation::Primary, [this, choice](VehicleHandle vehicle) {
            return m_Controller.SetPrimary(vehicle, choice);
        });
    }

    bool VehiclePaintService::QueueSecondary(PaintChoice choice)
    {
        if (!PaintChoiceAllowed(PaintTarget::Secondary, choice))
            return false;

        return QueueOperation(PaintOperation::Secondary, [this, choice](VehicleHandle vehicle) {
            return m_Controller.SetSecondary(vehicle, choice);
        });
    }

    bool VehiclePaintService::QueuePearlescent(int colorIndex)
    {
        if (!PaintChoiceAllowed(PaintTarget::Pearlescent, {PaintPalette::Classic, colorIndex}))
            return false;

        return QueueOperation(PaintOperation::Pearlescent, [this, colorIndex](VehicleHandle vehicle) {
            return m_Controller.SetPearlescent(vehicle, colorIndex);
        });
    }

    bool VehiclePaintService::QueueWheel(int colorIndex)
    {
        PaintChoice wheelChoice{PaintPalette::Classic, colorIndex};
        if (colorIndex >= 161)
            wheelChoice.palette = PaintPalette::Chameleon;
        if (!PaintChoiceAllowed(PaintTarget::Wheel, wheelChoice))
            return false;

        return QueueOperation(PaintOperation::Wheel, [this, colorIndex](VehicleHandle vehicle) {
            return m_Controller.SetWheel(vehicle, colorIndex);
        });
    }

    bool VehiclePaintService::QueueCustomPrimary(RgbColor color)
    {
        return QueueOperation(PaintOperation::CustomPrimary, [this, color](VehicleHandle vehicle) {
            return m_Controller.SetCustomPrimary(vehicle, color);
        });
    }

    bool VehiclePaintService::QueueCustomSecondary(RgbColor color)
    {
        return QueueOperation(PaintOperation::CustomSecondary, [this, color](VehicleHandle vehicle) {
            return m_Controller.SetCustomSecondary(vehicle, color);
        });
    }

    bool VehiclePaintService::QueueClearCustomPrimary()
    {
        return QueueOperation(PaintOperation::ClearCustomPrimary, [this](VehicleHandle vehicle) {
            return m_Controller.ClearCustomPrimary(vehicle);
        });
    }

    bool VehiclePaintService::QueueClearCustomSecondary()
    {
        return QueueOperation(PaintOperation::ClearCustomSecondary, [this](VehicleHandle vehicle) {
            return m_Controller.ClearCustomSecondary(vehicle);
        });
    }

    PaintServiceSnapshot VehiclePaintService::Snapshot() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    bool VehiclePaintService::QueueOperation(
        PaintOperation operation,
        std::function<bool(VehicleHandle)> apply)
    {
        const VehicleHandle expectedVehicle = m_VehicleSource.CurrentVehicle();
        if (expectedVehicle == 0 || !apply)
            return false;

        return m_TaskQueue.Enqueue([
            this,
            expectedVehicle,
            operation,
            apply = std::move(apply)]() mutable {
            if (m_VehicleSource.CurrentVehicle() != expectedVehicle)
            {
                RecordOperation(operation, false, true);
                return;
            }

            const bool success = apply(expectedVehicle);
            if (success)
                static_cast<void>(Refresh(expectedVehicle));

            RecordOperation(operation, success, false);
        });
    }

    bool VehiclePaintService::Refresh(VehicleHandle vehicle) noexcept
    {
        VehiclePaintState state{};
        if (!m_Controller.ReadPaintState(vehicle, state))
        {
            // Keep the last known editor state for the same vehicle. A transient or
            // optional metadata read must not collapse the entire Paint UI.
            std::scoped_lock lock(m_Mutex);
            if (m_Snapshot.paint.vehicle != vehicle)
                m_Snapshot.paint = {};
            return false;
        }

        std::scoped_lock lock(m_Mutex);
        m_Snapshot.paint = state;
        return true;
    }

    void VehiclePaintService::RecordOperation(
        PaintOperation operation,
        bool success,
        bool stale) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.lastOperation = operation;
        m_Snapshot.lastOperationSucceeded = success;
        m_Snapshot.lastOperationRejectedAsStale = stale;
    }

    void VehiclePaintService::ClearPaintSnapshot() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot.paint = {};
    }
}
