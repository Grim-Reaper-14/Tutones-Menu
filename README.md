# Tutones Vehicle Paint Overlay v5

Tutones GTA Enhanced vehicle-paint runtime checkpoint.

V5 moves the tested paint logic onto the real Tutones native/runtime architecture while fixing finish routing from the earlier standalone checkpoints.

## Paint routing

- Native paint types use `GET/SET_VEHICLE_MOD_COLOR_1/2`:
  - 0 Normal
  - 1 Metallic
  - 2 Pearl
  - 3 Matte
  - 4 Metal
  - 5 Chrome
- Classic and Utility choices map to native Normal instead of inventing unsupported paint types.
- Worn and Chameleon stay on indexed `GET/SET_VEHICLE_COLOURS`.
- Pearlescent overlay and wheel colour stay on `GET/SET_VEHICLE_EXTRA_COLOURS`.
- Custom primary/secondary RGB use the dedicated custom-colour natives.

## Runtime integration

- `TutonesVehiclePaintBackend` adapts `IVehiclePaintBackend` to the focused `NativeInvoker` / `NativeRegistry`.
- `VehiclePaintRuntime` adapts `GameRuntime::Enqueue` and `GameState::Snapshot().vehicle`.
- The paint service self-schedules one task per GTA script tick; native calls never execute from the D3D12/ImGui render thread.
- The existing 250 ms passive paint refresh, immediate successful-write refresh, stale-vehicle rejection, and conservative failure behavior remain intact.

## Validation

The standalone controller/service tests compile under C++20 with `-Wall -Wextra -Werror -pedantic` and pass as `Tutones vehicle paint v5 tests passed`.

The next checkpoint is the interactive Vehicle / Paint UI and broader vehicle modification controls.
