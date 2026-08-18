# Vehicle Paint Integration

Prepared against the Tutones GTA Enhanced runtime architecture.

## Intended wiring

1. Add focused native wrappers for only:
   - GET_VEHICLE_COLOURS
   - SET_VEHICLE_COLOURS
   - GET_VEHICLE_EXTRA_COLOURS
   - SET_VEHICLE_EXTRA_COLOURS
2. Implement `IVehiclePaintBackend` with those wrappers.
3. Adapt the existing GTA game-thread queue to `IGameTaskQueue`.
4. Adapt the existing `GameState` current-vehicle snapshot to `ICurrentVehicleSource`.
5. Call `VehiclePaintService::Tick()` from the GTA script-thread runtime tick.
6. Render `VehiclePaintService::Snapshot()` from ImGui; never invoke GTA natives from the render thread.
7. UI click/Enter actions call only `QueuePrimary`, `QueueSecondary`, `QueuePearlescent`, or `QueueWheel`.

## Polling cadence

`Tick()` may run every GTA script tick, but paint natives do not. The service refreshes paint state immediately on vehicle acquisition/change and then at a 250 ms cadence. Successful writes also refresh immediately. This mirrors the existing `GameState` philosophy and avoids unnecessary native traffic.

## Paint behavior

Primary/secondary palettes use indexed GTA vehicle colours: Chrome, Classic, Matte, Metals, Utility, Worn, and Chameleon. Pearlescent and wheel colour use the extra-colour pair. Wheel colour accepts Alloy, Classic, and Chameleon indices.

Queued writes are vehicle-stable: if the player changes vehicles before a queued action runs, the operation is rejected rather than applied to the new vehicle.
