# Tutones Menu V2 Base

Tutones Menu V2 is a fresh x64 DLL foundation that keeps the strongest architectural ideas from Tutones Menu V1 while enforcing a simpler three-layer split:

- **Core** owns lifecycle, logging, configuration, filesystem, threading, and shared services.
- **Backend** owns GTA-facing runtime services, hooks, natives, scripts, memory access, and feature implementations.
- **Frontend** owns D3D12/ImGui rendering, navigation, panels, input, themes, and notifications.

The frontend must never contain raw GTA pointers, native resolution, pattern scanning, or script-global logic. Frontend actions call backend feature APIs.

## Main menu categories

Player, Vehicle, Weapon, **Teleport**, World, Network, Recovery, Heists, Lua, Settings.

Teleport is a first-class main-menu category rather than a Player or Misc submenu.

## Current milestone

This first V2 commit establishes a compileable DLL shell, application lifecycle, logger, backend lifecycle, native/script/hook placeholders, renderer lifecycle, a typed teleport backend request API, and a tested main-menu model.

The native, script, hook, and renderer classes are intentionally lifecycle-only in this milestone. GTA-specific addresses, native registration-table resolution, and D3D12 hook installation will be migrated only after the base builds cleanly.

## Local build

```powershell
cmake -S v2 -B build-v2 -A x64 -DBUILD_TESTING=ON
cmake --build build-v2 --config Release --parallel
ctest --test-dir build-v2 -C Release --output-on-failure
```

Output:

```text
build-v2/Release/Tutones-MenuV2.dll
```
