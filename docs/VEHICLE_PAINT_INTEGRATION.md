# Vehicle paint integration notes

Prepared against the Tutones GTA Enhanced runtime architecture.

## Final adapter responsibilities

1. Implement `IVehiclePaintBackend` on top of `Native::NativeRegistry` / `Native::NativeInvoker`.
2. Add only the vehicle paint natives required by the backend:
   - get/set indexed vehicle colours
   - get/set extra colours
   - test/get/set/clear custom primary colour
   - test/get/set/clear custom secondary colour
3. Implement `IGameTaskQueue` with `Runtime::GameRuntime::Get().Enqueue(std::function<void()>)`.
4. Implement `ICurrentVehicleSource` from `GameState::Get().Snapshot().vehicle`.
5. Call `VehiclePaintService::Tick()` from the GTA script-thread runtime tick.
6. Render `VehiclePaintService::Snapshot()` from ImGui; never invoke GTA natives from the render thread.
7. UI actions call only the queue methods on `VehiclePaintService`.

## Polling cadence

`Tick()` may run every GTA script tick, but paint natives do not. The service refreshes paint state immediately on vehicle acquisition/change and then at a 250 ms cadence. Successful writes also refresh immediately.

Custom RGB getter natives are only called when the corresponding `IS_*_COLOUR_CUSTOM` result says an override is active.

## Indexed/custom interaction

The stored indexed GTA colour remains the fallback behind a custom RGB override. Applying an indexed primary/secondary choice writes the indexed pair first and only then clears the matching custom override. This means a failed indexed write does not destroy a visible custom colour.

Explicit `QueueClearCustomPrimary()` / `QueueClearCustomSecondary()` operations remove only the RGB override and preserve the indexed fallback.

## Paint behavior

Primary/secondary palettes use indexed GTA vehicle colours: Chrome, Classic, Matte, Metals, Utility, Worn, and Chameleon. Pearlescent and wheel colour use the extra-colour pair. Wheel colour accepts Alloy, Classic, and Chameleon indices.
