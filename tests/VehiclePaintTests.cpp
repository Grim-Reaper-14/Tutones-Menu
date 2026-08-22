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
    int primaryPaintType{static_cast<int>(NativePaintType::Metallic)};
    int secondaryPaintType{static_cast<int>(NativePaintType::Normal)};
    int primaryModColor{27};
    int secondaryModColor{64};
    int primaryModPearlescent{111};
    bool primaryCustom{};
    bool secondaryCustom{};
    RgbColor customPrimary{12, 34, 56};
    RgbColor customSecondary{78, 90, 123};

    int setColourCalls{};
    int setExtraCalls{};
    int setMod1Calls{};
    int setMod2Calls{};
    int clearPrimaryCalls{};
    int clearSecondaryCalls{};

    bool failSetColours{};
    bool failSetExtra{};
    bool failSetCustomPrimary{};
    bool failSetCustomSecondary{};
    bool failClearPrimary{};
    bool failClearSecondary{};
    bool failRead{};
    bool ignoreSetColours{};
    bool ignoreSetExtra{};

    bool GetVehicleColours(VehicleHandle, int& outPrimary, int& outSecondary) noexcept override
    {
        if (failRead) return false;
        outPrimary = primary;
        outSecondary = secondary;
        return true;
    }

    bool SetVehicleColours(VehicleHandle, int inPrimary, int inSecondary) noexcept override
    {
        ++setColourCalls;
        if (failSetColours) return false;
        if (!ignoreSetColours)
        {
            primary = inPrimary;
            secondary = inSecondary;
        }
        return true;
    }

    bool GetVehicleExtraColours(VehicleHandle, int& outPearl, int& outWheel) noexcept override
    {
        if (failRead) return false;
        outPearl = pearl;
        outWheel = wheel;
        return true;
    }

    bool SetVehicleExtraColours(VehicleHandle, int inPearl, int inWheel) noexcept override
    {
        ++setExtraCalls;
        if (failSetExtra) return false;
        if (!ignoreSetExtra)
        {
            pearl = inPearl;
            wheel = inWheel;
        }
        return true;
    }

    bool GetVehicleModColor1(VehicleHandle, int& paintType, int& color, int& pearlescent) noexcept override
    {
        if (failRead) return false;
        paintType = primaryPaintType;
        color = primaryModColor;
        pearlescent = primaryModPearlescent;
        return true;
    }

    bool SetVehicleModColor1(VehicleHandle, int paintType, int color, int pearlescent) noexcept override
    {
        ++setMod1Calls;
        primaryPaintType = paintType;
        primaryModColor = color;
        primaryModPearlescent = pearlescent;
        return true;
    }

    bool GetVehicleModColor2(VehicleHandle, int& paintType, int& color) noexcept override
    {
        if (failRead) return false;
        paintType = secondaryPaintType;
        color = secondaryModColor;
        return true;
    }

    bool SetVehicleModColor2(VehicleHandle, int paintType, int color) noexcept override
    {
        ++setMod2Calls;
        secondaryPaintType = paintType;
        secondaryModColor = color;
        return true;
    }

    bool IsPrimaryColourCustom(VehicleHandle, bool& custom) noexcept override
    {
        if (failRead) return false;
        custom = primaryCustom;
        return true;
    }

    bool IsSecondaryColourCustom(VehicleHandle, bool& custom) noexcept override
    {
        if (failRead) return false;
        custom = secondaryCustom;
        return true;
    }

    bool GetCustomPrimaryColour(VehicleHandle, RgbColor& color) noexcept override
    {
        if (failRead) return false;
        color = customPrimary;
        return true;
    }

    bool GetCustomSecondaryColour(VehicleHandle, RgbColor& color) noexcept override
    {
        if (failRead) return false;
        color = customSecondary;
        return true;
    }

    bool SetCustomPrimaryColour(VehicleHandle, RgbColor color) noexcept override
    {
        if (failSetCustomPrimary) return false;
        primaryCustom = true;
        customPrimary = color;
        return true;
    }

    bool SetCustomSecondaryColour(VehicleHandle, RgbColor color) noexcept override
    {
        if (failSetCustomSecondary) return false;
        secondaryCustom = true;
        customSecondary = color;
        return true;
    }

    bool ClearCustomPrimaryColour(VehicleHandle) noexcept override
    {
        ++clearPrimaryCalls;
        if (failClearPrimary) return false;
        primaryCustom = false;
        return true;
    }

    bool ClearCustomSecondaryColour(VehicleHandle) noexcept override
    {
        ++clearSecondaryCalls;
        if (failClearSecondary) return false;
        secondaryCustom = false;
        return true;
    }
};

class FakeTaskQueue final : public IGameTaskQueue
{
public:
    std::deque<std::function<void()>> tasks;
    bool reject{};

