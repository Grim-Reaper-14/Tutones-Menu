# Tutones Menu Architecture

Tutones Menu is a fresh D3D12 + Dear ImGui hooked menu project.

## Layout

```text
Tutones-Menu/
├── cmake/                  # CMake helper modules and dependency setup
├── docs/                   # Architecture and development notes
├── assets/
│   └── reference/          # UI reference images and design material
├── src/
│   ├── app/                # Application/bootstrap ownership
│   ├── backend/            # Game-facing backend services
│   ├── features/           # Menu features grouped by domain
│   ├── game/               # GTA-facing types, natives, runtime access
│   ├── hooking/            # D3D12/Win32/MinHook integration
│   ├── render/             # D3D12 renderer and ImGui renderer layer
│   ├── runtime/            # Runtime lifecycle and thread/tick coordination
│   ├── ui/                 # Menu layout, navigation, widgets, themes
│   └── utils/               # Small reusable helpers
├── third_party/
│   └── imgui/              # Dear ImGui dependency
└── tests/                  # Non-runtime tests and utilities
```

## Runtime direction

The renderer layer owns D3D12/ImGui rendering. The hooking layer owns swap-chain/device hook installation. UI code must not perform GTA/native work directly; UI actions enqueue requests into backend/game services. Game/runtime code owns native resolution, runtime lifecycle, and game-thread execution.

## First milestone

1. D3D12 hook initializes safely.
2. ImGui initializes through Win32 + D3D12 backends.
3. Tutones UI matches the approved reference layout.
4. F4 toggles the UI.
5. Numpad 8/2/4/6/5/0 navigation works.
6. No GTA feature code is added until this shell is stable.

## Design reference

The approved visual direction is the Dear ImGui menu reference supplied for Tutones Menu. The layout uses a narrow icon rail, a category rail, and a large content area with a dark theme and restrained accent color.
