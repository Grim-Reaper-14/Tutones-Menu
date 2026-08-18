# Vehicle Paint Integration

## V5 runtime wiring

The standalone paint service now has a concrete Tutones adapter path:

1. `NativeRegistry` resolves the focused vehicle-paint native set.
2. `Natives.hpp` exposes typed wrappers for indexed, mod-colour, extra-colour, and custom-RGB operations.
3. `TutonesVehiclePaintBackend` implements `IVehiclePaintBackend` with those wrappers.
4. `GameTaskQueueAdapter` forwards paint work to `Runtime::GameRuntime::Get().Enqueue(...)`.
5. `CurrentVehicleSource` reads the thread-safe `GameState::Get().Snapshot().vehicle` snapshot.
6. `VehiclePaintRuntime` self-schedules a service tick on the GTA task queue after `GameRuntime` starts and stops before it shuts down.

No GTA native is invoked from the D3D12/ImGui render thread.

## Finish routing

Primary/secondary finishes are split into the two GTA-native paths instead of treating every finish as a raw indexed colour:

- `SET/GET_VEHICLE_MOD_COLOR_1/2` for native paint types 0 through 5: Normal, Metallic, Pearl, Matte, Metal, and Chrome.
- Classic and Utility are routed through native Normal.
- `SET/GET_VEHICLE_COLOURS` for Worn and Chameleon indexed finishes.
- `SET/GET_VEHICLE_EXTRA_COLOURS` for pearlescent overlay and wheel colour.

Primary mod-colour writes read the existing primary mod colour first so the `SET_VEHICLE_MOD_COLOR_1` pearlescent companion argument is preserved.

## Custom RGB interaction

The indexed/mod paint is written before a custom RGB override is cleared. A failed base paint write therefore never destroys the visible custom override. If the base write succeeds but clearing the override fails, the operation reports failure conservatively.

Custom RGB getters only run when `GET_IS_VEHICLE_*_COLOUR_CUSTOM` reports that the matching override is active.

## Remaining UI work

The current Vehicle / Paint panel is still a visual placeholder. The next checkpoint should render `VehiclePaintRuntime::Get().Snapshot()` and route UI actions only through `VehiclePaintRuntime::Get().Service().Queue...` methods.
