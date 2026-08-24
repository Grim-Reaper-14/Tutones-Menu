#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/network/ProtectionRuntime.hpp"

#include <imgui.h>

#include <cstddef>

namespace Tutones::UI
{
    inline void RenderProtectionPanel(std::size_t subtab) noexcept
    {
        using Game::Protections::ProtectionRuntime;
        auto& runtime = ProtectionRuntime::Get();
        static_cast<void>(runtime.Start());
        auto snapshot = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##protections_v12_panel", ImVec2(780.0f, 500.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "PROTECTIONS");
            ImGui::SameLine();
            ImGui::TextDisabled(subtab == 0 ? "OVERVIEW" : subtab == 1 ? "NETWORK EVENTS" : "SCRIPT EVENTS");
            ImGui::TextDisabled("Live Enhanced packet and event protection controls.");
            ImGui::Separator();

            if (subtab == 0)
            {
                if (ImGui::BeginTable("##protection_overview_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##protection_runtime_card", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "RUNTIME");
                        ImGui::Separator();
                        ImGui::Text("Backend");
                        ImGui::SameLine(180.0f);
                        ImGui::TextColored(snapshot.installed ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                            "%s", snapshot.installed ? "ACTIVE" : "UNAVAILABLE");
                        ImGui::Spacing();
                        ImGui::TextWrapped("%s", snapshot.status.c_str());
                        ImGui::Spacing();
                        ImGui::SeparatorText("Default Policy");
                        ImGui::TextWrapped("Malformed packet and malformed scripted-event validation stay enabled by default. Broad event blocks remain opt-in to avoid breaking legitimate GTA Online activity.");
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##protection_counters_card", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "TELEMETRY");
                        ImGui::Separator();
                        ImGui::Text("Packets inspected");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("%llu", static_cast<unsigned long long>(snapshot.packetsInspected));
                        ImGui::Text("Packets blocked");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("%llu", static_cast<unsigned long long>(snapshot.packetsBlocked));
                        ImGui::Text("Events inspected");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("%llu", static_cast<unsigned long long>(snapshot.eventsInspected));
                        ImGui::Text("Events blocked");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("%llu", static_cast<unsigned long long>(snapshot.eventsBlocked));

                        ImGui::Spacing();
                        ImGui::SeparatorText("Last Block");
                        if (snapshot.lastBlockedEvent >= 0)
                            ImGui::Text("Event ID: %d", snapshot.lastBlockedEvent);
                        else if (snapshot.lastBlockedEvent == -2)
                            ImGui::Text("Malformed packet");
                        else
                            ImGui::TextDisabled("Nothing blocked this session.");

                        ImGui::Spacing();
                        if (ImGui::Button("RESET COUNTERS", ImVec2(-1.0f, 36.0f)))
                            runtime.ResetCounters();
                        DescribeLastV11Item("Clear Tutones protection telemetry without changing any filter setting.");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
            else if (subtab == 1)
            {
                if (ImGui::BeginTable("##network_protection_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##network_core_filters", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "CORE FILTERS");
                        ImGui::Separator();

                        bool malformed = snapshot.blockMalformed;
                        if (ImGui::Checkbox("Malformed Packets", &malformed)) runtime.SetBlockMalformed(malformed);
                        DescribeLastV11Item("Reject invalid message headers, impossible packet lengths and malformed PackedEvents data before GTA processes them.");

                        bool sounds = snapshot.blockSounds;
                        if (ImGui::Checkbox("Network Sound Events", &sounds)) runtime.SetBlockSounds(sounds);
                        DescribeLastV11Item("Block NETWORK_PLAY_SOUND_EVENT traffic.");

                        bool explosions = snapshot.blockExplosions;
                        if (ImGui::Checkbox("Explosion Events", &explosions)) runtime.SetBlockExplosions(explosions);

                        bool fire = snapshot.blockFire;
                        if (ImGui::Checkbox("Fire Events", &fire)) runtime.SetBlockFire(fire);
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##network_gameplay_filters", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "GAMEPLAY FILTERS");
                        ImGui::Separator();

                        bool weapon = snapshot.blockWeaponDamage;
                        if (ImGui::Checkbox("Weapon Damage Events", &weapon)) runtime.SetBlockWeaponDamage(weapon);

                        bool ragdoll = snapshot.blockRagdoll;
                        if (ImGui::Checkbox("Ragdoll Requests", &ragdoll)) runtime.SetBlockRagdoll(ragdoll);

                        bool clearTasks = snapshot.blockClearTasks;
                        if (ImGui::Checkbox("Clear Ped Tasks", &clearTasks)) runtime.SetBlockClearTasks(clearTasks);

                        bool ptfx = snapshot.blockPtfx;
                        if (ImGui::Checkbox("Network PTFX", &ptfx)) runtime.SetBlockPtfx(ptfx);

                        ImGui::Spacing();
                        ImGui::SeparatorText("Behavior");
                        ImGui::TextWrapped("Optional event filters reject the containing PackedEvents packet when matched. Keep broad filters off unless you specifically need them.");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
            else
            {
                if (ImGui::BeginChild("##script_event_protections_v12", ImVec2(0.0f, 352.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "SCRIPT EVENT POLICY");
                    ImGui::TextDisabled("Validation first; aggressive blocking is optional.");
                    ImGui::Separator();

                    bool malformedScript = snapshot.blockMalformedScriptEvents;
                    if (ImGui::Checkbox("Block Malformed Script Events", &malformedScript))
                        runtime.SetBlockMalformedScriptEvents(malformedScript);
                    DescribeLastV11Item("Validate CScriptedGameEvent argument byte size against the supported argument capacity before GTA sees it.");

                    bool scriptEvents = snapshot.blockScriptEvents;
                    if (ImGui::Checkbox("Block All Scripted Game Events", &scriptEvents))
                        runtime.SetBlockScriptEvents(scriptEvents);
                    DescribeLastV11Item("Aggressive mode. Reject any PackedEvents packet containing SCRIPTED_GAME_EVENT. Leave off for normal missions and freemode activity.");

                    ImGui::Spacing();
                    ImGui::SeparatorText("Important");
                    ImGui::TextWrapped("Malformed scripted-event validation is enabled by default. Full script-event blocking can interfere with legitimate GTA Online activities, so it stays opt-in.");
                }
                ImGui::EndChild();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
