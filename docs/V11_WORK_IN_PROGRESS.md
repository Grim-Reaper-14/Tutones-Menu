# Tutones Menu V11 — Work In Progress

This checkpoint preserves the V11 work that is being built on top of the working `vehicle workshop v10` baseline.

## UI direction

- Keep the existing Tutones menu structure and input model.
- Move the visual theme to black / charcoal with electric-blue accents.
- Use the approved `Tutones Menu` banner artwork as the top header.
- Add a matching bottom description banner that changes with the highlighted control.
- Reorganize the main rail into:
  - Player
  - Weapons
  - Vehicle
  - Game
  - World
  - Recovery
  - Network
  - Protections
  - Menu Settings

## Player

Keep the current Player functionality and organize it into General, Protection, Online, Movement, and Appearance groups.

Planned / in-progress additions:

- God Mode
- Max Health / Heal
- Bullet proof
- Fire proof
- Explosion proof
- Melee proof
- Off Radar backed by Freemode script globals
- Existing movement and appearance controls remain available

## Weapons

The V11 Weapons layout is organized around:

- General
- Ammo
  - Infinite Ammo
  - Infinite Clip
- Aimbot
  - Enable Aimbot
  - Aim For Head
  - Target Drivers
  - Release Dead Ped
- Bullet Effects
  - Explosive Ammo
  - Explosion type
  - Explosion damage
  - Camera shake

The YimMenuV2 Enhanced implementation is being used as the behavior reference. Patch-based aim behavior is kept separate from native-only weapon features.

## Vehicle

All vehicle work is being consolidated into one Vehicle Editor workspace with tabs for:

- General / current vehicle
- Handling / stance
- Spawn
- Personal Vehicles
- Clone
- Paint
- LSC modifications
- Wheels
- Lights & tires
- Local presets

Requirements retained from V10 and the V11 design pass include named paint catalogs, named wheel families/styles, lights, tire smoke, clone-nearest, license plates, and player-relative spawn placement.

True GTA Online Personal Vehicles / garages and Save Personal Vehicle remain distinct from Tutones local saved presets.

## Game

Session switching belongs under Game rather than Network.

Yim-style join types currently tracked by the V11 foundation:

- Public
- New / Solo Public
- Closed Crew
- Crew
- Closed Friends
- Find Friend
- Solo
- Invite Only
- Join Crew
- SCTV
- Leave Online

Creator transition is intentionally kept separate until its exact transition path is verified.

## Shared script foundation

This checkpoint includes the reusable script-program / script-global / script-pointer / script-function scaffolding needed by later V11 Online features.

The intent is to use one shared backend for:

- Off Radar
- Game session transitions
- Personal Vehicles and garage data
- Save Personal Vehicle
- LSC restriction script patching

The files are deliberately not added to the production CMake target in this WIP checkpoint. The working V10 build therefore remains the executable baseline while the V11 script layer is completed and validated.

## Settings and storage

V11 will move persistent Tutones data under:

`%LOCALAPPDATA%\\Tutones Menu\\`

Planned subfolders:

- `config`
- `logs`
- `saved_vehicles`
- `scripts`
- `cache`
- `dumps`
- `assets`
- `data`

`menu_settings.json` will persist stateful toggles, sliders, selectors, theme/layout options, and input preferences. Loading a config must never execute one-shot commands such as Heal, Clone Vehicle, Leave Online, Join Session, or Save Personal Vehicle.

## Checkpoint policy

This is a source-preservation checkpoint, not the final V11 integration commit. The V10 executable path is intentionally left unchanged until the new V11 runtime pieces are wired and Windows/MSVC build-tested.
