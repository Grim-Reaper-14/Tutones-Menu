#pragma once

#include <utility>

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/HornBoostRuntime.hpp"
#include "../features/vehicle/NitrousRuntime.hpp"
#include "../features/vehicle/VehicleAmmoRuntime.hpp"
#include "../features/vehicle/VehicleGeneralExtrasRuntime.hpp"
#include "../features/vehicle/VehicleLoopFeatures.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehicleSuspensionRuntime.hpp"
#include "../game/GameState.hpp"

#include <imgui.h>

namespace Tutones::UI
{
    inline void RenderVehicleGeneralV2Panel() noexcept
    {
        auto& loop = Game::Mods::VehicleLoopFeatures::Get();
        auto& hornBoost = Game::Mods::HornBoostRuntime::Get();
        auto& nitrous = Game::Mods::NitrousRuntime::Get();
        auto& vehicleAmmo = Game::Mods::VehicleAmmoRuntime::Get();
        auto& extras = Game::Mods::VehicleGeneralExtrasRuntime::Get();
        auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
        auto& suspension = Game::Mods::VehicleSuspensionRuntime::Get();
        const auto vehicleState = runtime.Snapshot();
        const auto gameState = Game::GameState::Get().Snapshot();
        const bool hasCurrentVehicle = gameState.inVehicle && gameState.vehicle != 0;

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_general_v2", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle General");
            ImGui::SameLine();
            ImGui::TextDisabled(hasCurrentVehicle ? "Current vehicle active" : "Enter a vehicle");
            ImGui::Separator();

            ImGui::SeparatorText("Persistent Vehicle Features");
            ImGui::TextDisabled("Current-vehicle behavior only. Spawning and garage tools stay in Vehicle Spawner.");

