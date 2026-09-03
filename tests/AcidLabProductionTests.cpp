#include "features/business/AcidLabProductionLogic.hpp"

#include <cassert>

using namespace Tutones::Game::Business::AcidLabProductionDetail;

int main()
{
    assert(FactoryArrayIndex(0) == PlayerFreemodeGlobal + 1
        + PropertyDataOffset + FactoryArrayOffset);
    assert(AcidFactoryEntryIndex(0) == FactoryArrayIndex(0) + 1
        + (AcidFactoryIndex * FactoryEntryStride));
    assert(AcidProductIndex(0) == AcidFactoryEntryIndex(0) + ProductOffset);
    assert(AcidProductIndex(31) == AcidProductIndex(0) + (31 * PlayerFreemodeStride));
    assert(AcidProductIndex(0) == 1845893);
    assert(AcidProductIndex(31) == 1873297);

    assert(IsValidStock(0));
    assert(IsValidStock(MaximumStockUnits));
    assert(!IsValidStock(-1));
    assert(!IsValidStock(MaximumStockUnits + 1));

    assert(CanMirrorLiveStock(FactoryArrayCount, AcidFactoryType, 0));
    assert(CanMirrorLiveStock(FactoryArrayCount, AcidFactoryType, MaximumStockUnits));
    assert(!CanMirrorLiveStock(FactoryArrayCount - 1, AcidFactoryType, 0));
    assert(!CanMirrorLiveStock(FactoryArrayCount, AcidFactoryType - 1, 0));
    assert(!CanMirrorLiveStock(FactoryArrayCount, AcidFactoryType, MaximumStockUnits + 1));
    return 0;
}
