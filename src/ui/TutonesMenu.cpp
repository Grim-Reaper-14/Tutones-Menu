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
            BaseCategories[4],
            BaseCategories[5],
            BaseCategories[6],
            BaseCategories[7],
            BaseCategories[8],
            {"M", "MISC",
                {{"General", "HUD", "World", "Camera & Utilities"}},
                {{"M", "H", "O", "C"}},
                {{"Gameplay convenience actions and shared quality-of-life controls.", "Always-on coordinates, heading, FPS and Online session overlays.", "Local clock, freeze-time, weather and blackout controls.", "Gameplay camera telemetry/shake suppression plus player cleanup, parachute and underwater utilities."}}, 4, true},
        }};
    }
}

#include "TutonesMenu.part01.inc"
#include "TutonesMenu.part03.inc"
