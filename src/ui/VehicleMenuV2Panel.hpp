#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/network/RequestServicesRuntime.hpp"
#include "../features/vehicle/DlcVehicleRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Tutones::UI
{
    namespace VehicleMenuV2Detail
    {
        inline int g_CustomizeMode{}; // 0 = paint, 1 = workshop
        inline int g_GarageMode{};    // 0 = Rockstar, 1 = Tutones saved, 2 = service vehicles

        inline char g_PresetName[48] = "my_vehicle";
        inline bool g_LoadPresetInside{true};
        inline int g_SelectedPreset{-1};
        inline bool g_SavedGarageLoaded{};
        inline std::vector<std::string> g_SavedPresets{};
        inline std::string g_SavedGarageMessage{"Ready"};

        inline void RefreshSavedGarage(Game::Mods::VehicleModificationRuntime& runtime)
        {
            g_SavedPresets = runtime.SavedPresetNames();
            if (g_SavedPresets.empty())
                g_SelectedPreset = -1;
            else
                g_SelectedPreset = std::clamp(g_SelectedPreset, 0, static_cast<int>(g_SavedPresets.size()) - 1);
            g_SavedGarageLoaded = true;
        }

        inline bool SectionButton(const char* label, bool selected, const ImVec2& size) noexcept
        {
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::AccentDark);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V11Theme::AccentDark);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, V11Theme::Accent);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, V11Theme::PanelBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V11Theme::ControlHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, V11Theme::AccentDark);
            }

            const bool pressed = ImGui::Button(label, size);
            ImGui::PopStyleColor(3);
            return pressed;
        }

        inline void DrawVehicleScriptFeatures() noexcept
        {
            auto& dlcRuntime = Game::VehicleFeatures::DlcVehicleRuntime::Get();
            const auto state = dlcRuntime.Snapshot();

            ImGui::SetCursorPos(ImVec2(724.0f, 330.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_script_features", ImVec2(284.0f, 168.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "DECOMPILED SCRIPTS");
                ImGui::TextDisabled("Enhanced vehicle website flow");
                ImGui::Separator();

                bool enabled = state.enabled;
                ImGui::BeginDisabled(!state.running);
                if (ImGui::Checkbox("Enable All DLC Vehicles", &enabled))
                    dlcRuntime.SetEnabled(enabled);
                ImGui::EndDisabled();
                DescribeLastV11Item("Patch the current Enhanced appinternet vehicle availability, price and purchase checks in the script shadow so supported DLC vehicles remain available on GTA's vehicle websites.");

                const bool allSupported = state.vehicleAvailabilitySupported
                    && state.priceGateSupported
                    && state.purchaseGateSupported;
                ImGui::TextDisabled("ScriptVM hook: %s", state.hookActive ? "READY" : "WAITING");
                ImGui::TextDisabled("appinternet: %s", state.programLoaded ? "LOADED" : "OPEN A VEHICLE WEBSITE");
                ImGui::TextDisabled("3 script checks: %s", allSupported ? "RESOLVED" : "WAITING");
                ImGui::TextColored(
                    state.applied ? ImVec4(0.20f, 0.86f, 0.38f, 1.0f) : V11Theme::MutedText,
                    "State: %s",
                    state.applied ? "APPLIED" : (state.enabled ? "ARMED" : "OFF"));
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        inline void DrawCustomizeSelector() noexcept
        {
            ImGui::SetCursorPos(ImVec2(724.0f, 330.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_customize_selector", ImVec2(284.0f, 168.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "CUSTOMIZATION");
                ImGui::TextDisabled("Choose what you want to edit");
                ImGui::Separator();

                if (SectionButton("Paint & Colors", g_CustomizeMode == 0, ImVec2(-1.0f, 31.0f)))
                    g_CustomizeMode = 0;
                DescribeLastV11Item("Primary and secondary paint, indexed color families, custom RGB, pearlescent and wheel color.");

                if (SectionButton("Mods / Wheels / Lights", g_CustomizeMode == 1, ImVec2(-1.0f, 31.0f)))
                    g_CustomizeMode = 1;
                DescribeLastV11Item("Body and performance modification slots, wheel styles, xenon, neon, tire smoke and tire behavior.");

                ImGui::Spacing();
                ImGui::TextDisabled(g_CustomizeMode == 0
                    ? "Paint stays inside Customization."
                    : "Workshop stays inside Customization.");
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        inline void DrawGarageSelector() noexcept
        {
            ImGui::SetCursorPos(ImVec2(724.0f, 330.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_garage_selector", ImVec2(284.0f, 214.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "GARAGE");
                ImGui::TextDisabled("Stored and requested vehicles");
                ImGui::Separator();

                if (SectionButton("Rockstar Personal Garage", g_GarageMode == 0, ImVec2(-1.0f, 31.0f)))
                    g_GarageMode = 0;
                DescribeLastV11Item("Browse owned GTA Online garage vehicles, request or repair them, save the current vehicle to a Rockstar garage, return the active personal vehicle to storage, or teleport into it.");

                if (SectionButton("Tutones Saved Vehicles", g_GarageMode == 1, ImVec2(-1.0f, 31.0f)))
                    g_GarageMode = 1;
                DescribeLastV11Item("Manage local Tutones full-vehicle presets without mixing them into current-vehicle or customization controls.");

                if (SectionButton("Service Vehicles", g_GarageMode == 2, ImVec2(-1.0f, 31.0f)))
                    g_GarageMode = 2;
                DescribeLastV11Item("Request Enhanced service vehicles through the Freemode Global_2733326 request states verified in the current decompiled scripts.");

                ImGui::Spacing();
                ImGui::TextDisabled("Garage and vehicle-delivery features stay here.");
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        inline void RenderSavedGaragePanel() noexcept
        {
            auto& runtime = Game::Mods::VehicleModificationRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            if (!g_SavedGarageLoaded)
                RefreshSavedGarage(runtime);

            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_saved_garage_v2", ImVec2(490.0f, 430.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Tutones Saved Vehicles");
                ImGui::SameLine();
                ImGui::TextDisabled("Local garage presets");
                ImGui::Separator();

                if (!runtime.IsRunning())
                {
                    ImGui::TextDisabled("Vehicle runtime is offline.");
                }
                else
                {
                    ImGui::TextWrapped("Local saved vehicles live here beside Rockstar personal-garage tools, but remain separate from Rockstar Online garage data.");
                    ImGui::SeparatorText("Save Current Vehicle");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextWithHint("##saved_garage_name", "Preset name", g_PresetName, sizeof(g_PresetName));
                    DescribeLastV11Item("Choose the local Tutones preset name for the current vehicle and its supported customization state.");

                    if (ImGui::Button("Save Current Vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueSaveCurrentPreset(g_PresetName);
                        g_SavedGarageMessage = queued
                            ? "Vehicle preset queued for save"
                            : "Save rejected - enter a vehicle first";
                    }
                    DescribeLastV11Item("Save the current vehicle as a local Tutones garage preset without changing Rockstar personal-garage data.");

                    ImGui::SeparatorText("Saved Vehicles");
                    if (ImGui::Button("Refresh Saved Vehicles", ImVec2(-1.0f, 0.0f)))
                        RefreshSavedGarage(runtime);
                    DescribeLastV11Item("Refresh the local Tutones saved-vehicle preset list.");

                    if (g_SavedPresets.empty())
                    {
                        ImGui::TextDisabled("No local saved vehicles yet.");
                    }
                    else if (ImGui::BeginListBox("##saved_garage_list", ImVec2(-1.0f, 130.0f)))
                    {
                        for (std::size_t i = 0; i < g_SavedPresets.size(); ++i)
                        {
                            const bool selected = g_SelectedPreset == static_cast<int>(i);
                            if (ImGui::Selectable(g_SavedPresets[i].c_str(), selected))
                                g_SelectedPreset = static_cast<int>(i);
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndListBox();
                    }

                    ImGui::Checkbox("Spawn inside saved vehicle", &g_LoadPresetInside);
                    DescribeLastV11Item("Enter the spawned saved vehicle automatically after its preset is restored.");

                    ImGui::BeginDisabled(g_SelectedPreset < 0 || g_SelectedPreset >= static_cast<int>(g_SavedPresets.size()));
                    if (ImGui::Button("Spawn Selected Saved Vehicle", ImVec2(-1.0f, 0.0f)))
                    {
                        const bool queued = runtime.QueueLoadPreset(
                            g_SavedPresets[static_cast<std::size_t>(g_SelectedPreset)],
                            g_LoadPresetInside);
                        g_SavedGarageMessage = queued ? "Saved vehicle queued" : "Saved vehicle load rejected";
                    }
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Spawn the selected Tutones garage preset and restore its supported paint, wheels, modifications, lighting and tire state.");

                    if (!snapshot.lastSavedPreset.empty())
                        ImGui::Text("Last preset: %s", snapshot.lastSavedPreset.c_str());
                    ImGui::TextDisabled("%s", g_SavedGarageMessage.c_str());
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        inline void RenderServiceVehiclesPanel() noexcept
        {
            using Game::NetworkFeatures::RequestService;
            using Game::NetworkFeatures::RequestServicesRuntime;

            auto& runtime = RequestServicesRuntime::Get();
            const auto snapshot = runtime.Snapshot();

            ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

            if (ImGui::BeginChild("##vehicle_service_garage_v2", ImVec2(490.0f, 430.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "Service Vehicles");
                ImGui::SameLine();
                ImGui::TextDisabled("Enhanced Freemode requests");
                ImGui::Separator();
                ImGui::TextWrapped("These requests use Global_2733326 states consumed by the current Enhanced freemode script. They stay in Garage because they deliver or manage vehicles rather than general Online services.");

                struct ServiceButton final
                {
                    const char* label;
                    RequestService service;
                    const char* source;
                };

                constexpr ServiceButton services[] = {
                    {"Request MOC", RequestService::MOC, "Decompile: Global_2733326.f_577"},
                    {"Request Avenger", RequestService::Avenger, "Decompile: Global_2733326.f_585"},
                    {"Request Terrorbyte", RequestService::Terrorbyte, "Decompile: Global_2733326.f_591"},
                    {"Request Kosatka", RequestService::Kosatka, "Decompile: Global_2733326.f_613"},
                    {"Request Dinghy", RequestService::Dinghy, "Decompile: Global_2733326.f_626"},
                    {"Request Acid Lab", RequestService::AcidLab, "Decompile: Global_2733326.f_592"},
                    {"Request Acid Lab Bike", RequestService::AcidLabBike, "Decompile: Global_2733326.f_648"},
                    {"Request Bail Transporter", RequestService::BailOfficeTransporter, "Enhanced request state: Global_2733326.f_362"},
                };

                ImGui::SeparatorText("Owned / Service Vehicle Delivery");
                ImGui::BeginDisabled(snapshot.pending);
                if (ImGui::BeginTable("##service_vehicle_requests", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    for (std::size_t i = 0; i < std::size(services); ++i)
                    {
                        if ((i % 2) == 0)
                            ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(static_cast<int>(i % 2));
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Button(services[i].label, ImVec2(-1.0f, 30.0f)))
                            static_cast<void>(runtime.QueueRequest(services[i].service));
                        DescribeLastV11Item(services[i].source);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();

                ImGui::SeparatorText("Script Status");
                ImGui::Text("Action: %s", snapshot.pending ? "PENDING" : (snapshot.haveResult ? (snapshot.lastSucceeded ? "SUCCESS" : "FAILED") : "READY"));
                ImGui::TextWrapped("%s", snapshot.message.c_str());
                ImGui::Spacing();
                ImGui::TextDisabled("Freemode decompile verifies the MOC / Avenger / Terrorbyte / Kosatka / Dinghy / Acid Lab request-state family.");
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }

    inline void RenderVehicleHubV2() noexcept
    {
        RenderV12VehicleGeneralPanel();
        VehicleMenuV2Detail::DrawVehicleScriptFeatures();
        SetV11Description("Vehicle Hub - spawn/current/clone controls plus verified Enhanced decompiled-script vehicle features. Website DLC availability is kept here, while paint stays in Customization and all stored-vehicle actions stay in Garage.");
    }

    inline void RenderVehicleCustomizeV2() noexcept
    {
        using namespace VehicleMenuV2Detail;
        if (g_CustomizeMode == 0)
            RenderV12VehiclePaintPanel();
        else
            RenderV12VehicleModificationPanel();

        DrawCustomizeSelector();

        SetV11Description(g_CustomizeMode == 0
            ? "Vehicle Customization - paint and color controls are grouped here instead of living as a separate top-level vehicle menu."
            : "Vehicle Customization - LSC modifications, wheels, lighting, neon, tire smoke and tire behavior share the same customization workspace.");
    }

    inline void RenderVehicleGarageV2() noexcept
    {
        using namespace VehicleMenuV2Detail;
        if (g_GarageMode == 0)
            RenderV12PersonalVehiclePanel();
        else if (g_GarageMode == 1)
            RenderV12PageSurface([] { RenderSavedGaragePanel(); });
        else
            RenderV12PageSurface([] { RenderServiceVehiclesPanel(); });

        DrawGarageSelector();

        if (g_GarageMode == 0)
        {
            SetV11Description("Vehicle Garage - Rockstar MPSV/Freemode-backed personal vehicles, request, return-to-storage, repair-all, save-current and teleport actions stay together here.");
        }
        else if (g_GarageMode == 1)
        {
            SetV11Description("Vehicle Garage - local Tutones saved vehicle presets stay with the rest of the garage tools instead of being mixed into the current-vehicle page.");
        }
        else
        {
            SetV11Description("Vehicle Garage - decompile-backed Enhanced Freemode service-vehicle requests for MOC, Avenger, Terrorbyte, Kosatka, Dinghy, Acid Lab, Acid Lab Bike and Bail Office Transporter.");
        }
    }
}