    bool Enqueue(std::function<void()> task) override
    {
        if (reject) return false;
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
    [[nodiscard]] VehicleHandle CurrentVehicle() const noexcept override { return current; }
};

int main()
{
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Classic));
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Matte));
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Chrome));
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Worn));
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Chameleon));
    static_assert(!UsesIndexedVehicleColourPath(PaintPalette::Alloy));

    FakePaintBackend backend;
    FakeTaskQueue queue;
    FakeVehicleSource vehicles;
    VehiclePaintController controller(backend);
    VehiclePaintService service(controller, queue, vehicles);

    const auto t0 = VehiclePaintService::Clock::time_point{} + std::chrono::seconds{10};
    service.TickAt(t0);
    assert(!service.Snapshot().paint.valid);

    vehicles.current = 42;
    service.TickAt(t0);
    auto snapshot = service.Snapshot();
    assert(snapshot.paint.valid);
    assert(snapshot.paint.primaryColor == 27);
    assert(snapshot.paint.secondaryColor == 64);

    // All named LSC paint families use GTA's indexed primary/secondary pair, as Yim does.
    const int secondaryBeforeMatte = backend.secondary;
    assert(service.QueuePrimary({PaintPalette::Matte, 12}));
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(backend.primary == 12);
    assert(backend.secondary == secondaryBeforeMatte);
    assert(backend.setMod1Calls == 0);

    const int primaryBeforeChrome = backend.primary;
    assert(service.QueueSecondary({PaintPalette::Chrome, 120}));
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(backend.primary == primaryBeforeChrome);
    assert(backend.secondary == 120);
    assert(backend.setMod2Calls == 0);

    assert(service.QueuePrimary({PaintPalette::Worn, 21}));
    queue.RunOne();
    assert(backend.primary == 21);
    assert(service.QueueSecondary({PaintPalette::Chameleon, 220}));
    queue.RunOne();
    assert(backend.secondary == 220);

    // Indexed paint clears active custom overrides so the requested base color is visible.
    constexpr RgbColor pink{255, 32, 180};
    assert(service.QueueCustomPrimary(pink));
    queue.RunOne();
    assert(backend.primaryCustom && backend.customPrimary == pink);
    assert(service.Snapshot().lastOperationSucceeded);

    assert(service.QueuePrimary({PaintPalette::Classic, 28}));
    queue.RunOne();
    assert(!backend.primaryCustom);
    assert(backend.primary == 28);
    assert(service.Snapshot().lastOperationSucceeded);

    // Dispatch success without an actual GTA state change must fail verification.
    backend.ignoreSetColours = true;
    assert(service.QueuePrimary({PaintPalette::Classic, 29}));
    queue.RunOne();
    assert(!service.Snapshot().lastOperationSucceeded);
    assert(backend.primary == 28);
    backend.ignoreSetColours = false;

    // Extra colors preserve their companion and are verified after the write.
    assert(service.QueuePearlescent(70));
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(backend.pearl == 70 && backend.wheel == 156);
    assert(service.QueueWheel(221));
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(backend.pearl == 70 && backend.wheel == 221);

    backend.ignoreSetExtra = true;
    assert(service.QueueWheel(222));
    queue.RunOne();
    assert(!service.Snapshot().lastOperationSucceeded);
    assert(backend.wheel == 221);
    backend.ignoreSetExtra = false;

    // Custom colors are also verified by custom-enabled state and RGB read-back.
    constexpr RgbColor blue{20, 90, 255};
    assert(service.QueueCustomSecondary(blue));
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(backend.secondaryCustom && backend.customSecondary == blue);

    backend.failClearSecondary = true;
    assert(service.QueueClearCustomSecondary());
    queue.RunOne();
    assert(!service.Snapshot().lastOperationSucceeded);
    assert(backend.secondaryCustom);
    backend.failClearSecondary = false;

    assert(service.QueueClearCustomSecondary());
    queue.RunOne();
    assert(service.Snapshot().lastOperationSucceeded);
    assert(!backend.secondaryCustom);

    // Vehicle-stability guard still rejects queued paint if the player changes vehicles.
    assert(service.QueueCustomSecondary(blue));
    vehicles.current = 77;
    queue.RunOne();
    assert(service.Snapshot().lastOperationRejectedAsStale);
    vehicles.current = 42;

    // A transient read failure for the same vehicle keeps the last known editor state.
    backend.failRead = true;
    service.TickAt(t0 + std::chrono::seconds{1});
    assert(service.Snapshot().paint.valid);
    assert(service.Snapshot().paint.vehicle == 42);
    backend.failRead = false;

    // No vehicle rejects writes.
    vehicles.current = 0;
    service.TickAt(t0 + std::chrono::seconds{2});
    assert(!service.QueuePrimary({PaintPalette::Classic, 1}));
    assert(!service.QueueCustomPrimary(pink));

    std::cout << "Tutones vehicle paint verified indexed-path tests passed\n";
    return 0;
}
