#include "MiscPanel.hpp"
#include "NativeToolsPanel.hpp"
#include "NightclubPanel.hpp"
#include "PersistentMenuState.hpp"
#include "ProtectionPanel.hpp"

#define Categories BaseCategories
#include "TutonesMenu.part00.inc"
#undef Categories

namespace Tutones::UI
{
    namespace
    {
        constexpr std::array<CategoryEntry, 11> Categories{{
            BaseCategories[0],
            BaseCategories[1],
            BaseCategories[2],
            BaseCategories[3],
            {"O", "WORLD",
                {{"General", "Time & Weather", "Teleport", "Entities"}},
                {{"O", "Q", "L", "E"}},
                {{"Pedestrian, scenario, traffic and parked-vehicle population density controls.", "Verified Enhanced local clock, freeze-time, weather and blackout controls.", "Waypoint and map-destination teleport tools with ground-height resolution.", "Local entity cleanup plus a live crosshair inspector for model, transform, health, material, vehicle and ped debug data."}}, 4, true},
            BaseCategories[5],
            BaseCategories[6],
            {"S", "PROTECTIONS",
                {{"Overview", "Network Events", "Script Events", nullptr}},
                {{"S", "N", "K", nullptr}},
                {{"Live Enhanced packet-hook status, counters and last blocked event.", "Yim-style PackedEvents filters for malformed traffic, sound, explosion, fire, weapon damage, ragdoll, clear-task and PTFX events.", "Malformed CScriptedGameEvent validation plus an optional full scripted-event block mode.", nullptr}}, 3, true},
            BaseCategories[8],
            {"M", "MISC",
                {{"General", "HUD", "Nightclub", "Camera & Utilities"}},
                {{"M", "H", "N", "C"}},
                {{"Gameplay convenience actions and shared quality-of-life controls.", "Always-on coordinates, heading, FPS and Online session overlays.", "Enhanced 1.73 Nightclub warehouse values, cooldowns, capacity, production tuning and popularity income globals. Values are written only when you press an Apply button.", "Gameplay camera telemetry/shake suppression plus player cleanup, parachute and underwater utilities."}}, 4, true},
            {"T", "NATIVE TOOLS",
                {{"Workshop", "Vehicle & Camera", "World Tools", "Diagnostics"}},
                {{"W", "C", "O", "K"}},
                {{"Enhanced weapon components/tints, player props, full outfit presets and animation playback.", "Scripted freecam plus native vehicle doors, windows, engine, lights, top-speed, torque and grip controls.", "Custom blips, particle effects, bodyguards, IPL streaming and interior entity-set controls.", "Resolve current Enhanced native hashes to executable handler addresses without invoking them."}}, 4, true},
        }};

        void RenderTutonesRuntimeOverlays() noexcept
        {
            // Keep F4 usable even when GTA's WndProc path misses the key event.
            // This runs once per rendered frame before TutonesMenu checks IsMenuOpen().
            Input::Get().PollFallbackHotkeys();
            static_cast<void>(Game::Protections::ProtectionRuntime::Get().Start());
            RenderMiscOverlay();
        }
    }
}

#include "TutonesMenu.part01.inc"
#define DrawPanel(...) RenderProtectionPanel(m_Item)
#define RenderMiscOverlay(...) RenderTutonesRuntimeOverlays()
#define RenderMiscPanel(index) ((m_Item == 2) ? RenderNightclubPanel() : ::Tutones::UI::RenderMiscPanel(index))
#include "TutonesMenu.part03.inc"
#undef RenderMiscPanel
#undef RenderMiscOverlay
#undef DrawPanel
