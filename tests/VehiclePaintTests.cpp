#include "../src/features/vehicle/VehiclePaintService.hpp"

#include <cassert>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <utility>

using namespace Tutones::Game::Paint;

class FakePaintBackend final : public IVehiclePaintBackend
{
public:
    int primary{27};
    int secondary{64};
    int pearl{111};
    int wheel{156};
    int getColourCalls{};
    int setColourCalls{};
    int getExtraCalls{};
    int setExtraCalls{};
    bool failGetColours{};
    bool failSetColours{};
    bool failGetExtra{};
    bool failSetExtra{};

    bool GetVehicleColours(VehicleHandle, int& outPrimary, int& outSecondary) noexcept override
    {
        ++getColourCalls;
        if (failGetColours)
            return false;
        outPrimary = primary;
        outSecondary = secondary;
        return true;
    }

    bool SetVehicleColours(VehicleHandle, int inPrimary, int inSecondary) noexcept override
    {
        ++setColourCalls;
        if (failSetColours)
            return false;
        primary = inPrimary;
        secondary = inSecondary;
        return true;
    }

    bool GetVehicleExtraColours(VehicleHandle, int& outPearl, int& outWheel) noexcept override
    {
        ++getExtraCalls;
        if (failGetExtra)
            return false;
        outPearl = pearl;
        outWheel = wheel;
        return true;
    }

    bool SetVehicleExtraColours(VehicleHandle, int inPearl, int inWheel) noexcept override
    {
        ++setExtraCalls;
        if (failSetExtra)
            return false;
        pearl = inPearl;
        wheel = inWheel;
        return true;
    }
};

class FakeTaskQueue final : public IGameTaskQueue
{
public:
    std::deque<std::function<void()>> tasks;
    bool rejectEnqueue{};

    bool Enqueue(std::function<void()> task) override
    {
        if (rejectEnqueue)
            return false;
        tasks.emplace_back(std::move(task));
        return true;
    }

    void RunOne()
    {
        assert(!tasks.empty());
        auto task = std::move(tasks.front());
        tasks.pop_front();
        task();
    }
};

class FakeVehicleSource final : public ICurrentVehicleSource
{
public:
    VehicleHandle current{};

    [[nodiscard]] VehicleHandle CurrentVehicle() const noexcept override
    {
        return current;
    }
};

