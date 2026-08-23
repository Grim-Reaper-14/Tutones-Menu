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
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##protections_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Protections");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", subtab == 0 ? "Overview" : subtab == 1 ? "Network Events" : "Script Events");
            ImGui::Separator();

            if (subtab == 0)
            {
                ImGui::Text("Backend: %s", snapshot.installed ? "ACTIVE" : "UNAVAILABLE");
                ImGui::TextWrapped("%s", snapshot.status.c_str());
                ImGui::Spacing();
                ImGui::Text("Packets inspected: %llu", static_cast<unsigned long long>(snapshot.packetsInspected));
                ImGui::Text("Packets blocked:   %llu", static_cast<unsigned long long>(snapshot.packetsBlocked));
                ImGui::Text("Events inspected:  %llu", static_cast<unsigned long long>(snapshot.eventsInspected));
                ImGui::Text("Events blocked:    %llu", static_cast<unsigned long long>(snapshot.eventsBlocked));
                if (snapshot.lastBlockedEvent >= 0)
                    ImGui::Text("Last blocked event ID: %d", snapshot.lastBlockedEvent);
                else if (snapshot.lastBlockedEvent == -2)
                    ImGui::Text("Last blocked: malformed packet");
                else
                    ImGui::TextDisabled("No packet has been blocked this session.");

                if (ImGui::Button("Reset Protection Counters", ImVec2(-1.0f, 0.0f)))
                    runtime.ResetCounters();
                DescribeLastV11Item("Clear Tutones protection telemetry without changing any filter setting.");

                ImGui::Spacing();
                ImGui::TextWrapped("The backend hooks GTA Enhanced ReceiveNetMessage and parses the same 0x3246/PackedEvents framing used by YimMenuV2. Broad filters are opt-in so legitimate Online traffic is not blocked by default.");
            }
            else if (subtab == 1)
            {
                bool malformed = snapshot.blockMalformed;
                if (ImGui::Checkbox("Block Malformed Packets", &malformed)) runtime.SetBlockMalformed(malformed);
                DescribeLastV11Item("Reject invalid message headers, impossible packet lengths and malformed PackedEvents data before GTA processes them.");

                bool sounds = snapshot.blockSounds;
                if (ImGui::Checkbox("Block Network Sound Events", &sounds)) runtime.SetBlockSounds(sounds);
                DescribeLastV11Item("Blocks NETWORK_PLAY_SOUND_EVENT. YimMenuV2 currently rejects this event in its net-event handler.");

                bool explosions = snapshot.blockExplosions;
                if (ImGui::Checkbox("Block Explosion Events", &explosions)) runtime.SetBlockExplosions(explosions);
                bool fire = snapshot.blockFire;
                if (ImGui::Checkbox("Block Fire Events", &fire)) runtime.SetBlockFire(fire);
                bool weapon = snapshot.blockWeaponDamage;
                if (ImGui::Checkbox("Block Weapon Damage Events", &weapon)) runtime.SetBlockWeaponDamage(weapon);
                bool ragdoll = snapshot.blockRagdoll;
                if (ImGui::Checkbox("Block Ragdoll Requests", &ragdoll)) runtime.SetBlockRagdoll(ragdoll);
                bool clearTasks = snapshot.blockClearTasks;
                if (ImGui::Checkbox("Block Clear Ped Tasks", &clearTasks)) runtime.SetBlockClearTasks(clearTasks);
                bool ptfx = snapshot.blockPtfx;
                if (ImGui::Checkbox("Block Network PTFX", &ptfx)) runtime.SetBlockPtfx(ptfx);

                ImGui::Spacing();
                ImGui::TextDisabled("Optional event filters reject the containing PackedEvents packet when matched.");
            }
            else
            {
                bool malformedScript = snapshot.blockMalformedScriptEvents;
                if (ImGui::Checkbox("Block Malformed Script Events", &malformedScript)) runtime.SetBlockMalformedScriptEvents(malformedScript);
                DescribeLastV11Item("Validates CScriptedGameEvent argument byte size against YimMenuV2's 54-int64 argument capacity before GTA sees it.");

                bool scriptEvents = snapshot.blockScriptEvents;
                if (ImGui::Checkbox("Block All Scripted Game Events", &scriptEvents)) runtime.SetBlockScriptEvents(scriptEvents);
                DescribeLastV11Item("Aggressive mode. Rejects any PackedEvents packet containing SCRIPTED_GAME_EVENT. Leave off for normal missions and freemode activity.");

                ImGui::Spacing();
                ImGui::TextWrapped("Malformed scripted-event validation is enabled by default. Full script-event blocking is intentionally opt-in because legitimate GTA Online activities rely on scripted events.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
