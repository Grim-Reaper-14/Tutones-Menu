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
    bool primaryCustom{};
    bool secondaryCustom{};
    RgbColor customPrimary{12, 34, 56};
    RgbColor customSecondary{78, 90, 123};

    int getColourCalls{};
    int setColourCalls{};
    int getExtraCalls{};
    int setExtraCalls{};
    int isPrimaryCustomCalls{};
    int isSecondaryCustomCalls{};
    int getCustomPrimaryCalls{};
    int getCustomSecondaryCalls{};
    int setCustomPrimaryCalls{};
    int setCustomSecondaryCalls{};
    int clearCustomPrimaryCalls{};
    int clearCustomSecondaryCalls{};

    bool failGetColours{};
    bool failSetColours{};
    bool failGetExtra{};
    bool failSetExtra{};
    bool failIsPrimaryCustom{};
    bool failIsSecondaryCustom{};
    bool failGetCustomPrimary{};
    bool failGetCustomSecondary{};
    bool failSetCustomPrimary{};
    bool failSetCustomSecondary{};
    bool failClearCustomPrimary{};
    bool failClearCustomSecondary{};

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

    bool IsPrimaryColourCustom(VehicleHandle, bool& custom) noexcept override
    {
        ++isPrimaryCustomCalls;
        if (failIsPrimaryCustom)
            return false;
        custom = primaryCustom;
        return true;
    }

    bool IsSecondaryColourCustom(VehicleHandle, bool& custom) noexcept override
    {
        ++isSecondaryCustomCalls;
        if (failIsSecondaryCustom)
            return false;
        custom = secondaryCustom;
        return true;
    }

    bool GetCustomPrimaryColour(VehicleHandle, RgbColor& color) noexcept override
    {
        ++getCustomPrimaryCalls;
        if (failGetCustomPrimary)
            return false;
        color = customPrimary;
        return true;
    }

    bool GetCustomSecondaryColour(VehicleHandle, RgbColor& color) noexcept override
    {
        ++getCustomSecondaryCalls;
        if (failGetCustomSecondary)
            return false;
        color = customSecondary;
        return true;
    }

    bool SetCustomPrimaryColour(VehicleHandle, RgbColor color) noexcept override
    {
        ++setCustomPrimaryCalls;
        if (failSetCustomPrimary)
            return false;
        primaryCustom = true;
        customPrimary = color;
        return true;
    }

    bool SetCustomSecondaryColour(VehicleHandle, RgbColor color) noexcept override
    {
        ++setCustomSecondaryCalls;
        if (failSetCustomSecondary)
            return false;
        secondaryCustom = true;
        customSecondary = color;
        return true;
    }

    bool ClearCustomPrimaryColour(VehicleHandle) noexcept override
    {
        ++clearCustomPrimaryCalls;
        if (failClearCustomPrimary)
            return false;
        primaryCustom = false;
        return true;
    }

    bool ClearCustomSecondaryColour(VehicleHandle) noexcept override
    {
        ++clearCustomSecondaryCalls;
        if (failClearCustomSecondary)
            return false;
        secondaryCustom = false;
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
    static_assert(RgbColor{1, 2, 3} == RgbColor{1, 2, 3});

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
    assert(!snapshot.paint.primaryCustom);
    assert(!snapshot.paint.secondaryCustom);
    assert(backend.getColourCalls == 1);
    assert(backend.getExtraCalls == 1);
    assert(backend.isPrimaryCustomCalls == 1);
    assert(backend.isSecondaryCustomCalls == 1);
    assert(backend.getCustomPrimaryCalls == 0);
    assert(backend.getCustomSecondaryCalls == 0);

    // Passive polling is throttled; a script tick does not become a native-call storm.
    service.TickAt(t0 + std::chrono::milliseconds{100});
    assert(backend.getColourCalls == 1);
    service.TickAt(t0 + VehiclePaintService::RefreshInterval);
    assert(backend.getColourCalls == 2);

    // Switching vehicles bypasses the throttle and refreshes immediately.
    vehicles.current = 50;
    service.TickAt(t0 + std::chrono::milliseconds{251});
    snapshot = service.Snapshot();
    assert(snapshot.paint.vehicle == 50);
    assert(backend.getColourCalls == 3);
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

    // Chameleon primary/secondary are indexed colors on the same vehicle-colour path.
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

    // Custom RGB primary/secondary are first-class queued operations.
    constexpr RgbColor neonPink{255, 32, 180};
    constexpr RgbColor coldBlue{20, 90, 255};
    assert(service.QueueCustomPrimary(neonPink));
    assert(!backend.primaryCustom);
    queue.RunOne();
    assert(backend.primaryCustom);
    assert(backend.customPrimary == neonPink);
    snapshot = service.Snapshot();
    assert(snapshot.paint.primaryCustom);
    assert(snapshot.paint.customPrimary == neonPink);
    assert(snapshot.lastOperation == PaintOperation::CustomPrimary);

    assert(service.QueueCustomSecondary(coldBlue));
    queue.RunOne();
    assert(backend.secondaryCustom);
    assert(backend.customSecondary == coldBlue);
    snapshot = service.Snapshot();
    assert(snapshot.paint.secondaryCustom);
    assert(snapshot.paint.customSecondary == coldBlue);

    // Selecting an indexed paint writes the index first, then removes the custom override.
    const int clearsBeforeIndexedPrimary = backend.clearCustomPrimaryCalls;
    assert(service.QueuePrimary({PaintPalette::Classic, 28}));
    queue.RunOne();
    assert(backend.primary == 28);
    assert(!backend.primaryCustom);
    assert(backend.clearCustomPrimaryCalls == clearsBeforeIndexedPrimary + 1);

    const int clearsBeforeIndexedSecondary = backend.clearCustomSecondaryCalls;
    assert(service.QueueSecondary({PaintPalette::Matte, 12}));
    queue.RunOne();
    assert(backend.secondary == 12);
    assert(!backend.secondaryCustom);
    assert(backend.clearCustomSecondaryCalls == clearsBeforeIndexedSecondary + 1);

    // Explicit reset keeps the underlying indexed colour and only removes RGB override state.
    assert(service.QueueCustomPrimary(neonPink));
    queue.RunOne();
    const int indexedBeforeClear = backend.primary;
    assert(service.QueueClearCustomPrimary());
    queue.RunOne();
    assert(!backend.primaryCustom);
    assert(backend.primary == indexedBeforeClear);
    assert(service.Snapshot().lastOperation == PaintOperation::ClearCustomPrimary);

    assert(service.QueueCustomSecondary(coldBlue));
    queue.RunOne();
    const int indexedSecondaryBeforeClear = backend.secondary;
    assert(service.QueueClearCustomSecondary());
    queue.RunOne();
    assert(!backend.secondaryCustom);
    assert(backend.secondary == indexedSecondaryBeforeClear);

    // A failed indexed write never clears a currently visible custom override.
    assert(service.QueueCustomPrimary(neonPink));
    queue.RunOne();
    backend.failSetColours = true;
    const int clearsBeforeFailedIndexed = backend.clearCustomPrimaryCalls;
    assert(service.QueuePrimary({PaintPalette::Classic, 29}));
    queue.RunOne();
    assert(backend.primaryCustom);
    assert(backend.customPrimary == neonPink);
    assert(backend.clearCustomPrimaryCalls == clearsBeforeFailedIndexed);
    assert(!service.Snapshot().lastOperationSucceeded);
    backend.failSetColours = false;

    // If index storage succeeds but clearing the override fails, surface failure conservatively.
    backend.failClearCustomPrimary = true;
    assert(service.QueuePrimary({PaintPalette::Classic, 30}));
    queue.RunOne();
    assert(backend.primary == 30);
    assert(backend.primaryCustom);
    assert(!service.Snapshot().lastOperationSucceeded);
    backend.failClearCustomPrimary = false;

    // Restore to a known non-custom state before the remaining failure tests.
    assert(service.QueueClearCustomPrimary());
    queue.RunOne();

    // Reject invalid palette/index combinations before queueing.
    const auto queuedBeforeInvalid = queue.tasks.size();
    assert(!service.QueuePrimary({PaintPalette::Worn, 220}));
    assert(queue.tasks.size() == queuedBeforeInvalid);

    // Queue rejection is surfaced synchronously and does not execute anything.
    queue.rejectEnqueue = true;
    assert(!service.QueuePrimary({PaintPalette::Classic, 28}));
    assert(!service.QueueCustomPrimary(neonPink));
    assert(queue.tasks.empty());
    queue.rejectEnqueue = false;

    // A delayed click must not paint a newly-entered vehicle.
    assert(service.QueueCustomSecondary(coldBlue));
    vehicles.current = 77;
    const auto customBeforeStale = backend.customSecondary;
    const bool customFlagBeforeStale = backend.secondaryCustom;
    queue.RunOne();
    assert(backend.customSecondary == customBeforeStale);
    assert(backend.secondaryCustom == customFlagBeforeStale);
    snapshot = service.Snapshot();
    assert(snapshot.lastOperation == PaintOperation::CustomSecondary);
    assert(!snapshot.lastOperationSucceeded);
    assert(snapshot.lastOperationRejectedAsStale);

    // Read custom RGB values only when the game reports that override as active.
    vehicles.current = 42;
    backend.primaryCustom = true;
    backend.customPrimary = {1, 2, 3};
    backend.secondaryCustom = true;
    backend.customSecondary = {4, 5, 6};
    service.TickAt(t0 + std::chrono::seconds{1});
    snapshot = service.Snapshot();
    assert(snapshot.paint.primaryCustom);
    assert(snapshot.paint.secondaryCustom);
    assert((snapshot.paint.customPrimary == RgbColor{1, 2, 3}));
    assert((snapshot.paint.customSecondary == RgbColor{4, 5, 6}));
    assert(backend.getCustomPrimaryCalls > 0);
    assert(backend.getCustomSecondaryCalls > 0);

    // A custom-colour read failure invalidates the published snapshot.
    backend.failGetCustomPrimary = true;
    service.TickAt(t0 + std::chrono::seconds{2});
    assert(!service.Snapshot().paint.valid);
    backend.failGetCustomPrimary = false;
    backend.primaryCustom = false;
    backend.secondaryCustom = false;

    // General read failure invalidates the snapshot rather than exposing stale data.
    backend.failGetExtra = true;
    service.TickAt(t0 + std::chrono::seconds{3});
    assert(!service.Snapshot().paint.valid);
    backend.failGetExtra = false;

    // No vehicle: state clears and UI writes are rejected.
    vehicles.current = 0;
    service.TickAt(t0 + std::chrono::seconds{4});
    snapshot = service.Snapshot();
    assert(!snapshot.paint.valid);
    assert(!service.QueueSecondary({PaintPalette::Matte, 12}));
    assert(!service.QueueCustomPrimary(neonPink));
    assert(!service.QueueClearCustomSecondary());

    std::cout << "Tutones vehicle paint v4 tests passed\n";
    return 0;
}
