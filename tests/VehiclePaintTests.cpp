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
    int setMod1Calls{};
    int setMod2Calls{};
    int clearPrimaryCalls{};
    int clearSecondaryCalls{};

    bool failSetColours{};
    bool failSetMod1{};
    bool failSetMod2{};
    bool failClearPrimary{};
    bool failClearSecondary{};
    bool failRead{};

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
        primary = inPrimary;
        secondary = inSecondary;
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
        pearl = inPearl;
        wheel = inWheel;
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
        if (failSetMod1) return false;
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
        if (failSetMod2) return false;
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
        primaryCustom = true;
        customPrimary = color;
        return true;
    }

    bool SetCustomSecondaryColour(VehicleHandle, RgbColor color) noexcept override
    {
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
    static_assert(NativePaintTypeValue(PaintPalette::Normal) == 0);
    static_assert(NativePaintTypeValue(PaintPalette::Metallic) == 1);
    static_assert(NativePaintTypeValue(PaintPalette::Pearl) == 2);
    static_assert(NativePaintTypeValue(PaintPalette::Matte) == 3);
    static_assert(NativePaintTypeValue(PaintPalette::Metals) == 4);
    static_assert(NativePaintTypeValue(PaintPalette::Chrome) == 5);
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Worn));
    static_assert(UsesIndexedVehicleColourPath(PaintPalette::Chameleon));
    static_assert(!UsesIndexedVehicleColourPath(PaintPalette::Chrome));

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
    assert(snapshot.paint.primaryPaintType == NativePaintType::Metallic);
    assert(snapshot.paint.secondaryPaintType == NativePaintType::Normal);
    assert(snapshot.paint.primaryModColor == 27);
    assert(snapshot.paint.secondaryModColor == 64);

    // Native paint types 0-5 use SET_VEHICLE_MOD_COLOR_1/2, not indexed SET_VEHICLE_COLOURS.
    const int indexedBefore = backend.setColourCalls;
    assert(service.QueuePrimary({PaintPalette::Matte, 12}));
    queue.RunOne();
    assert(backend.setMod1Calls == 1);
    assert(backend.setColourCalls == indexedBefore);
    assert(backend.primaryPaintType == static_cast<int>(NativePaintType::Matte));
    assert(backend.primaryModColor == 12);
    assert(backend.primaryModPearlescent == 111);

    assert(service.QueueSecondary({PaintPalette::Chrome, 120}));
    queue.RunOne();
    assert(backend.setMod2Calls == 1);
    assert(backend.setColourCalls == indexedBefore);
    assert(backend.secondaryPaintType == static_cast<int>(NativePaintType::Chrome));
    assert(backend.secondaryModColor == 120);

    // Classic/Utility map to native Normal rather than fake paint types 6/7.
    assert(service.QueuePrimary({PaintPalette::Classic, 21}));
    queue.RunOne();
    assert(backend.primaryPaintType == static_cast<int>(NativePaintType::Normal));
    assert(service.QueueSecondary({PaintPalette::Utility, 20}));
    queue.RunOne();
    assert(backend.secondaryPaintType == static_cast<int>(NativePaintType::Normal));

    // Worn and Chameleon remain on indexed vehicle colours and preserve the companion value.
    const int secondaryBeforeWorn = backend.secondary;
    const int mod1BeforeWorn = backend.setMod1Calls;
    assert(service.QueuePrimary({PaintPalette::Worn, 21}));
    queue.RunOne();
    assert(backend.primary == 21);
    assert(backend.secondary == secondaryBeforeWorn);
    assert(backend.setMod1Calls == mod1BeforeWorn);

    const int primaryBeforeChameleon = backend.primary;
    const int mod2BeforeChameleon = backend.setMod2Calls;
    assert(service.QueueSecondary({PaintPalette::Chameleon, 220}));
    queue.RunOne();
    assert(backend.primary == primaryBeforeChameleon);
    assert(backend.secondary == 220);
    assert(backend.setMod2Calls == mod2BeforeChameleon);

    // Extra colours preserve their companion.
    assert(service.QueuePearlescent(70));
    queue.RunOne();
    assert(backend.pearl == 70 && backend.wheel == 156);
    assert(service.QueueWheel(221));
    queue.RunOne();
    assert(backend.pearl == 70 && backend.wheel == 221);

    // Custom RGB stays vehicle-stable and indexed/mod writes clear it only after success.
    constexpr RgbColor pink{255, 32, 180};
    assert(service.QueueCustomPrimary(pink));
    queue.RunOne();
    assert(backend.primaryCustom && backend.customPrimary == pink);

    backend.failSetMod1 = true;
    const int clearsBeforeFailure = backend.clearPrimaryCalls;
    assert(service.QueuePrimary({PaintPalette::Metallic, 28}));
    queue.RunOne();
    assert(backend.primaryCustom);
    assert(backend.clearPrimaryCalls == clearsBeforeFailure);
    assert(!service.Snapshot().lastOperationSucceeded);
    backend.failSetMod1 = false;

    assert(service.QueuePrimary({PaintPalette::Metallic, 28}));
    queue.RunOne();
    assert(!backend.primaryCustom);
    assert(backend.primaryPaintType == static_cast<int>(NativePaintType::Metallic));
    assert(backend.primaryModColor == 28);

    assert(service.QueueCustomSecondary({20, 90, 255}));
    vehicles.current = 77;
    queue.RunOne();
    assert(service.Snapshot().lastOperationRejectedAsStale);
    vehicles.current = 42;

    // Read/backend failures invalidate the snapshot conservatively.
    backend.failRead = true;
    service.TickAt(t0 + std::chrono::seconds{1});
    assert(!service.Snapshot().paint.valid);
    backend.failRead = false;

    // No vehicle rejects writes.
    vehicles.current = 0;
    service.TickAt(t0 + std::chrono::seconds{2});
    assert(!service.QueuePrimary({PaintPalette::Normal, 1}));
    assert(!service.QueueCustomPrimary(pink));

    std::cout << "Tutones vehicle paint v5 tests passed\n";
    return 0;
}
