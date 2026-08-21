#include "LscBypassWidget.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    void RenderLscBypassWidget() noexcept
    {
        auto& runtime = Game::Mods::LscBypassRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        ImGui::Separator();
        ImGui::TextColored(V11Theme::Accent, "LSC Restrictions");

        const bool patternsReady = snapshot.canUseVehicleSupported && snapshot.blockMenuOptionSupported;
        const bool canToggle = snapshot.running && snapshot.hookActive && snapshot.programLoaded && patternsReady;
        bool enabled = snapshot.enabled;

        ImGui::BeginDisabled(!canToggle);
        if (ImGui::Checkbox("Remove LSC restrictions", &enabled))
            runtime.SetEnabled(enabled);
        ImGui::EndDisabled();
        DescribeLastV11Item("Enable the verified Enhanced carmod_shop shadow-bytecode patches that remove the real script-side LSC use/menu restrictions. The toggle stays disabled until both exact patterns are supported.");

        if (!snapshot.running)
        {
            ImGui::TextDisabled("LSC restriction runtime is offline.");
        }
        else if (!snapshot.hookActive)
        {
            ImGui::TextDisabled("ScriptVM shadow-patch hook is unavailable; GTA script bytecode is untouched.");
        }
        else if (!snapshot.programLoaded)
        {
            ImGui::TextDisabled("Waiting for carmod_shop to load before verifying the two Enhanced patch signatures.");
        }
        else if (!patternsReady)
        {
            ImGui::TextDisabled("carmod_shop is loaded, but one or both exact Enhanced restriction patterns were not found.");
        }
        else if (snapshot.applied)
        {
            ImGui::TextColored(V11Theme::Accent, "Active: both carmod_shop restriction patches are running from shadow bytecode.");
        }
        else if (snapshot.enabled)
        {
            ImGui::TextDisabled("Enabled; both patches are verified and will apply on the next carmod_shop VM execution.");
        }
        else
        {
            ImGui::TextDisabled("Available: both current Enhanced carmod_shop restriction patterns are verified.");
        }

        ImGui::TextDisabled("This is separate from Tutones' direct native mod apply path and removes the real script-side LSC restrictions.");

        ImGui::SeparatorText("Persistent Vehicle Features");
        auto& vehicleFeatures = Game::Mods::VehicleLoopFeatures::Get();

        bool keepClean = vehicleFeatures.KeepVehicleClean();
        if (ImGui::Checkbox("Keep Vehicle Clean", &keepClean))
            vehicleFeatures.SetKeepVehicleClean(keepClean);
        DescribeLastV11Item("Continuously keeps the vehicle you are driving at zero dirt level. The loop follows you when you change vehicles.");

        bool loweredStance = vehicleFeatures.LoweredStance();
        if (ImGui::Checkbox("Lowered Stance", &loweredStance))
            vehicleFeatures.SetLoweredStance(loweredStance);
        DescribeLastV11Item("Continuously applies the reduced-suspension stance to supported vehicles. Turning this off restores normal suspension on the last affected vehicle.");

        if (loweredStance)
            ImGui::TextDisabled("Lowered stance is active; only vehicles supported by GTA's suspension native will visibly change.");
    }
}
