# Tutones Menu V2 Architecture

## Layer contract

### Core
Owns process lifecycle and reusable infrastructure. It must not know about individual GTA features.

Planned services: logging, configuration, filesystem, scheduler/task queue, build metadata, diagnostics, and Lua VM ownership.

### Backend
Owns all game-facing operations. This layer will eventually contain game state, pointer discovery, pattern scanning, native registration/invocation, script globals/locals/functions, hooks, and feature runtimes.

Frontend code is not allowed to resolve natives or touch GTA memory directly.

### Frontend
Owns presentation and input: D3D12, Dear ImGui, menu navigation, panels, theme, fonts/textures, notifications, keyboard/controller input.

Frontend panels submit requests to typed backend feature APIs.

## Primary navigation

1. Player
2. Vehicle
3. Weapon
4. Teleport
5. World
6. Network
7. Recovery
8. Heists
9. Lua
10. Settings

Teleport deliberately remains a top-level category.

## Migration order from V1

1. Validate x64 DLL bootstrap and lifecycle.
2. Bring over logging/config/filesystem foundations.
3. Add D3D12 + ImGui renderer and input shell.
4. Add pattern/pointer infrastructure.
5. Add native resolver/invoker and game-thread dispatch.
6. Add script runtime (globals, locals, pointers, functions).
7. Add MinHook-backed hook manager with explicit install/uninstall lifecycle.
8. Add Lua and bind it to backend APIs rather than directly to raw memory.
9. Migrate feature domains one at a time, including Teleport as its own domain.
10. Keep tests and Windows CI green after each migration slice.
