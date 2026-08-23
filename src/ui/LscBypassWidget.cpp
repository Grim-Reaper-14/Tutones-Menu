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

        bool godMode = vehicleFeatures.VehicleGodMode();
        if (ImGui::Checkbox("Vehicle God Mode", &godMode))
            vehicleFeatures.SetVehicleGodMode(godMode);
        DescribeLastV11Item("Blocks incoming damage to the vehicle you are currently using. Protection follows you to a new vehicle and is removed from the old vehicle when you switch, exit, disable the toggle, or unload Tutones.");

        bool autoRepair = vehicleFeatures.AutoRepair();
        if (ImGui::Checkbox("Auto Repair", &autoRepair))
            vehicleFeatures.SetAutoRepair(autoRepair);
        DescribeLastV11Item("Continuously repairs body and mechanical damage on the vehicle you are currently using.");

        bool bulletproofTyres = vehicleFeatures.BulletproofTyres();
        if (ImGui::Checkbox("Bulletproof Tyres", &bulletproofTyres))
            vehicleFeatures.SetBulletproofTyres(bulletproofTyres);
        DescribeLastV11Item("Prevents the current vehicle's tyres from bursting. The previous tyre setting is restored when you switch vehicles, disable the toggle, or unload Tutones.");

        bool engineAlwaysOn = vehicleFeatures.EngineAlwaysOn();
        if (ImGui::Checkbox("Engine Always On", &engineAlwaysOn))
            vehicleFeatures.SetEngineAlwaysOn(engineAlwaysOn);
        DescribeLastV11Item("Keeps the engine running on the vehicle you are currently using.");

        bool autoFlip = vehicleFeatures.AutoFlip();
        if (ImGui::Checkbox("Auto Flip", &autoFlip))
            vehicleFeatures.SetAutoFlip(autoFlip);
        DescribeLastV11Item("When your current vehicle is upside down and moving slowly, Tutones automatically places it upright on the ground.");

        bool invisible = vehicleFeatures.VehicleInvisible();
        if (ImGui::Checkbox("Vehicle Invisible", &invisible))
            vehicleFeatures.SetVehicleInvisible(invisible);
        DescribeLastV11Item("Makes your current vehicle invisible. Visibility is restored on the old vehicle when you switch, exit, disable the toggle, or unload Tutones.");

        bool noCollision = vehicleFeatures.NoCollision();
        if (ImGui::Checkbox("No Collision", &noCollision))
            vehicleFeatures.SetNoCollision(noCollision);
        DescribeLastV11Item("Disables collision on your current vehicle while enabled. Collision is restored on the old vehicle when you switch, exit, disable the toggle, or unload Tutones.");

        bool keepClean = vehicleFeatures.KeepVehicleClean();
        if (ImGui::Checkbox("Keep Vehicle Clean", &keepClean))
            vehicleFeatures.SetKeepVehicleClean(keepClean);
        DescribeLastV11Item("Continuously removes dirt and vehicle decals from the vehicle you are currently using. This does not grant invincibility or repair mechanical/body damage.");

        bool loweredStance = vehicleFeatures.LoweredStance();
        if (ImGui::Checkbox("Lowered Stance", &loweredStance))
            vehicleFeatures.SetLoweredStance(loweredStance);
        DescribeLastV11Item("Continuously applies the reduced-suspension stance to supported vehicles. Turning this off restores normal suspension on the last affected vehicle.");

        if (ImGui::Button("Vehicle Jump", ImVec2(-1.0f, 0.0f)))
            static_cast<void>(vehicleFeatures.QueueVehicleJump());
        DescribeLastV11Item("Adds an upward velocity boost to your current vehicle while preserving its horizontal movement.");

        if (godMode)
            ImGui::TextDisabled("Vehicle God Mode is active for your current vehicle.");
        if (autoRepair)
            ImGui::TextDisabled("Auto Repair is maintaining body and mechanical health.");
        if (bulletproofTyres)
            ImGui::TextDisabled("Bulletproof Tyres is protecting the current vehicle.");
        if (engineAlwaysOn)
            ImGui::TextDisabled("Engine Always On is active.");
        if (autoFlip)
            ImGui::TextDisabled("Auto Flip is watching for a slow upside-down vehicle.");
        if (invisible)
            ImGui::TextDisabled("Vehicle Invisible is active for your current vehicle.");
        if (noCollision)
            ImGui::TextDisabled("No Collision is active for your current vehicle.");
        if (keepClean)
            ImGui::TextDisabled("Vehicle cleaning loop is active: dirt and decals are continuously removed.");
        if (loweredStance)
            ImGui::TextDisabled("Lowered stance is active; only vehicles supported by GTA's suspension native will visibly change.");
    }
}
