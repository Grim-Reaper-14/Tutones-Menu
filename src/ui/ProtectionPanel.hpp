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
        const auto snapshot = runtime.Snapshot();

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
            ImGui::TextDisabled(subtab == 0 ? "SESSION SAFETY" : subtab == 1 ? "NETWORK POLICY" : "SCRIPT POLICY");
            ImGui::TextDisabled("Passive Enhanced receive monitor. Tutones does not drop session packets in this build.");
            ImGui::Separator();

            if (subtab == 0)
            {
                if (ImGui::BeginTable("##protection_overview_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##protection_runtime_card", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "SESSION SAFE MODE");
                        ImGui::Separator();
                        ImGui::Text("Receive hook");
                        ImGui::SameLine(180.0f);
                        ImGui::TextColored(snapshot.installed ? ImVec4(0.20f, 0.88f, 0.42f, 1.0f) : V11Theme::MutedText,
                            "%s", snapshot.installed ? "PASS-THROUGH" : "UNAVAILABLE");
                        ImGui::Spacing();
                        ImGui::TextWrapped("%s", snapshot.status.c_str());

                        ImGui::Spacing();
                        ImGui::SeparatorText("Why this changed");
                        ImGui::TextWrapped(
                            "The previous PackedEvents parser could reject traffic before GTA processed it. "
                            "That can make the local client lose session synchronization and make every other player appear to leave at once.");

                        ImGui::Spacing();
                        ImGui::TextWrapped(
                            "This build always calls GTA's original ReceiveNetMessage handler. Host migration, session synchronization, "
                            "Rockstar networking and anti-cheat-related transport are not suppressed by Tutones.");

                        ImGui::Spacing();
                        ImGui::SeparatorText("Protection policy");
                        ImGui::TextWrapped(
                            "Saved protection preferences are retained in Settings, but active packet/event blocking is suspended while "
                            "the Enhanced parser is being validated against live traffic.");
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##protection_counters_card", ImVec2(0.0f, 352.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "TELEMETRY");
                        ImGui::Separator();
                        ImGui::Text("Frames observed");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("%llu", static_cast<unsigned long long>(snapshot.packetsInspected));
                        ImGui::Text("Frames blocked");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("0");
                        ImGui::Text("Events blocked");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("0");
                        ImGui::Text("Direct kicks blocked");
                        ImGui::SameLine(190.0f);
                        ImGui::Text("0");

                        ImGui::Spacing();
                        ImGui::SeparatorText("Session invariant");
                        ImGui::TextWrapped(
                            "Tutones must never be the component that removes a legitimate inbound network frame while pass-through mode is active.");

                        ImGui::Spacing();
                        if (ImGui::Button("RESET COUNTERS", ImVec2(-1.0f, 36.0f)))
                            runtime.ResetCounters();
                        DescribeLastV11Item("Reset passive receive telemetry. This does not alter the session or any protection preference.");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
            else if (subtab == 1)
            {
                if (ImGui::BeginChild("##network_policy_passive", ImVec2(0.0f, 352.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "NETWORK FILTER PREFERENCES");
                    ImGui::TextDisabled("Stored for later validation; not enforced in session-safe pass-through mode.");
                    ImGui::Separator();

                    ImGui::BeginDisabled(true);
                    bool forcedLeave = snapshot.blockForcedLeave;
                    ImGui::Checkbox("Aggressive Direct Kick Blocking", &forcedLeave);
                    bool knownCrashes = snapshot.blockKnownCrashes;
                    ImGui::Checkbox("Known Crash Protection", &knownCrashes);
                    bool malformed = snapshot.blockMalformed;
                    ImGui::Checkbox("Malformed Packets", &malformed);
                    bool sounds = snapshot.blockSounds;
                    ImGui::Checkbox("Block All Network Sound Events", &sounds);
                    bool explosions = snapshot.blockExplosions;
                    ImGui::Checkbox("Block All Explosion Events", &explosions);
                    bool fire = snapshot.blockFire;
                    ImGui::Checkbox("Block All Fire Events", &fire);
                    bool weapon = snapshot.blockWeaponDamage;
                    ImGui::Checkbox("Weapon Damage Events", &weapon);
                    bool ragdoll = snapshot.blockRagdoll;
                    ImGui::Checkbox("Ragdoll Requests", &ragdoll);
                    bool clearTasks = snapshot.blockClearTasks;
                    ImGui::Checkbox("Clear Ped Tasks", &clearTasks);
                    bool ptfx = snapshot.blockPtfx;
                    ImGui::Checkbox("Network PTFX", &ptfx);
                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "No receive-side packet is discarded by these options in this build. This is intentional so we can verify whether lobby-emptying stops with Tutones fully fail-open.");
                }
                ImGui::EndChild();
            }
            else
            {
                if (ImGui::BeginChild("##script_event_protections_v12", ImVec2(0.0f, 352.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "SCRIPT EVENT POLICY");
                    ImGui::TextDisabled("Stored preferences only while session-safe pass-through mode is active.");
                    ImGui::Separator();

                    ImGui::BeginDisabled(true);
                    bool malformedScript = snapshot.blockMalformedScriptEvents;
                    ImGui::Checkbox("Block Malformed Script Events", &malformedScript);
                    bool scriptEvents = snapshot.blockScriptEvents;
                    ImGui::Checkbox("Block All Scripted Game Events", &scriptEvents);
                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::SeparatorText("Important");
                    ImGui::TextWrapped(
                        "GTA receives scripted game events unchanged in this build. If the lobby still empties with this policy, the cause is outside Tutones' receive filter and we can move the investigation to connection/session or BattlEye behavior.");
                }
                ImGui::EndChild();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
