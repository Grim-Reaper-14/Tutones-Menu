#include "BusinessPanel.hpp"
#include "CasinoLuckyWheelPanel.hpp"
#include "GoodBehaviorBonusPanel.hpp"
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
            {"D", "RECOVERY",
                {{"Overview", "RP Multiplier", "Unlocks", "Casino"}},
                {{"D", "H", "U", "C"}},
                {{"Live Enhanced Recovery capability and character-state overview, including the Good Behavior Bonus reward trigger.", "Override the current Enhanced XP multiplier while enabled, restoring the prior value when disabled.", "Enhanced DLC clothing unlock groups with packed-stat read-back verification.", "Enhanced Casino Lucky Wheel globals plus a validated read-only casino_lucky_wheel player-local inspector."}}, 4, true},
            BaseCategories[6],
            {"S", "PROTECTIONS",
                {{"Overview", "Network Events", "Script Events", nullptr}},
                {{"S", "N", "K", nullptr}},
                {{"Live Enhanced packet-hook status, counters and last blocked event.", "Yim-style PackedEvents filters for malformed traffic, sound, explosion, fire, weapon damage, ragdoll, clear-task and PTFX events.", "Malformed CScriptedGameEvent validation plus an optional full scripted-event block mode.", nullptr}}, 3, true},
            BaseCategories[8],
            {"M", "MISC",
                {{"General", "HUD", "Businesses", "Camera & Utilities"}},
                {{"M", "H", "B", "C"}},
                {{"Gameplay convenience actions and shared quality-of-life controls.", "Always-on coordinates, heading, FPS and Online session overlays.", "Central business hub for Nightclub, Special Cargo, Bunker, Motorcycle Club, Acid Lab, Hangar/Air Freight and Vehicle Cargo, with validated Enhanced globals and script runtime guards.", "Gameplay camera telemetry/shake suppression plus player cleanup, parachute and underwater utilities."}}, 4, true},
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

        void RenderRecoveryNavigationPanel(std::size_t index) noexcept
        {
            if (index == 3)
            {
                RenderCasinoLuckyWheelPanel();
                return;
            }

            // Recovery navigation exposes Overview / RP Multiplier / Unlocks.
            // The legacy business renderer remains at internal index 2 so the Misc
            // business hub can reuse it; visible Unlocks therefore maps to index 3.
            RenderRecoveryPanel(index == 2 ? 3 : index);

            // Good Behavior Bonus is a Recovery reward, not a business feature.
            // Render it after the Overview panel so the control stays visible above
            // the full-size Recovery child and actually participates in the menu UI.
            if (index == 0)
                RenderGoodBehaviorBonusControl();
        }
    }
}

#include "TutonesMenu.part01.inc"
#define DrawPanel(...) RenderProtectionPanel(m_Item)
#define RenderMiscOverlay(...) RenderTutonesRuntimeOverlays()
#define RenderRecoveryPanel(index) RenderRecoveryNavigationPanel(index)
#define RenderMiscPanel(index) ((m_Item == 2) ? RenderBusinessPanel() : ::Tutones::UI::RenderMiscPanel(index))
#include "TutonesMenu.part03.inc"
#undef RenderMiscPanel
#undef RenderRecoveryPanel
#undef RenderMiscOverlay
#undef DrawPanel
