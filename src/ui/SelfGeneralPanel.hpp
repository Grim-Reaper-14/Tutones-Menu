#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/player/SelfStatEditorRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    inline void RenderSelfGeneralPanel() noexcept
    {
        using Game::PlayerFeatures::OffRadarRuntime;
        using Game::PlayerFeatures::PlayerRuntime;
        using Game::PlayerFeatures::SelfStatEditorRuntime;

        auto& playerRuntime = PlayerRuntime::Get();
        auto& offRadarRuntime = OffRadarRuntime::Get();
        auto& statRuntime = SelfStatEditorRuntime::Get();

        const auto player = playerRuntime.Snapshot();
        const auto offRadar = offRadarRuntime.Snapshot();
        const auto stats = statRuntime.Snapshot();

        static bool initialized{};
        static int rank{};
        static int rp{};
        static int kills{};
        static int deaths{};
        static int radarMode{};
        static int semiGodMode{};
        static const char* message = "Ready";

        if (!initialized && stats.readable)
        {
            rank = stats.rank;
            rp = stats.rp;
            kills = stats.kills;
            deaths = stats.deaths;
            initialized = true;
        }

        bool godMode = player.invincible;
        bool neverWanted = player.neverWanted;
        bool superRun = player.runMultiplier > 1.01f;
        bool superJump = player.superJump;
        bool noRagdoll = player.noRagdoll;

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##self_v2_general", ImVec2(780.0f, 500.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::TextColored(V11Theme::Accent, "SELF");
            ImGui::SameLine();
            ImGui::TextDisabled("GENERAL");
            ImGui::TextDisabled("Player options, stat editor and quick actions mirrored from the V2 concept.");
            ImGui::Separator();

            if (ImGui::BeginTable("##self_v2_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##self_player_options", ImVec2(0.0f, 350.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "PLAYER OPTIONS");
                    ImGui::Separator();

                    if (ImGui::Checkbox("God Mode", &godMode))
                        playerRuntime.SetInvincible(godMode);
                    DescribeLastV11Item("Maintain full invincibility on the local player while alive.");

                    if (ImGui::Checkbox("Never Wanted", &neverWanted))
                        playerRuntime.SetNeverWanted(neverWanted);
                    DescribeLastV11Item("Continuously clear the local player's wanted level.");

                    if (ImGui::Checkbox("Super Run", &superRun))
                        playerRuntime.SetRunMultiplier(superRun ? 1.49f : 1.0f);
                    DescribeLastV11Item("Toggle the supported local run/sprint multiplier between normal and maximum supported speed.");

                    if (ImGui::Checkbox("Super Jump", &superJump))
                        playerRuntime.SetSuperJump(superJump);
                    DescribeLastV11Item("Maintain GTA's super-jump state for the local player.");

                    if (ImGui::Checkbox("No Ragdoll", &noRagdoll))
                        playerRuntime.SetNoRagdoll(noRagdoll);
                    DescribeLastV11Item("Prevent normal local-player ragdoll states while enabled.");

                    ImGui::Spacing();
                    ImGui::TextUnformatted("Off The Radar");
                    ImGui::SetNextItemWidth(-1.0f);
                    const char* radarModes[] = {"Standard Off Radar", "Ghost Organization"};
                    if (ImGui::Combo("##self_radar_mode", &radarMode, radarModes, IM_ARRAYSIZE(radarModes)))
                    {
                        if (radarMode == 0)
                        {
                            offRadarRuntime.SetEnabled(true);
                            message = "Standard Off Radar enabled";
                        }
                        else
                        {
                            offRadarRuntime.SetEnabled(false);
                            message = "Ghost Organization selected; backend not verified yet";
                        }
                    }
                    DescribeLastV11Item("Choose Standard Off Radar or the separate Ghost Organization mode. Standard Off Radar uses the verified Freemode backend; Ghost Organization remains distinct until its Enhanced backend is verified.");

                    ImGui::TextUnformatted("Semi God Mode");
                    ImGui::SetNextItemWidth(-1.0f);
                    const char* damageModes[] = {"Normal", "Semi", "Full"};
                    if (ImGui::Combo("##self_semi_god", &semiGodMode, damageModes, IM_ARRAYSIZE(damageModes)))
                    {
                        if (semiGodMode == 0)
                        {
                            playerRuntime.SetInvincible(false);
                            playerRuntime.SetBulletproof(false);
                        }
                        else if (semiGodMode == 1)
                        {
                            playerRuntime.SetInvincible(false);
                            playerRuntime.SetBulletproof(true);
                        }
                        else
                        {
                            playerRuntime.SetBulletproof(false);
                            playerRuntime.SetInvincible(true);
                        }
                    }
                    DescribeLastV11Item("Normal restores standard damage, Semi applies bulletproof protection without full invincibility, and Full enables God Mode.");

                    ImGui::Spacing();
                    if (ImGui::Button("Clean Player", ImVec2(-1.0f, 32.0f)))
                        message = playerRuntime.QueueClearDamage() ? "Clean Player queued" : "Clean Player rejected";
                    if (ImGui::Button("Suicide", ImVec2(-1.0f, 32.0f)))
                        message = playerRuntime.QueueSuicide() ? "Suicide queued" : "Suicide rejected";
                }
                ImGui::EndChild();

                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##self_stats_editor", ImVec2(0.0f, 350.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "STATS EDITOR");
                    ImGui::Separator();

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputInt("Rank", &rank);
                    rank = std::clamp(rank, 0, 8000);

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputInt("RP", &rp);
                    rp = std::max(0, rp);

                    ImGui::BeginDisabled();
                    int cashPlaceholder{};
                    int bankPlaceholder{};
                    ImGui::InputInt("Cash", &cashPlaceholder);
                    ImGui::InputInt("Bank", &bankPlaceholder);
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("Cash and Bank are account-controlled and are not exposed as unverified local stat writes.");

                    const float kd = deaths > 0 ? static_cast<float>(kills) / static_cast<float>(deaths) : static_cast<float>(kills);
                    ImGui::Text("K/D Ratio");
                    ImGui::SameLine(180.0f);
                    ImGui::Text("%.2f", kd);

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputInt("Kills", &kills);
                    kills = std::max(0, kills);

                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputInt("Deaths", &deaths);
                    deaths = std::max(0, deaths);

                    ImGui::BeginDisabled(stats.pending);
                    if (ImGui::Button("APPLY STATS", ImVec2(-1.0f, 36.0f)))
                        message = statRuntime.QueueApply(rank, rp, kills, deaths) ? "Stat update queued" : "Stat update rejected";
                    if (ImGui::Button("REFRESH STATS", ImVec2(-1.0f, 30.0f)))
                        message = statRuntime.QueueRefresh() ? "Stat refresh queued" : "Stat refresh rejected";
                    ImGui::EndDisabled();

                    if (stats.haveResult)
                    {
                        ImGui::TextWrapped("%s", stats.message.c_str());
                        if (stats.readable)
                        {
                            rank = stats.rank;
                            rp = stats.rp;
                            kills = stats.kills;
                            deaths = stats.deaths;
                            initialized = true;
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Rank, RP, kills and deaths use the native stat backend with read-back verification.");
                    }
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }

            ImGui::Spacing();
            if (ImGui::BeginChild("##self_quick_actions", ImVec2(0.0f, 112.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "QUICK ACTIONS");
                ImGui::Separator();

                const float width = 134.0f;
                if (ImGui::Button("Fill Health", ImVec2(width, 34.0f)))
                    message = playerRuntime.QueueHeal() ? "Health refill queued" : "Health refill rejected";
                ImGui::SameLine();
                if (ImGui::Button("Fill Armor", ImVec2(width, 34.0f)))
                    message = playerRuntime.QueueSetArmor(100) ? "Armor refill queued" : "Armor refill rejected";
                ImGui::SameLine();
                if (ImGui::Button("Clear Wanted", ImVec2(width, 34.0f)))
                    message = playerRuntime.QueueClearWanted() ? "Wanted clear queued" : "Wanted clear rejected";
                ImGui::SameLine();
                if (ImGui::Button("Off Radar", ImVec2(width, 34.0f)))
                {
                    offRadarRuntime.SetEnabled(!offRadar.enabled);
                    message = offRadar.enabled ? "Off Radar disabled" : "Off Radar enabled";
                }
                ImGui::SameLine();
                ImGui::BeginDisabled();
                ImGui::Button("Request Job", ImVec2(width, 34.0f));
                ImGui::EndDisabled();

                ImGui::TextDisabled("%s", message);
            }
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
