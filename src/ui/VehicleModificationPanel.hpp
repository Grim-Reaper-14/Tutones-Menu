#pragma once

namespace Tutones::UI
{
    void RenderVehicleModificationPanel() noexcept;
    void RenderLegacyVehicleModificationPanel() noexcept;
}

#if defined(TUTONES_BUILD_LEGACY_VEHICLE_EDITOR)
#define RenderVehicleModificationPanel RenderLegacyVehicleModificationPanel
#endif
