#include "MiscPanel.hpp"

#define Categories BaseCategories
#include "TutonesMenu.part00.inc"
#undef Categories

namespace Tutones::UI
{
    namespace
    {
        constexpr std::array<CategoryEntry, 10> Categories{{
            BaseCategories[0],
            BaseCategories[1],
            BaseCategories[2],
            BaseCategories[3],
            {"O", "WORLD",
                {{"General", "Time & Weather", "Teleport", "Entities"}},
                {{"O", "Q", "L", "E"}},
                {{"Pedestrian, scenario, traffic and parked-vehicle population density controls.", "Verified Enhanced local clock, freeze-time, weather and blackout controls.", "Waypoint and map-destination teleport tools with ground-height resolution.", "Radius-based local cleanup for ambient peds, vehicles and world objects."}}, 4, true},
            BaseCategories[5],
            BaseCategories[6],
            BaseCategories[7],
            BaseCategories[8],
            {"M", "MISC",
                {{"General", "HUD", "Camera & Utilities", nullptr}},
                {{"M", "H", "C", nullptr}},
                {{"Gameplay convenience actions and shared quality-of-life controls.", "Always-on coordinates, heading, FPS and Online session overlays.", "Gameplay camera telemetry/shake suppression plus player cleanup, parachute and underwater utilities.", nullptr}}, 3, true},
        }};
    }
}

#include "TutonesMenu.part01.inc"
#include "TutonesMenu.part03.inc"
