# Tutones Vehicle Paint Overlay v6

Tutones GTA Enhanced vehicle-paint runtime and interactive UI checkpoint.

V6 exposes the V5 paint runtime through the real Vehicle / Paint ImGui subtab while keeping every GTA native call on the GTA script-thread task queue.

## Interactive Vehicle / Paint editor

- Primary and secondary finish selection for Normal, Metallic, Pearl, Chrome, Classic, Matte, Metal, Utility, Worn, and Chameleon.
- Primary and secondary color-index controls with Chameleon constrained to indices 161-223.
- Custom primary and secondary RGB color editors with explicit clear-custom actions.
- Pearlescent overlay and wheel-color controls.
- Live status for the tracked vehicle, native paint types, indexed colors, custom-color state, and the last queued paint operation.
- Paint controls stay disabled until `VehiclePaintRuntime` has a valid vehicle snapshot.

## Threading contract

The ImGui panel only reads `VehiclePaintRuntime::Snapshot()` and calls `VehiclePaintService::Queue...` methods. It never invokes GTA natives directly from the D3D12/render thread.

Queued operations retain the V5 behavior:

- vehicle-stable execution,
- 250 ms passive paint refresh,
- immediate refresh after successful writes,
- conservative failure reporting,
- primary/secondary companion preservation,
- pearlescent/wheel companion preservation,
- custom RGB fallback preservation.

## Paint routing

- `GET/SET_VEHICLE_MOD_COLOR_1/2` for native paint types 0-5.
- `GET/SET_VEHICLE_COLOURS` for Worn and Chameleon indexed finishes.
- `GET/SET_VEHICLE_EXTRA_COLOURS` for pearlescent overlay and wheel color.
- Dedicated custom primary/secondary RGB natives for custom colors.

## Validation

The new UI translation unit and the updated menu translation unit pass a strict C++20 syntax build with `-Wall -Wextra -Werror -pedantic` against the V5 paint headers and an ImGui 1.89-compatible API surface.

The next vehicle checkpoint can expand Vehicle / Modifications into working wheels, engine, brakes, transmission, suspension, and related mod-kit controls.
