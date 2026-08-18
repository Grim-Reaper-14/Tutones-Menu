# Tutones Vehicle Paint Overlay v1

Repo-shaped first pass for vehicle paint support in Tutones-Menu.

V1 deliberately stays small:

- Read the current indexed primary and secondary vehicle colours.
- Change one indexed colour while preserving the companion colour.
- Keep GTA/native details behind an `IVehiclePaintBackend` interface.
- Keep the controller independent from ImGui so the UI can be wired later.

The real Tutones repo already uses `Tutones::Game::Vehicle` as the vehicle handle type, so this reconstruction uses `Tutones::Game::Paint` as its namespace to avoid a name collision.
