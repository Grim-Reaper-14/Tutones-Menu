# Vehicle Paint Integration

## V6 UI wiring

`Vehicle / Paint` is now a functional editor instead of a visual placeholder.

`TutonesMenu::RenderContent()` delegates the Paint subtab to `RenderVehiclePaintPanel()`. The panel reads only the thread-safe `VehiclePaintRuntime::Get().Snapshot()` value and sends mutations through `VehiclePaintRuntime::Get().Service().Queue...`.

No GTA native is invoked from the D3D12/ImGui render thread.

## Exposed controls

The editor contains four tabs:

- **Primary**: native/indexed finish selection, color index, custom RGB, clear custom.
- **Secondary**: native/indexed finish selection, color index, custom RGB, clear custom.
- **Extras**: pearlescent overlay and wheel color.
- **Status**: current vehicle handle, mod paint types/colors, indexed pair, pearl/wheel values, custom override state, and last operation outcome.

Chameleon controls are constrained to indices 161-223. Other primary/secondary finish selections are constrained to 0-160. Wheel color accepts 0-223, with 161-223 representing the Chameleon range.

## Runtime path

1. ImGui reads `VehiclePaintRuntime::Snapshot()`.
2. User actions call `VehiclePaintService::QueuePrimary`, `QueueSecondary`, `QueuePearlescent`, `QueueWheel`, `QueueCustomPrimary`, `QueueCustomSecondary`, `QueueClearCustomPrimary`, or `QueueClearCustomSecondary`.
3. `GameTaskQueueAdapter` forwards the work to `Runtime::GameRuntime::Get().Enqueue(...)`.
4. `VehiclePaintController` validates the target and routes the operation to the correct backend path.
5. `TutonesVehiclePaintBackend` invokes the focused native wrappers only on the GTA script thread.
6. Successful writes immediately refresh the published paint snapshot.

## Finish routing

- `SET/GET_VEHICLE_MOD_COLOR_1/2`: Normal, Metallic, Pearl, Matte, Metal, Chrome.
- Classic and Utility route through native Normal.
- `SET/GET_VEHICLE_COLOURS`: Worn and Chameleon indexed finishes.
- `SET/GET_VEHICLE_EXTRA_COLOURS`: pearlescent overlay and wheel color.

The UI does not guess a Worn state from a snapshot because GTA does not expose a separate Worn paint-type flag in this layer. Chameleon can be recognized when the indexed color is in the 161-223 range; otherwise the editor initializes from the native mod-color state and still allows Worn to be selected explicitly.
