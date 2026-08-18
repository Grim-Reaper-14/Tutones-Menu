#include "../src/game/vehicle/VehiclePaintController.hpp"

#include <cassert>
#include <iostream>

using namespace Tutones::Game::Paint;

class FakePaintBackend final : public IVehiclePaintBackend
{
public:
    int primary{27};
    int secondary{64};
    int pearl{111};
    int wheel{156};
    bool failGetColours{};
    bool failSetColours{};
    bool failGetExtra{};
    bool failSetExtra{};

    bool GetVehicleColours(VehicleHandle, int& outPrimary, int& outSecondary) noexcept override
    {
        if (failGetColours)
            return false;
        outPrimary = primary;
        outSecondary = secondary;
        return true;
    }

    bool SetVehicleColours(VehicleHandle, int inPrimary, int inSecondary) noexcept override
    {
        if (failSetColours)
            return false;
        primary = inPrimary;
        secondary = inSecondary;
        return true;
    }

    bool GetVehicleExtraColours(VehicleHandle, int& outPearl, int& outWheel) noexcept override
    {
        if (failGetExtra)
            return false;
        outPearl = pearl;
        outWheel = wheel;
        return true;
    }

    bool SetVehicleExtraColours(VehicleHandle, int inPearl, int inWheel) noexcept override
    {
        if (failSetExtra)
            return false;
        pearl = inPearl;
        wheel = inWheel;
        return true;
    }
};

int main()
{
    static_assert(PaletteAllowed(PaintTarget::Primary, PaintPalette::Worn));
    static_assert(PaletteAllowed(PaintTarget::Wheel, PaintPalette::Chameleon));
    static_assert(!PaletteAllowed(PaintTarget::Pearlescent, PaintPalette::Chameleon));
    static_assert(!PaintChoiceAllowed(PaintTarget::Primary, {PaintPalette::Worn, 220}));
    static_assert(PaintChoiceAllowed(PaintTarget::Primary, {PaintPalette::Chameleon, 220}));

    FakePaintBackend backend;
    VehiclePaintController controller(backend);

    VehiclePaintState state{};
    assert(controller.ReadPaintState(42, state));
    assert(state.valid);
    assert(state.primaryColor == 27);
    assert(state.secondaryColor == 64);
    assert(state.pearlescentColor == 111);
    assert(state.wheelColor == 156);

    assert(controller.SetPrimary(42, {PaintPalette::Classic, 28}));
    assert(backend.primary == 28);
    assert(backend.secondary == 64);

    assert(controller.SetSecondary(42, {PaintPalette::Chameleon, 220}));
    assert(backend.primary == 28);
    assert(backend.secondary == 220);

    assert(controller.SetPearlescent(42, 70));
    assert(backend.pearl == 70);
    assert(backend.wheel == 156);

    assert(controller.SetWheel(42, 221));
    assert(backend.pearl == 70);
    assert(backend.wheel == 221);

    assert(!controller.SetPrimary(42, {PaintPalette::Worn, 220}));
    assert(!controller.SetPrimary(0, {PaintPalette::Classic, 29}));

    backend.failGetExtra = true;
    VehiclePaintState failedState{};
    assert(!controller.ReadPaintState(42, failedState));
    assert(!failedState.valid);

    std::cout << "Tutones vehicle paint v2 tests passed\n";
    return 0;
}
