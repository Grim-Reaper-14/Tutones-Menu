#include "../src/game/vehicle/VehiclePaintController.hpp"

#include <cassert>
#include <iostream>

using namespace Tutones::Game::Paint;

class FakePaintBackend final : public IVehiclePaintBackend
{
public:
    int primary{27};
    int secondary{64};
    bool failRead{};
    bool failWrite{};

    bool GetVehicleColours(VehicleHandle, int& outPrimary, int& outSecondary) noexcept override
    {
        if (failRead)
            return false;
        outPrimary = primary;
        outSecondary = secondary;
        return true;
    }

    bool SetVehicleColours(VehicleHandle, int inPrimary, int inSecondary) noexcept override
    {
        if (failWrite)
            return false;
        primary = inPrimary;
        secondary = inSecondary;
        return true;
    }
};

int main()
{
    FakePaintBackend backend;
    VehiclePaintController controller(backend);

    assert(controller.SetPrimary(42, {28}));
    assert(backend.primary == 28);
    assert(backend.secondary == 64);

    assert(controller.SetSecondary(42, {12}));
    assert(backend.primary == 28);
    assert(backend.secondary == 12);

    assert(!controller.SetPrimary(0, {29}));
    assert(!controller.SetPrimary(42, {999}));

    backend.failRead = true;
    assert(!controller.SetSecondary(42, {30}));

    std::cout << "Tutones vehicle paint v1 tests passed\n";
    return 0;
}
