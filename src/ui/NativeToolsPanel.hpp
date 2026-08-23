#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/native/EnhancedNativeToolkit.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::UI
{
    namespace NativeToolsPanelDetail
    {
        inline int g_WeaponTint{};
        inline Game::Hash g_LastWeapon{};
        inline char g_ComponentName[64] = "COMPONENT_AT_AR_SUPP_02";

        inline int g_PropSlot{};
        inline int g_PropDrawable{};
        inline int g_PropTexture{};
        inline char g_OutfitName[64] = "tutones_outfit";
        inline std::vector<std::string> g_SavedOutfits{};
        inline int g_SelectedOutfit{-1};

        inline char g_AnimDictionary[96] = "anim@mp_player_intcelebrationmale@thumbs_up";
        inline char g_AnimName[64] = "thumbs_up";
        inline bool g_AnimLoop{};
        inline bool g_AnimUpperBody{};

        inline float g_FreecamSpeed{0.45f};
        inline float g_FreecamFov{70.0f};
        inline int g_VehicleDoor{};
        inline int g_VehicleWindow{};
        inline int g_VehicleLights{};
        inline bool g_PerformanceEnabled{};
        inline float g_TopSpeedModifier{};
        inline float g_TorqueMultiplier{1.0f};
        inline bool g_ReduceGrip{};

        inline int g_BlipSprite{1};
        inline int g_BlipColor{5};
        inline bool g_BlipRoute{};
        inline char g_PtfxAsset[64] = "core";
        inline char g_PtfxEffect[96] = "ent_sht_electrical_box";
        inline float g_PtfxScale{1.0f};
        inline char g_InteriorSet[96]{};
        inline int g_InteriorColor{};
        inline char g_IplName[96]{};

        inline char g_ProbeHash[32] = "0xB0D77D90171EC35F";

        [[nodiscard]] inline std::uint32_t Joaat(std::string_view text) noexcept
        {
            std::uint32_t hash{};
            for (char ch : text)
            {
                if (ch >= 'A' && ch <= 'Z')
                    ch = static_cast<char>(ch - 'A' + 'a');
                hash += static_cast<std::uint8_t>(ch);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

        [[nodiscard]] inline std::uint64_t ParseHash64(const char* text) noexcept
        {
            if (!text || text[0] == '\0')
                return 0;
            char* end{};
            const auto value = std::strtoull(text, &end, 0);
            return end != text ? static_cast<std::uint64_t>(value) : 0;
        }

        inline void RefreshOutfits(Game::NativeTools::EnhancedNativeToolkit& toolkit)
        {
            g_SavedOutfits = toolkit.SavedOutfitNames();
            if (g_SavedOutfits.empty())
            {
                g_SelectedOutfit = -1;
                return;
            }
            g_SelectedOutfit = std::clamp(g_SelectedOutfit, 0, static_cast<int>(g_SavedOutfits.size()) - 1);
        }

        inline void SyncWeaponUi(const Game::NativeTools::ToolkitSnapshot& snapshot) noexcept
        {
            if (snapshot.selectedWeapon == 0 || snapshot.selectedWeapon == g_LastWeapon)
                return;
            g_LastWeapon = snapshot.selectedWeapon;
            g_WeaponTint = std::clamp(snapshot.weaponTint, 0, 31);
        }

        inline void RenderStatus(const Game::NativeTools::ToolkitSnapshot& snapshot) noexcept
        {
            ImGui::SeparatorText("Toolkit Status");
            ImGui::Text("Enhanced native runtime: %s", snapshot.nativeReady ? "ready" : "unavailable");
            if (snapshot.pending)
                ImGui::TextDisabled("%s", snapshot.message.c_str());
            else if (snapshot.haveResult)
                ImGui::TextWrapped("%s: %s", snapshot.lastSucceeded ? "Success" : "Failed", snapshot.message.c_str());
            else
                ImGui::TextDisabled("%s", snapshot.message.c_str());
        }

        inline void RenderWorkshop(Game::NativeTools::EnhancedNativeToolkit& toolkit, const Game::NativeTools::ToolkitSnapshot& snapshot)
        {
            SyncWeaponUi(snapshot);

            ImGui::TextColored(V11Theme::Accent, "Weapon Workshop");
            ImGui::Text("Current weapon: 0x%08X", snapshot.selectedWeapon);
            ImGui::SliderInt("Weapon Tint", &g_WeaponTint, 0, 31);
            DescribeLastV11Item("Select the native tint index for the weapon currently in your hands.");
            ImGui::BeginDisabled(snapshot.pending || snapshot.selectedWeapon == 0);
            if (ImGui::Button("Apply Weapon Tint", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueSetWeaponTint(g_WeaponTint));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply and read back the selected weapon tint through the Enhanced weapon natives.");

            ImGui::TextDisabled("Component name / hash source");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##weapon_component_name", g_ComponentName, sizeof(g_ComponentName));
            const std::uint32_t componentHash = Joaat(g_ComponentName);
            ImGui::TextDisabled("JOAAT: 0x%08X", componentHash);
            DescribeLastV11Item("Enter a GTA component identifier such as COMPONENT_AT_AR_SUPP_02. Compatibility is checked against the current weapon before applying it.");
            ImGui::BeginDisabled(snapshot.pending || snapshot.selectedWeapon == 0 || componentHash == 0);
            if (ImGui::Button("Add Compatible Component", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueWeaponComponent(componentHash, true));
            ImGui::SameLine();
            if (ImGui::Button("Remove", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueWeaponComponent(componentHash, false));
            ImGui::EndDisabled();
            DescribeLastV11Item("Add or remove the component only if GTA reports that the selected weapon accepts it, then verify the final component state.");

            ImGui::SeparatorText("Outfit Props");
            constexpr std::array<const char*, 8> PropNames{{
                "Hats", "Glasses", "Ears", "Unknown 3", "Unknown 4", "Unknown 5", "Watches", "Bracelets",
            }};
            ImGui::Combo("Prop Slot", &g_PropSlot, PropNames.data(), static_cast<int>(PropNames.size()));
            ImGui::InputInt("Prop Drawable", &g_PropDrawable);
            ImGui::InputInt("Prop Texture", &g_PropTexture);
            g_PropDrawable = std::max(0, g_PropDrawable);
            g_PropTexture = std::max(0, g_PropTexture);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Apply Prop", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueSetProp(g_PropSlot, g_PropDrawable, g_PropTexture));
            ImGui::SameLine();
            if (ImGui::Button("Clear Prop", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueClearProp(g_PropSlot));
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply or clear GTA prop slots such as hats, glasses, watches and bracelets with read-back verification.");

            ImGui::SeparatorText("Saved Outfits");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##outfit_name", "Preset name", g_OutfitName, sizeof(g_OutfitName));
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Save Full Outfit", ImVec2(226.0f, 0.0f)))
            {
                if (toolkit.QueueSaveOutfit(g_OutfitName))
                    RefreshOutfits(toolkit);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Named", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueLoadOutfit(g_OutfitName));
            ImGui::EndDisabled();
            DescribeLastV11Item("Save or restore all 12 clothing components and 8 prop slots. Presets are model-specific and are rejected on a different ped model.");

            if (ImGui::Button("Refresh Saved Outfits", ImVec2(-1.0f, 0.0f)))
                RefreshOutfits(toolkit);
            if (!g_SavedOutfits.empty())
            {
                const char* preview = g_SelectedOutfit >= 0
                    ? g_SavedOutfits[static_cast<std::size_t>(g_SelectedOutfit)].c_str()
                    : "Select outfit";
                if (ImGui::BeginCombo("Saved Outfit", preview))
                {
                    for (std::size_t i = 0; i < g_SavedOutfits.size(); ++i)
                    {
                        const bool selected = g_SelectedOutfit == static_cast<int>(i);
                        if (ImGui::Selectable(g_SavedOutfits[i].c_str(), selected))
                        {
                            g_SelectedOutfit = static_cast<int>(i);
                            std::snprintf(g_OutfitName, sizeof(g_OutfitName), "%s", g_SavedOutfits[i].c_str());
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SeparatorText("Animation / Emotes");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##anim_dict", "Animation dictionary", g_AnimDictionary, sizeof(g_AnimDictionary));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##anim_name", "Animation name", g_AnimName, sizeof(g_AnimName));
            ImGui::Checkbox("Loop", &g_AnimLoop);
            ImGui::SameLine();
            ImGui::Checkbox("Upper body", &g_AnimUpperBody);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Play Animation", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueuePlayAnimation(g_AnimDictionary, g_AnimName, g_AnimLoop, g_AnimUpperBody));
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueStopAnimation());
            ImGui::EndDisabled();
            DescribeLastV11Item("Request an animation dictionary over GTA script ticks, play the named animation, or clear the local ped's active tasks.");
        }

        inline void RenderVehicleCamera(Game::NativeTools::EnhancedNativeToolkit& toolkit, const Game::NativeTools::ToolkitSnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Freecam");
            bool freecam = snapshot.freecamEnabled;
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Checkbox("Enable Scripted Freecam", &freecam))
                static_cast<void>(toolkit.SetFreecamEnabled(freecam));
            ImGui::EndDisabled();
            DescribeLastV11Item("Create, activate and render a dedicated Enhanced scripted camera, destroying it cleanly when disabled.");

            g_FreecamSpeed = toolkit.FreecamSpeed();
            if (ImGui::SliderFloat("Move Speed", &g_FreecamSpeed, 0.05f, 5.0f, "%.2f"))
                toolkit.SetFreecamSpeed(g_FreecamSpeed);
            g_FreecamFov = snapshot.freecamFov;
            if (ImGui::SliderFloat("Camera FOV", &g_FreecamFov, 20.0f, 120.0f, "%.0f"))
                toolkit.SetFreecamFov(g_FreecamFov);
            ImGui::TextDisabled("WASD move | Space/Ctrl vertical | Arrows rotate | Shift boost");
            if (snapshot.freecamEnabled)
            {
                ImGui::TextDisabled("XYZ %.1f %.1f %.1f | Pitch %.1f | Yaw %.1f",
                    snapshot.freecamX,
                    snapshot.freecamY,
                    snapshot.freecamZ,
                    snapshot.freecamPitch,
                    snapshot.freecamYaw);
            }

            ImGui::SeparatorText("Vehicle Controls");
            ImGui::SliderInt("Door Index", &g_VehicleDoor, 0, 7);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Open Door", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleDoor(g_VehicleDoor, true));
            ImGui::SameLine();
            if (ImGui::Button("Close Door", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleDoor(g_VehicleDoor, false));

            ImGui::SliderInt("Window Index", &g_VehicleWindow, 0, 7);
            if (ImGui::Button("Window Down", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleWindow(g_VehicleWindow, true));
            ImGui::SameLine();
            if (ImGui::Button("Window Up", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleWindow(g_VehicleWindow, false));

            if (ImGui::Button("Engine On", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleEngine(true));
            ImGui::SameLine();
            if (ImGui::Button("Engine Off", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleEngine(false));

            ImGui::SliderInt("Light State", &g_VehicleLights, 0, 3);
            if (ImGui::Button("Apply Lights", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueVehicleLights(g_VehicleLights));
            ImGui::EndDisabled();
            DescribeLastV11Item("Control the current vehicle's doors, windows, engine and light override through Enhanced vehicle natives.");

            ImGui::SeparatorText("Native Performance");
            bool changed = ImGui::Checkbox("Enable Performance Override", &g_PerformanceEnabled);
            changed = ImGui::SliderFloat("Top Speed Modifier", &g_TopSpeedModifier, -100.0f, 500.0f, "%.0f%%") || changed;
            changed = ImGui::SliderFloat("Torque Multiplier", &g_TorqueMultiplier, 0.1f, 10.0f, "%.2fx") || changed;
            changed = ImGui::Checkbox("Reduced Grip", &g_ReduceGrip) || changed;
            if (changed)
                toolkit.ConfigureVehiclePerformance(g_PerformanceEnabled, g_TopSpeedModifier, g_TorqueMultiplier, g_ReduceGrip);
            DescribeLastV11Item("Maintain reversible top-speed, torque and grip native overrides on the current vehicle. Defaults are restored when disabled or when you leave/switch vehicles.");
        }

        inline void RenderWorldTools(Game::NativeTools::EnhancedNativeToolkit& toolkit, const Game::NativeTools::ToolkitSnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "HUD / Blip Marker");
            ImGui::SliderInt("Blip Sprite", &g_BlipSprite, 1, 1000);
            ImGui::SliderInt("Blip Color", &g_BlipColor, 0, 100);
            ImGui::Checkbox("GPS Route", &g_BlipRoute);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Mark Current Position", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueAddPlayerBlip(g_BlipSprite, g_BlipColor, g_BlipRoute));
            ImGui::SameLine();
            if (ImGui::Button("Remove Marker", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueRemoveBlip());
            ImGui::EndDisabled();
            DescribeLastV11Item("Create a custom local map marker at your current position with configurable sprite, color and GPS route.");

            ImGui::SeparatorText("Particle FX");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##ptfx_asset", "Named PTFX asset", g_PtfxAsset, sizeof(g_PtfxAsset));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##ptfx_effect", "Looped effect name", g_PtfxEffect, sizeof(g_PtfxEffect));
            ImGui::SliderFloat("Effect Scale", &g_PtfxScale, 0.05f, 5.0f, "%.2f");
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Start Attached Effect", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueStartParticle(g_PtfxAsset, g_PtfxEffect, g_PtfxScale));
            ImGui::SameLine();
            if (ImGui::Button("Stop Effect", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueStopParticle());
            ImGui::EndDisabled();
            DescribeLastV11Item("Load a named PTFX asset over GTA script ticks and attach a looped particle effect to the local player.");

            ImGui::SeparatorText("Bodyguards");
            ImGui::Text("Active bodyguards: %zu / 4", snapshot.bodyguardCount);
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Spawn Bodyguard", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueSpawnBodyguard());
            if (ImGui::Button("Attack Aimed Target", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueBodyguardsAttackAimedPed());
            ImGui::SameLine();
            if (ImGui::Button("Dismiss All", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueDismissBodyguards());
            ImGui::EndDisabled();
            DescribeLastV11Item("Spawn up to four local companions in your player group. They follow you, inherit your selected weapon, and can be ordered to attack your aimed target.");

            ImGui::SeparatorText("Interior Manager");
            ImGui::Text("Current interior ID: %d", snapshot.currentInterior);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##interior_set", "Entity set name", g_InteriorSet, sizeof(g_InteriorSet));
            ImGui::SliderInt("Interior Set Color", &g_InteriorColor, 0, 15);
            ImGui::BeginDisabled(snapshot.pending || snapshot.currentInterior <= 0);
            if (ImGui::Button("Activate Set", ImVec2(155.0f, 0.0f)))
                static_cast<void>(toolkit.QueueInteriorEntitySet(g_InteriorSet, true, g_InteriorColor));
            ImGui::SameLine();
            if (ImGui::Button("Deactivate", ImVec2(145.0f, 0.0f)))
                static_cast<void>(toolkit.QueueInteriorEntitySet(g_InteriorSet, false, g_InteriorColor));
            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueRefreshInterior());
            ImGui::EndDisabled();
            DescribeLastV11Item("Activate/deactivate named interior entity sets, optionally change their color, and refresh the current interior.");

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##ipl_name", "IPL name", g_IplName, sizeof(g_IplName));
            ImGui::BeginDisabled(snapshot.pending);
            if (ImGui::Button("Request IPL", ImVec2(226.0f, 0.0f)))
                static_cast<void>(toolkit.QueueIpl(g_IplName, true));
            ImGui::SameLine();
            if (ImGui::Button("Remove IPL", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueIpl(g_IplName, false));
            ImGui::EndDisabled();
            DescribeLastV11Item("Request or remove a named IPL through the current Enhanced streaming path.");

            ImGui::Separator();
            ImGui::TextDisabled("Waypoint and map-destination teleporting already lives under World > Teleport and uses its dedicated ground-resolution runtime.");
        }

        inline void RenderDiagnostics(Game::NativeTools::EnhancedNativeToolkit& toolkit, const Game::NativeTools::ToolkitSnapshot& snapshot)
        {
            ImGui::TextColored(V11Theme::Accent, "Enhanced Native Diagnostics");
            ImGui::TextWrapped("Probe a current Enhanced hash without invoking it. Tutones resolves the slot through GTA's native table initializer and verifies that the resulting handler address is executable.");

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##native_probe_hash", "0x0123456789ABCDEF", g_ProbeHash, sizeof(g_ProbeHash));
            const std::uint64_t hash = ParseHash64(g_ProbeHash);
            ImGui::TextDisabled("Parsed hash: 0x%016llX", static_cast<unsigned long long>(hash));
            ImGui::BeginDisabled(snapshot.pending || hash == 0);
            if (ImGui::Button("Resolve Handler", ImVec2(-1.0f, 0.0f)))
                static_cast<void>(toolkit.QueueProbe(hash));
            ImGui::EndDisabled();
            DescribeLastV11Item("Resolve the entered Enhanced native hash to a handler pointer without executing the handler.");

            ImGui::SeparatorText("Last Probe");
            ImGui::Text("Hash: 0x%016llX", static_cast<unsigned long long>(snapshot.lastProbeHash));
            ImGui::Text("Resolved: %s", snapshot.lastProbeResolved ? "yes" : "no");
            ImGui::Text("Address: 0x%llX", static_cast<unsigned long long>(snapshot.lastProbeAddress));

            ImGui::SeparatorText("Runtime Inventory");
            ImGui::Text("Current weapon: 0x%08X | tint %d", snapshot.selectedWeapon, snapshot.weaponTint);
            ImGui::Text("Freecam: %s | handle %d", snapshot.freecamEnabled ? "active" : "off", snapshot.freecamHandle);
            ImGui::Text("Vehicle performance: %s", snapshot.vehiclePerformanceEnabled ? "active" : "off");
            ImGui::Text("Custom blip: %d", snapshot.activeBlip);
            ImGui::Text("Particle handle: %d", snapshot.activeParticle);
            ImGui::Text("Bodyguards: %zu", snapshot.bodyguardCount);
            ImGui::Text("Current interior: %d", snapshot.currentInterior);

            ImGui::Spacing();
            ImGui::TextWrapped("Toolkit handlers resolve lazily. A GTA update can therefore make one feature unavailable without forcing the entire Native Tools backend offline.");
        }
    }

    inline void RenderNativeToolsPanel(std::size_t subtab) noexcept
    {
        using namespace NativeToolsPanelDetail;
        auto& toolkit = Game::NativeTools::EnhancedNativeToolkit::Get();
        toolkit.RequestSample();
        const auto snapshot = toolkit.Snapshot();

        constexpr std::array<const char*, 4> Names{{
            "Workshop", "Vehicle & Camera", "World Tools", "Diagnostics",
        }};
        const std::size_t index = std::min<std::size_t>(subtab, Names.size() - 1);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##native_tools_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Native Tools");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Names[index]);
            ImGui::Separator();

            if (!snapshot.nativeReady)
            {
                ImGui::TextDisabled("Enhanced native runtime is not ready yet.");
            }
            else if (index == 0)
            {
                RenderWorkshop(toolkit, snapshot);
            }
            else if (index == 1)
            {
                RenderVehicleCamera(toolkit, snapshot);
            }
            else if (index == 2)
            {
                RenderWorldTools(toolkit, snapshot);
            }
            else
            {
                RenderDiagnostics(toolkit, snapshot);
            }

            RenderStatus(snapshot);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
