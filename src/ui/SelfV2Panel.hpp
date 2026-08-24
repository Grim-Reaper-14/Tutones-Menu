#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/player/GhostOrganizationRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/player/PlayerStatsRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace Tutones::UI
{
    namespace SelfV2Detail
    {
        inline bool Toggle(const char* id, const char* label, bool& value) noexcept
        {
            ImGui::PushID(id);
            ImGui::TextUnformatted(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 42.0f);

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 size{42.0f, 22.0f};
            const bool pressed = ImGui::InvisibleButton("##toggle", size);
            if (pressed)
                value = !value;

            const bool hovered = ImGui::IsItemHovered();
            auto* draw = ImGui::GetWindowDrawList();
            const ImU32 track = value
                ? ImGui::GetColorU32(hovered ? V11Theme::AccentHover : V11Theme::Accent)
                : ImGui::GetColorU32(hovered ? V11Theme::ControlHover : V11Theme::ControlBg);
            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), track, size.y * 0.5f);
            const float knobX = value ? pos.x + size.x - 11.0f : pos.x + 11.0f;
            draw->AddCircleFilled(ImVec2(knobX, pos.y + 11.0f), 8.0f, IM_COL32(225, 230, 238, 255), 24);
            ImGui::PopID();
            return pressed;
        }

        inline void StatInputInt(const char* label, int& value) noexcept
        {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(105.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushID(label);
            ImGui::InputInt("##value", &value, 0, 0);
            ImGui::PopID();
        }

        inline void ReadOnlyMoneyRow(const char* label) noexcept
        {
            ImGui::PushID(label);
            ImGui::TextUnformatted(label);
            ImGui::SameLine(105.0f);
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(-1.0f);
            char unavailable[] = "Transaction controlled";
            ImGui::InputText("##money", unavailable, sizeof(unavailable), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
    }

    inline void RenderSelfV2Panel() noexcept
    {
        using namespace Game::PlayerFeatures;

        auto& playerRuntime = PlayerRuntime::Get();
        auto& offRadarRuntime = OffRadarRuntime::Get();
        auto& ghostRuntime = GhostOrganizationRuntime::Get();
        auto& statsRuntime = PlayerStatsRuntime::Get();

        const auto player = playerRuntime.Snapshot();
        const auto offRadar = offRadarRuntime.Snapshot();
        const auto ghost = ghostRuntime.Snapshot();
        const auto stats = statsRuntime.Snapshot();

        static Game::Ped lastPed{};
        static bool godMode{};
        static bool neverWanted{};
        static bool superRun{};
        static bool superJump{};
        static bool noRagdoll{};
        static int offRadarMode{};
        static int semiGodMode{};
        static bool statsRequested{};
        static bool statsLoaded{};
        static int rank{};
        static int rp{};
        static int kills{};
        static int deaths{};
        static const char* actionMessage = "Ready";

        if (player.valid && player.ped != lastPed)
        {
            lastPed = player.ped;
            godMode = player.invincible;
            neverWanted = player.neverWanted;
            superRun = player.runMultiplier > 1.01f;
            superJump = player.superJump;
            noRagdoll = player.noRagdoll;
            semiGodMode = player.bulletproof ? 1 : 0;
        }

        if (ghost.enabled || ghost.applied)
            offRadarMode = 2;
        else if (offRadar.enabled || offRadar.applied)
            offRadarMode = 1;
        else if (!offRadar.enabled)
            offRadarMode = 0;

        // Retry the first read until the GTA game/script runtime is actually ready.
        // A failed early frame should not permanently leave the editor empty.
        if (!statsRequested && !stats.pending)
            statsRequested = statsRuntime.QueueRefresh();

        if (!statsLoaded && stats.haveResult && stats.readable)
        {
            rank = stats.rank;
            rp = stats.rp;
            kills = stats.kills;
            deaths = stats.deaths;
            statsLoaded = true;
        }

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##self_v2_dashboard", ImVec2(780.0f, 548.0f), true))
        {
            if (ImGui::BeginTable("##self_v2_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##self_player_options", ImVec2(0.0f, 386.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "PLAYER OPTIONS");
                    ImGui::Separator();

                    if (SelfV2Detail::Toggle("god", "God Mode", godMode))
                        playerRuntime.SetInvincible(godMode);
                    DescribeLastV11Item("Maintain local-player invincibility through the Player runtime.");

                    if (SelfV2Detail::Toggle("wanted", "Never Wanted", neverWanted))
                        playerRuntime.SetNeverWanted(neverWanted);
                    DescribeLastV11Item("Continuously clear the local player's wanted level while enabled.");

                    if (SelfV2Detail::Toggle("run", "Super Run", superRun))
                        playerRuntime.SetRunMultiplier(superRun ? 1.49f : 1.0f);
                    DescribeLastV11Item("Switch the supported run/sprint multiplier between normal and the 1.49x Enhanced limit.");

                    if (SelfV2Detail::Toggle("jump", "Super Jump", superJump))
                        playerRuntime.SetSuperJump(superJump);
                    DescribeLastV11Item("Maintain GTA's super-jump state on the script tick.");

                    if (SelfV2Detail::Toggle("ragdoll", "No Ragdoll", noRagdoll))
                        playerRuntime.SetNoRagdoll(noRagdoll);
                    DescribeLastV11Item("Prevent the local ped from entering normal ragdoll states.");

                    ImGui::Spacing();
                    ImGui::TextUnformatted("Off The Radar");
                    ImGui::SameLine(145.0f);
                    ImGui::SetNextItemWidth(-1.0f);
                    constexpr std::array<const char*, 3> radarModes{{"Off", "Off Radar", "Ghost Organization"}};
                    if (ImGui::Combo("##off_radar_mode", &offRadarMode, radarModes.data(), static_cast<int>(radarModes.size())))
                    {
                        if (offRadarMode == 0)
                        {
                            ghostRuntime.SetEnabled(false);
                            offRadarRuntime.SetEnabled(false);
                        }
                        else if (offRadarMode == 1)
                        {
                            ghostRuntime.SetEnabled(false);
                            offRadarRuntime.SetEnabled(true);
                        }
                        else
                        {
                            offRadarRuntime.SetEnabled(true);
                            ghostRuntime.SetEnabled(true);
                        }
                    }
                    DescribeLastV11Item("Choose normal Off Radar or the current Enhanced Ghost Organization Freemode flag layered on top of Off Radar.");

                    ImGui::TextUnformatted("Semi God Mode");
                    ImGui::SameLine(145.0f);
                    ImGui::SetNextItemWidth(-1.0f);
                    constexpr std::array<const char*, 2> semiModes{{"Normal", "Bulletproof"}};
                    if (ImGui::Combo("##semi_god_mode", &semiGodMode, semiModes.data(), static_cast<int>(semiModes.size())))
                        playerRuntime.SetBulletproof(semiGodMode == 1);
                    DescribeLastV11Item("Normal keeps regular damage. Bulletproof blocks bullet damage while leaving other damage types game-controlled.");

                    ImGui::Spacing();
                    if (ImGui::Button("Clean Player", ImVec2(180.0f, 34.0f)))
                        actionMessage = playerRuntime.QueueClearDamage() ? "Player cleanup queued" : "Player cleanup rejected";
                    DescribeLastV11Item("Clear blood, wetness, environmental dirt and visible damage from the local ped.");
                    ImGui::SameLine();
                    if (ImGui::Button("Suicide", ImVec2(-1.0f, 34.0f)))
                        actionMessage = playerRuntime.QueueSuicide() ? "Suicide queued" : "Suicide rejected";
                    DescribeLastV11Item("Trigger the normal death transition while preserving the saved God Mode preference for respawn.");

                    ImGui::TextDisabled("OTR: %s  /  Ghost Org: %s",
                        offRadar.applied ? "ACTIVE" : "OFF",
                        ghost.applied ? "ACTIVE" : ghost.enabled ? "WAITING" : "OFF");
                }
                ImGui::EndChild();

                ImGui::TableNextColumn();
                if (ImGui::BeginChild("##self_stats_editor", ImVec2(0.0f, 386.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "STATS EDITOR");
                    ImGui::SameLine();
                    ImGui::TextDisabled("MP%d", stats.characterIndex >= 0 ? stats.characterIndex : 0);
                    ImGui::Separator();

                    const int oldRank = rank;
                    SelfV2Detail::StatInputInt("Rank", rank);
                    rank = std::clamp(rank, 1, 8000);
                    if (rank != oldRank)
                    {
                        if (const auto required = PlayerStatsRuntime::RpForRank(rank))
                            rp = *required;
                    }

                    SelfV2Detail::StatInputInt("RP", rp);
                    rp = std::max(0, rp);
                    SelfV2Detail::ReadOnlyMoneyRow("Cash");
                    SelfV2Detail::ReadOnlyMoneyRow("Bank");

                    ImGui::TextUnformatted("K/D Ratio");
                    ImGui::SameLine(105.0f);
                    ImGui::SetNextItemWidth(-1.0f);
                    float kd = deaths > 0 ? static_cast<float>(kills) / static_cast<float>(deaths) : static_cast<float>(kills);
                    ImGui::BeginDisabled();
                    ImGui::InputFloat("##kd_derived", &kd, 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                    ImGui::EndDisabled();

                    SelfV2Detail::StatInputInt("Kills", kills);
                    SelfV2Detail::StatInputInt("Deaths", deaths);
                    kills = std::max(0, kills);
                    deaths = std::max(0, deaths);

                    ImGui::Spacing();
                    ImGui::BeginDisabled(stats.pending || !stats.readable);
                    if (ImGui::Button("APPLY STATS", ImVec2(-1.0f, 36.0f)))
                        actionMessage = statsRuntime.QueueApply(rank, rp, kills, deaths) ? "Stat writes queued" : "Stat writes rejected";
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Apply Rank, RP, Kills and Deaths on the GTA script thread with stat read-back. K/D is derived from kills/deaths. Cash and Bank remain transaction-controlled.");

                    if (ImGui::Button("Refresh Current Stats", ImVec2(-1.0f, 28.0f)))
                    {
                        statsLoaded = false;
                        const bool queued = statsRuntime.QueueRefresh();
                        statsRequested = queued;
                        actionMessage = queued ? "Stat refresh queued" : "Stat refresh rejected";
                    }

                    if (stats.pending)
                        ImGui::TextDisabled("Reading/writing Online stats...");
                    else
                        ImGui::TextWrapped("%s", stats.message.c_str());
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }

            ImGui::Spacing();
            if (ImGui::BeginChild("##self_quick_actions", ImVec2(0.0f, 116.0f), true))
            {
                ImGui::TextColored(V11Theme::Accent, "QUICK ACTIONS");
                ImGui::Separator();

                const float buttonWidth = 136.0f;
                if (ImGui::Button("Fill Health", ImVec2(buttonWidth, 36.0f)))
                    actionMessage = playerRuntime.QueueHeal() ? "Heal queued" : "Heal rejected";
                ImGui::SameLine();
                if (ImGui::Button("Fill Armor", ImVec2(buttonWidth, 36.0f)))
                    actionMessage = playerRuntime.QueueSetArmor(100) ? "Armor queued" : "Armor rejected";
                ImGui::SameLine();
                if (ImGui::Button("Clear Wanted", ImVec2(buttonWidth, 36.0f)))
                    actionMessage = playerRuntime.QueueClearWanted() ? "Wanted clear queued" : "Wanted clear rejected";
                ImGui::SameLine();
                if (ImGui::Button("Off Radar", ImVec2(buttonWidth, 36.0f)))
                {
                    offRadarRuntime.SetEnabled(true);
                    actionMessage = "Off Radar enabled";
                }
                ImGui::SameLine();
                ImGui::BeginDisabled();
                ImGui::Button("Request Job", ImVec2(buttonWidth, 36.0f));
                ImGui::EndDisabled();
                DescribeLastV11Item("Request Job remains disabled until a current Enhanced job-request path is verified.");

                ImGui::TextDisabled("%s", actionMessage);
            }
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        SetV11Description("Tutones Menu V2 Self dashboard: Player Options, Enhanced Ghost Organization / Off Radar, live stat editing and quick actions mirrored from the approved design.");
    }
}