            if (ImGui::BeginTable("##vehicle_general_persistent", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool godMode = loop.VehicleGodMode();
                if (ImGui::Checkbox("Vehicle God Mode", &godMode))
                    loop.SetVehicleGodMode(godMode);
                DescribeLastV11Item("Keep the vehicle you are currently driving invincible. Protection follows you to a new vehicle and is restored when disabled.");

                ImGui::TableSetColumnIndex(1);
                bool keepFixed = extras.KeepVehicleFixed();
                if (ImGui::Checkbox("Keep Vehicle Fixed", &keepFixed))
                    extras.SetKeepVehicleFixed(keepFixed);
                DescribeLastV11Item("Continuously run GTA's full repair path when the current vehicle reports damage: body, entity health, engine, petrol tank, dirt and decals.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool keepClean = loop.KeepVehicleClean();
                if (ImGui::Checkbox("Keep Vehicle Clean", &keepClean))
                    loop.SetKeepVehicleClean(keepClean);
                DescribeLastV11Item("Continuously remove dirt and visible decals from the vehicle you are currently driving.");

                ImGui::TableSetColumnIndex(1);
                bool seatbelt = extras.Seatbelt();
                if (ImGui::Checkbox("Seatbelt", &seatbelt))
                    extras.SetSeatbelt(seatbelt);
                DescribeLastV11Item("Prevent the local player from flying through the windscreen or being knocked off the current vehicle, matching the current Enhanced seatbelt behavior.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool hornBoostEnabled = hornBoost.Enabled();
                if (ImGui::Checkbox("Horn Boost", &hornBoostEnabled))
                    hornBoost.SetEnabled(hornBoostEnabled);
                DescribeLastV11Item("Hold the normal vehicle horn while driving to progressively accelerate forward. The boost resets when the horn is released.");

                ImGui::TableSetColumnIndex(1);
                bool loweredStance = loop.LoweredStance();
                if (ImGui::Checkbox("Lower Vehicle Stance", &loweredStance))
                    loop.SetLoweredStance(loweredStance);
                DescribeLastV11Item("Continuously apply GTA's reduced-suspension state to supported vehicles and restore the previous vehicle when disabled.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool nitrousEnabled = nitrous.Enabled();
                if (ImGui::Checkbox("Nitrous", &nitrousEnabled))
                    nitrous.SetEnabled(nitrousEnabled);
                DescribeLastV11Item("Enable GTA's native nitrous backend on the vehicle you are currently driving. Hold Sprint + Accelerate to fire nitrous.");

                ImGui::TableSetColumnIndex(1);
                bool unlimitedNitrous = nitrous.Unlimited();
                if (ImGui::Checkbox("Unlimited Nitrous", &unlimitedNitrous))
                    nitrous.SetUnlimited(unlimitedNitrous);
                DescribeLastV11Item("Continuously refill the current vehicle's native nitrous charge while the nitrous system is enabled.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool infiniteVehicleAmmo = vehicleAmmo.Enabled();
                if (ImGui::Checkbox("Infinite Vehicle Ammo / Missiles", &infiniteVehicleAmmo))
                    vehicleAmmo.SetEnabled(infiniteVehicleAmmo);
                DescribeLastV11Item("Keep GTA's currently selected mounted vehicle weapon at unlimited restricted ammo. Covers supported guns, rockets and missiles without replacing GTA's normal projectile or targeting logic.");

                ImGui::TableSetColumnIndex(1);
                if (vehicleAmmo.Enabled())
                {
                    const auto weaponHash = vehicleAmmo.CurrentWeaponHash();
                    if (weaponHash != 0)
                        ImGui::Text("Weapon 0x%08X", static_cast<unsigned int>(weaponHash));
                    else
                        ImGui::TextDisabled("Weapon: waiting");
                }
                else
                {
                    ImGui::TextDisabled("Ammo refill off");
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool keepEngineRunning = extras.KeepEngineRunning();
                if (ImGui::Checkbox("Keep Engine Running", &keepEngineRunning))
                    extras.SetKeepEngineRunning(keepEngineRunning);
                DescribeLastV11Item("Use GTA's LeaveEngineOnWhenExitingVehicles ped flag and keep the active vehicle engine running while enabled.");

                ImGui::TableSetColumnIndex(1);
                bool allowHats = extras.AllowHatsInVehicles();
                if (ImGui::Checkbox("Keep Hats / Headgear", &allowHats))
                    extras.SetAllowHatsInVehicles(allowHats);
                DescribeLastV11Item("Apply GTA reset flag 337 each tick so supported hats and headgear stay equipped inside vehicles.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool headlights = extras.KeepHeadlightsOn();
                if (ImGui::Checkbox("Headlights Always On", &headlights))
                    extras.SetKeepHeadlightsOn(headlights);
                DescribeLastV11Item("Continuously force the current vehicle headlights on using GTA's vehicle-light state native.");

                ImGui::TableSetColumnIndex(1);
                bool highBeams = extras.HighBeams();
                if (ImGui::Checkbox("High Beams", &highBeams))
                    extras.SetHighBeams(highBeams);
                DescribeLastV11Item("Continuously force the current vehicle's full-beam headlights while enabled.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool speedReadout = extras.SpeedReadout();
                if (ImGui::Checkbox("Live Speed Readout", &speedReadout))
                    extras.SetSpeedReadout(speedReadout);
                DescribeLastV11Item("Read the current vehicle speed every GTA tick and show MPH and KPH in the status block below.");

                ImGui::TableSetColumnIndex(1);
                if (speedReadout)
                    ImGui::Text("%.0f MPH  /  %.0f KPH", extras.SpeedMph(), extras.SpeedKph());
                else
                    ImGui::TextDisabled("Speed readout off");

                ImGui::EndTable();
            }

            ImGui::SeparatorText("Nitrous Tuning");
            ImGui::BeginDisabled(!nitrous.Enabled());
            float nitrousLevel = nitrous.Level();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##nitrous_level", &nitrousLevel, 0.5f, 5.0f, "Charge %.2fx"))
                nitrous.SetLevel(nitrousLevel);
            DescribeLastV11Item("Set the native nitrous charge level supplied to SET_OVERRIDE_NITROUS_LEVEL.");

            float nitrousPower = nitrous.Power();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##nitrous_power", &nitrousPower, 0.5f, 5.0f, "Power %.2fx"))
                nitrous.SetPower(nitrousPower);
            DescribeLastV11Item("Set the power multiplier used by GTA's native nitrous system.");
            ImGui::EndDisabled();
            ImGui::TextDisabled("Nitrous input: hold Sprint + Accelerate while driving.");

            ImGui::SeparatorText("Ride Height");
            bool extraLowering = suspension.Enabled();
            if (ImGui::Checkbox("Extra Suspension Lowering", &extraLowering))
                suspension.SetEnabled(extraLowering);
            DescribeLastV11Item("Lower the current Enhanced vehicle through its CHandlingData fSuspensionRaise value. The original ride height is restored when disabled, when changing vehicles, or during teardown.");

            float loweringAmount = suspension.LoweringAmount();
            ImGui::BeginDisabled(!extraLowering);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##extra_suspension_lowering", &loweringAmount, 0.0f, 0.20f, "Extra drop %.3f"))
                suspension.SetLoweringAmount(loweringAmount);
            ImGui::EndDisabled();
            DescribeLastV11Item("Higher values lower the Enhanced vehicle farther. Very low settings can cause body or wheel clipping on some vehicles.");

            if (extraLowering && hasCurrentVehicle)
            {
                if (suspension.Supported())
                {
                    ImGui::TextDisabled(
                        "Ride-height path: %s | write: %s",
                        suspension.ActivePathName(),
                        suspension.LastWriteSucceeded() ? "OK" : "FAILED");
                }
                else
                {
                    ImGui::TextDisabled("Ride-height path: resolving Enhanced handling data...");
                }
            }

            ImGui::SeparatorText("Quick Vehicle Actions");

            ImGui::BeginDisabled(!runtime.IsRunning() || !hasCurrentVehicle);
            if (ImGui::BeginTable("##vehicle_general_actions", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Repair", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueRepair());
                DescribeLastV11Item("Repair the current vehicle through the verified vehicle runtime.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Clean Now", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueClean());
                DescribeLastV11Item("Immediately clear the current vehicle's dirt level.");

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Set Upright", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueFlipUpright());
                DescribeLastV11Item("Place a rolled or awkwardly landed vehicle upright on the ground.");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##vehicle_general_engine", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Engine On", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(extras.QueueSetEngine(true));
                DescribeLastV11Item("Immediately start the current vehicle engine.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Engine Off", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(extras.QueueSetEngine(false));
                DescribeLastV11Item("Immediately stop the current vehicle engine and disable automatic restart for that call.");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##vehicle_general_locks", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Lock Doors", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(extras.QueueSetDoorsLocked(true));
                DescribeLastV11Item("Set the current vehicle's door-lock state to locked.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Unlock Doors", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(extras.QueueSetDoorsLocked(false));
                DescribeLastV11Item("Restore the current vehicle's door-lock state to unlocked.");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##vehicle_general_stealth", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::BeginDisabled(vehicleState.stealthPending);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Enable Vehicle Stealth", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueStealthMode(true));
                DescribeLastV11Item("Enable the supported stealth state on vehicles such as the Akula, Annihilator2 and Raiju.");

                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Disable Vehicle Stealth", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(runtime.QueueStealthMode(false));
                DescribeLastV11Item("Restore the supported stealth vehicle's normal wing or missile-bay state.");
                ImGui::EndDisabled();
                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(!Game::Native::NativeRegistry::Get().IsReady());
            if (ImGui::Button("Enter Last Vehicle", ImVec2(-1.0f, 30.0f)))
                static_cast<void>(extras.QueueEnterLastVehicle());
            ImGui::EndDisabled();
            DescribeLastV11Item("Warp the local player into the driver seat of the last GTA vehicle they occupied, when that entity still exists.");

            ImGui::SeparatorText("Current Vehicle Status");
            ImGui::Text("Vehicle: %d", gameState.vehicle);
            ImGui::SameLine();
            ImGui::Text("  In vehicle: %s", gameState.inVehicle ? "YES" : "NO");
            if (extras.SpeedReadout())
                ImGui::Text("Speed: %.0f MPH  |  %.0f KPH", extras.SpeedMph(), extras.SpeedKph());
            ImGui::Text("God Mode: %s", loop.VehicleGodMode() ? "ON" : "OFF");
            ImGui::SameLine();
            ImGui::Text("  Fixed: %s", extras.KeepVehicleFixed() ? "ON" : "OFF");
            ImGui::Text("Seatbelt: %s", extras.Seatbelt() ? "ON" : "OFF");
            ImGui::SameLine();
            ImGui::Text("  Engine Hold: %s", extras.KeepEngineRunning() ? "ON" : "OFF");
            ImGui::Text("Nitrous: %s  |  Unlimited: %s  |  Power: %.2fx", nitrous.Enabled() ? "ON" : "OFF", nitrous.Unlimited() ? "ON" : "OFF", nitrous.Power());
            ImGui::Text(
                "Vehicle Ammo/Missiles: %s  |  tracked weapons: %zu",
                vehicleAmmo.Enabled() ? "INFINITE" : "NORMAL",
                vehicleAmmo.TrackedWeaponCount());
            ImGui::Text("Extra Lowering: %s  |  Drop: %.3f", suspension.Enabled() ? "ON" : "OFF", suspension.LoweringAmount());
            if (suspension.Enabled())
            {
                ImGui::TextDisabled(
                    "Lowering backend: %s | write: %s",
                    suspension.ActivePathName(),
                    suspension.LastWriteSucceeded() ? "OK" : "WAITING/FAILED");
            }
            if (vehicleState.lastAction != Game::Mods::VehicleModAction::None)
            {
                const char* status = vehicleState.stealthPending
                    && vehicleState.lastAction == Game::Mods::VehicleModAction::SetStealthMode
                    ? "PENDING"
                    : (vehicleState.lastActionSucceeded ? "SUCCESS" : "FAILED");
                ImGui::TextDisabled("Last vehicle action: %s", status);
                if (!vehicleState.lastActionMessage.empty())
                    ImGui::TextWrapped("%s", vehicleState.lastActionMessage.c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        SetV11Description("Vehicle General - active-vehicle behavior: God Mode, Keep Fixed/Clean, Seatbelt, Horn Boost, native Nitrous, infinite mounted-weapon ammo/missiles, stance, extra ride-height lowering, engine, lights, headgear, speed, locks, repair, upright and stealth controls.");
    }
}