int main()
{
    static_assert(PaletteAllowed(PaintTarget::Primary, PaintPalette::Worn));
    static_assert(PaletteAllowed(PaintTarget::Wheel, PaintPalette::Chameleon));
    static_assert(!PaletteAllowed(PaintTarget::Pearlescent, PaintPalette::Chameleon));

    FakePaintBackend backend;
    FakeTaskQueue queue;
    FakeVehicleSource vehicles;
    VehiclePaintController controller(backend);
    VehiclePaintService service(controller, queue, vehicles);

    const auto t0 = VehiclePaintService::Clock::time_point{} + std::chrono::seconds{10};

    // No vehicle starts invalid and does not touch the backend.
    service.TickAt(t0);
    assert(!service.Snapshot().paint.valid);
    assert(backend.getColourCalls == 0);

    // Entering a vehicle refreshes immediately.
    vehicles.current = 42;
    service.TickAt(t0);
    auto snapshot = service.Snapshot();
    assert(snapshot.paint.valid);
    assert(snapshot.paint.vehicle == 42);
    assert(snapshot.paint.primaryColor == 27);
    assert(snapshot.paint.secondaryColor == 64);
    assert(snapshot.paint.pearlescentColor == 111);
    assert(snapshot.paint.wheelColor == 156);
    assert(backend.getColourCalls == 1);
    assert(backend.getExtraCalls == 1);

    // Passive polling is throttled; a script tick does not become four native calls.
    service.TickAt(t0 + std::chrono::milliseconds{100});
    assert(backend.getColourCalls == 1);
    assert(backend.getExtraCalls == 1);
    service.TickAt(t0 + VehiclePaintService::RefreshInterval);
    assert(backend.getColourCalls == 2);
    assert(backend.getExtraCalls == 2);

    // Switching vehicles bypasses the throttle and refreshes immediately.
    vehicles.current = 50;
    service.TickAt(t0 + std::chrono::milliseconds{251});
    snapshot = service.Snapshot();
    assert(snapshot.paint.vehicle == 50);
    assert(backend.getColourCalls == 3);
    assert(backend.getExtraCalls == 3);
    vehicles.current = 42;
    service.TickAt(t0 + std::chrono::milliseconds{252});
    assert(service.Snapshot().paint.vehicle == 42);

    // Writes are queued and preserve companion indexed values.
    const int originalSecondary = backend.secondary;
    assert(service.QueuePrimary({PaintPalette::Classic, 21}));
    assert(backend.primary == 27);
    queue.RunOne();
    assert(backend.primary == 21);
    assert(backend.secondary == originalSecondary);
    snapshot = service.Snapshot();
    assert(snapshot.lastOperation == PaintOperation::Primary);
    assert(snapshot.lastOperationSucceeded);
    assert(!snapshot.lastOperationRejectedAsStale);
    assert(snapshot.paint.primaryColor == 21);

    // Chameleon primary/secondary are indexed colors on the same SET_VEHICLE_COLOURS path.
    assert(service.QueueSecondary({PaintPalette::Chameleon, 220}));
    queue.RunOne();
    assert(backend.primary == 21);
    assert(backend.secondary == 220);

    // Extra-colour writes preserve their companion value.
    assert(service.QueuePearlescent(70));
    queue.RunOne();
    assert(backend.pearl == 70);
    assert(backend.wheel == 156);
    assert(service.QueueWheel(221));
    queue.RunOne();
    assert(backend.pearl == 70);
    assert(backend.wheel == 221);

    // Reject invalid palette/index combinations before queueing.
    const auto queuedBeforeInvalid = queue.tasks.size();
    assert(!service.QueuePrimary({PaintPalette::Worn, 220}));
    assert(queue.tasks.size() == queuedBeforeInvalid);

    // Queue rejection is surfaced synchronously and does not execute anything.
    queue.rejectEnqueue = true;
    assert(!service.QueuePrimary({PaintPalette::Classic, 28}));
    assert(queue.tasks.empty());
    queue.rejectEnqueue = false;

    // A delayed click must not paint a newly-entered vehicle.
    assert(service.QueuePrimary({PaintPalette::Classic, 28}));
    vehicles.current = 77;
    const int primaryBeforeStale = backend.primary;
    queue.RunOne();
    assert(backend.primary == primaryBeforeStale);
    snapshot = service.Snapshot();
    assert(snapshot.lastOperation == PaintOperation::Primary);
    assert(!snapshot.lastOperationSucceeded);
    assert(snapshot.lastOperationRejectedAsStale);

    // Backend write failure is recorded and leaves the snapshot at the last good state.
    vehicles.current = 42;
    service.TickAt(t0 + std::chrono::seconds{1});
    const auto snapshotBeforeFailure = service.Snapshot();
    backend.failSetColours = true;
    assert(service.QueueSecondary({PaintPalette::Classic, 29}));
    queue.RunOne();
    snapshot = service.Snapshot();
    assert(snapshot.lastOperation == PaintOperation::Secondary);
    assert(!snapshot.lastOperationSucceeded);
    assert(!snapshot.lastOperationRejectedAsStale);
    assert(snapshot.paint.secondaryColor == snapshotBeforeFailure.paint.secondaryColor);
    backend.failSetColours = false;

    // Read failure invalidates the published paint snapshot rather than exposing stale data.
    backend.failGetExtra = true;
    service.TickAt(t0 + std::chrono::seconds{2});
    assert(!service.Snapshot().paint.valid);
    backend.failGetExtra = false;

    // No vehicle: state clears and UI writes are rejected.
    vehicles.current = 0;
    service.TickAt(t0 + std::chrono::seconds{3});
    snapshot = service.Snapshot();
    assert(!snapshot.paint.valid);
    assert(!service.QueueSecondary({PaintPalette::Matte, 12}));

    std::cout << "Tutones vehicle paint v3 tests passed\n";
    return 0;
}
