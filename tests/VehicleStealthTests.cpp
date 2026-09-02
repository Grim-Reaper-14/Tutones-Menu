#include "features/vehicle/VehicleStealthLogic.hpp"

#include <cassert>

using namespace Tutones::Game::Mods::VehicleStealth;

int main()
{
    assert(HardwareForModel(AkulaModel) == Hardware::FoldingWings);
    assert(HardwareForModel(Annihilator2Model) == Hardware::FoldingWings);
    assert(HardwareForModel(RaijuModel) == Hardware::MissileBays);
    assert(HardwareForModel(0) == Hardware::Unsupported);

    assert(PhysicalStateMatches(false, true));
    assert(!PhysicalStateMatches(true, true));
    assert(PhysicalStateMatches(true, false));
    assert(!PhysicalStateMatches(false, false));

    assert(ScriptStateMatches(StealthBit, true));
    assert(!ScriptStateMatches(0, true));
    assert(ScriptStateMatches(0, false));
    assert(!ScriptStateMatches(StealthBit, false));

    assert(PlayerStealthFlagsIndex(0) == PlayerFreemodeGlobal + 1 + StealthFlagsOffset);
    assert(PlayerStealthFlagsIndex(31) == PlayerFreemodeGlobal + 1
        + (31 * PlayerFreemodeStride) + StealthFlagsOffset);
    return 0;
}
