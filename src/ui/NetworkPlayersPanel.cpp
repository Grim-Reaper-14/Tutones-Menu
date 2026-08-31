#include "NetworkPlayersPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/network/NetworkPlayerToolsRuntime.hpp"
#include "../features/network/NetworkRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::UI
{
    namespace
    {
        int g_SelectedNetworkPlayer{-1};
        char g_PlayerSearch[64]{};

        [[nodiscard]] bool IsSelectablePlayer(
            const Game::NetworkFeatures::NetworkPlayerRosterSnapshot& roster,
            int playerId) noexcept
        {
            return playerId >= 0
                && playerId < static_cast<int>(roster.players.size())
                && roster.players[static_cast<std::size_t>(playerId)].active;
        }

        [[nodiscard]] bool ContainsCaseInsensitive(std::string_view text, std::string_view query)
        {
            if (query.empty())
                return true;
            if (query.size() > text.size())
                return false;

            for (std::size_t start = 0; start + query.size() <= text.size(); ++start)
            {
                bool matches = true;
                for (std::size_t index = 0; index < query.size(); ++index)
                {
                    const auto left = static_cast<unsigned char>(text[start + index]);
                    const auto right = static_cast<unsigned char>(query[index]);
                    if (std::tolower(left) != std::tolower(right))
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                    return true;
            }
            return false;
        }

        void EnsureSelection(const Game::NetworkFeatures::NetworkPlayerRosterSnapshot& roster) noexcept
        {
            if (IsSelectablePlayer(roster, g_SelectedNetworkPlayer))
                return;

            if (IsSelectablePlayer(roster, roster.localPlayer))
            {
                g_SelectedNetworkPlayer = roster.localPlayer;
                return;
            }

            g_SelectedNetworkPlayer = -1;
            for (const auto& player : roster.players)
            {
                if (player.active)
                {
                    g_SelectedNetworkPlayer = player.id;
                    break;
                }
            }
        }

        void RenderPlayerActions(
            const Game::NetworkFeatures::NetworkPlayerSnapshot& player,
            Game::NetworkFeatures::NetworkPlayerToolsRuntime& tools,
            const Game::NetworkFeatures::NetworkPlayerToolsSnapshot& toolState) noexcept
        {
            ImGui::SeparatorText("Player actions");

            const bool spectatingThisPlayer = toolState.spectating
                && toolState.spectatingPlayer == player.id;
            const bool targetUnavailable = !player.pedAvailable || !player.positionReadable;
            const bool actionBlocked = toolState.pending || player.local || targetUnavailable;

            if (ImGui::BeginTable("##selected_player_actions", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::BeginDisabled(toolState.pending || player.local || !player.pedAvailable);
                if (spectatingThisPlayer)
                {
                    if (ImGui::Button("Stop Spectating", ImVec2(-1.0f, 30.0f)))
                        static_cast<void>(tools.QueueStopSpectating());
                    DescribeLastV11Item("Leave GTA's network spectator mode and return control to the local player.");
                }
                else
                {
                    if (ImGui::Button("Spectate", ImVec2(-1.0f, 30.0f)))
                        static_cast<void>(tools.QueueSpectate(player.id));
                    DescribeLastV11Item("Use GTA's network spectator mode to observe the selected active player. Selecting another player while spectating switches the target cleanly.");
                }
                ImGui::EndDisabled();

                ImGui::TableSetColumnIndex(1);
                ImGui::BeginDisabled(actionBlocked);
                if (ImGui::Button("Teleport To Player", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(tools.QueueTeleportToPlayer(player.id));
                ImGui::EndDisabled();
                DescribeLastV11Item("Move only your local player or current vehicle beside the selected player, offset behind their heading to avoid occupying the exact same position.");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::BeginDisabled(actionBlocked);
                if (ImGui::Button("Set Player Waypoint", ImVec2(-1.0f, 30.0f)))
                    static_cast<void>(tools.QueueWaypointToPlayer(player.id));
                ImGui::EndDisabled();
                DescribeLastV11Item("Place the map waypoint at the selected player's currently streamed position.");

                ImGui::TableSetColumnIndex(1);
                if (toolState.pending)
                {
                    ImGui::TextDisabled("Action running...");
                }
                else if (toolState.spectating)
                {
                    ImGui::Text("Spectating PID %d", toolState.spectatingPlayer);
                }
                else
                {
                    ImGui::TextDisabled("Spectator: off");
                }

                ImGui::EndTable();
            }

            if (player.local)
                ImGui::TextDisabled("Player actions are disabled for your own roster entry.");
            else if (targetUnavailable)
                ImGui::TextDisabled("Actions require the selected player's ped and position to be streamed locally.");

            if (toolState.haveResult || toolState.pending)
                ImGui::TextDisabled("%s", toolState.message.c_str());
        }

        void RenderSelectedPlayer(
            const Game::NetworkFeatures::NetworkPlayerSnapshot& player,
            Game::NetworkFeatures::NetworkPlayerToolsRuntime& tools,
            const Game::NetworkFeatures::NetworkPlayerToolsSnapshot& toolState) noexcept
        {
            ImGui::TextColored(V11Theme::Accent, "%s", player.name.c_str());
            if (player.local)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[YOU]");
            }
            if (player.freemodeHost)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[FREEMODE HOST]");
            }
            if (player.vehicleReadable)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[IN VEHICLE]");
            }
            if (player.healthReadable && player.health <= 0)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[DEAD]");
            }

            RenderPlayerActions(player, tools, toolState);

            ImGui::SeparatorText("Quick status");
            if (player.healthReadable)
                ImGui::Text("Health: %d / %d", player.health, player.maxHealth);
            else
                ImGui::TextDisabled("Health: unavailable");
            ImGui::SameLine();
            if (player.armourReadable)
                ImGui::Text("| Armor: %d", player.armour);
            else
                ImGui::TextDisabled("| Armor: --");

            if (player.wantedReadable)
                ImGui::Text("Wanted: %d stars", player.wantedLevel);
            else
                ImGui::TextDisabled("Wanted: unavailable");
            ImGui::SameLine();
            if (player.distanceReadable)
                ImGui::Text("| Distance: %.1f m", player.distance);
            else
                ImGui::TextDisabled("| Distance: --");

            if (ImGui::CollapsingHeader("Session identity", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Player ID: %d", player.id);
                if (player.activeIndex >= 0)
                    ImGui::Text("Active index: %d", player.activeIndex);
                else
                    ImGui::TextDisabled("Active index: unavailable");
                ImGui::Text("Manager slot: %s",
                    player.managerSlotPresent
                        ? (player.managerIndexMatches ? "present / index verified" : "present / index mismatch")
                        : "unavailable");
                if (player.managerSlotPresent)
                    ImGui::Text("Manager local flag: %s", player.managerLocalFlag ? "set" : "clear");
                ImGui::Text("Ped state: %s", player.pedAvailable ? "available" : "unavailable");
            }

            if (ImGui::CollapsingHeader("Position & vehicle", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (player.positionReadable)
                    ImGui::Text("Coords: %.1f, %.1f, %.1f", player.x, player.y, player.z);
                else
                    ImGui::TextDisabled("Coords: unavailable");

                if (player.distanceReadable)
                    ImGui::Text("Distance: %.1f m", player.distance);
                else
                    ImGui::TextDisabled("Distance: unavailable");

                if (player.vehicleReadable)
                {
                    std::string vehicleTitle;
                    if (!player.vehicleMake.empty())
                        vehicleTitle = player.vehicleMake;
                    if (!player.vehicleName.empty())
                    {
                        if (!vehicleTitle.empty())
                            vehicleTitle += " ";
                        vehicleTitle += player.vehicleName;
                    }
                    if (vehicleTitle.empty())
                        vehicleTitle = "Unknown vehicle";

                    ImGui::Text("Vehicle: %s", vehicleTitle.c_str());
                    ImGui::Text("Entity handle: %d", player.vehicle);
                    ImGui::Text("Model hash: 0x%08X", static_cast<unsigned int>(player.vehicleModelHash));
                    ImGui::Text("Class: %d", player.vehicleClass);
                    ImGui::Text("Plate: %s", player.vehiclePlate.empty() ? "unavailable" : player.vehiclePlate.c_str());
                }
                else
                {
                    ImGui::TextDisabled("Vehicle: on foot / unavailable");
                }
            }

            if (ImGui::CollapsingHeader("Progression"))
            {
                if (player.statsReadable)
                {
                    ImGui::Text("Rank: %d", player.rank);
                    ImGui::Text("RP: %d", player.rp);
                    ImGui::Text("Crew RP: %d", player.crewRp);
                    ImGui::Text("Wallet balance: $%d", player.walletBalance);
                    ImGui::Text("Money: $%d", player.money);
                }
                else
                {
                    ImGui::TextDisabled("Freemode progression block: unavailable");
                }
            }

            if (ImGui::CollapsingHeader("Activity"))
            {
                if (player.statsReadable)
                {
                    ImGui::Text("Current activity ID: %d", player.currentActivity);
                    ImGui::Text("Mission script instance: %d", player.missionScriptInstance);
                    ImGui::Text("Team: %d", player.team);
                    ImGui::Text("Can spectate: %s", player.canSpectate ? "yes" : "no");
                    ImGui::Text("Communication restrictions: 0x%08X",
                        static_cast<unsigned int>(player.communicationRestrictions));
                }
                else
                {
                    ImGui::TextDisabled("Activity state: unavailable");
                }
            }

            if (ImGui::CollapsingHeader("Combat / history"))
            {
                if (player.statsReadable)
                {
                    ImGui::Text("K/D ratio: %.2f", player.kdRatio);
                    ImGui::Text("Player kills / deaths: %d / %d", player.killsOnPlayers, player.deathsByPlayers);
                    ImGui::Text("Weapon accuracy: %.2f", player.weaponAccuracy);
                    ImGui::Text("Races won / lost: %d / %d", player.racesWon, player.racesLost);
                    ImGui::Text("Deathmatches won / lost: %d / %d", player.deathmatchesWon, player.deathmatchesLost);
                    ImGui::Text("Missions won / played: %d / %d", player.missionWins, player.totalMissionsPlayed);
                    ImGui::Text("Survivals won / played: %d / %d", player.survivalWins, player.totalSurvivalsPlayed);
                    ImGui::Text("Favorite vehicle hash: 0x%08X", static_cast<unsigned int>(player.favoriteVehicleHash));
                    ImGui::Text("Favorite weapon hash: 0x%08X", static_cast<unsigned int>(player.favoriteWeaponHash));
                }
                else
                {
                    ImGui::TextDisabled("Combat/history stats: unavailable");
                }
            }

            if (ImGui::CollapsingHeader("Connection"))
            {
                if (player.latencyReadable)
                    ImGui::Text("Average latency: %.2f", player.averageLatency);
                else
                    ImGui::TextDisabled("Average latency: unavailable");

                if (player.packetLossReadable)
                    ImGui::Text("Packet loss: %.2f", player.averagePacketLoss);
                else
                    ImGui::TextDisabled("Packet loss: unavailable");

                if (player.resendReadable)
                    ImGui::Text("Highest reliable resend count: %d", player.highestReliableResendCount);
                else
                    ImGui::TextDisabled("Reliable resend count: unavailable");
            }
        }
    }

    void RenderNetworkPlayersPanel() noexcept
    {
        const auto snapshot = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
        const auto& roster = snapshot.playerRoster;
        auto& tools = Game::NetworkFeatures::NetworkPlayerToolsRuntime::Get();
        const auto toolState = tools.Snapshot();

        ImGui::TextColored(V11Theme::Accent, "Online Players");
        ImGui::SameLine();
        ImGui::TextDisabled("%d / 32", roster.activeCount);
        ImGui::Separator();

        if (!snapshot.sessionStarted)
        {
            g_SelectedNetworkPlayer = -1;
            ImGui::TextWrapped("Join a GTA Online session to populate the live player roster.");
            return;
        }

        if (!roster.backendReady)
        {
            g_SelectedNetworkPlayer = -1;
            ImGui::TextWrapped("The Enhanced player-native backend is not ready on the GTA script thread yet.");
            return;
        }

        if (roster.freemodeHost >= 0)
            ImGui::TextDisabled("Freemode host: %d", roster.freemodeHost);
        else
            ImGui::TextDisabled("Freemode host: unavailable");
        ImGui::SameLine();
        if (roster.freemodeParticipants >= 0)
            ImGui::TextDisabled("| participants: %d", roster.freemodeParticipants);
        else
            ImGui::TextDisabled("| participants: unavailable");

        if (roster.managerReady)
        {
            ImGui::TextDisabled("Player manager: loaded %d | physical %d | remote physical %d | max %d",
                roster.managerLoadedPlayers,
                roster.managerPhysicalPlayers,
                roster.managerNonLocalPhysicalPlayers,
                roster.managerMaxPlayers);
        }
        else
        {
            ImGui::TextDisabled("Player manager: unavailable; native roster remains active.");
        }

        EnsureSelection(roster);

        ImGui::SetNextItemWidth(165.0f);
        ImGui::InputTextWithHint("##network_player_search", "Search players...", g_PlayerSearch, sizeof(g_PlayerSearch));
        DescribeLastV11Item("Filter the live 32-slot roster by player name without changing the selected-player backend.");
        ImGui::SameLine();
        if (toolState.spectating)
            ImGui::TextDisabled("Spectating PID %d", toolState.spectatingPlayer);
        else
            ImGui::TextDisabled("Select a player for details and actions");

        std::vector<int> sortedPlayers;
        sortedPlayers.reserve(static_cast<std::size_t>(roster.activeCount));
        const std::string_view filter(g_PlayerSearch);
        for (const auto& player : roster.players)
        {
            if (player.active && ContainsCaseInsensitive(player.name, filter))
                sortedPlayers.push_back(player.id);
        }

        std::sort(sortedPlayers.begin(), sortedPlayers.end(), [&roster](int left, int right) {
            const auto& leftPlayer = roster.players[static_cast<std::size_t>(left)];
            const auto& rightPlayer = roster.players[static_cast<std::size_t>(right)];
            if (leftPlayer.local != rightPlayer.local)
                return leftPlayer.local;
            if (leftPlayer.freemodeHost != rightPlayer.freemodeHost)
                return leftPlayer.freemodeHost;
            if (leftPlayer.name == rightPlayer.name)
                return left < right;
            return leftPlayer.name < rightPlayer.name;
        });

        if (sortedPlayers.empty())
        {
            ImGui::TextDisabled(filter.empty()
                ? "No active Online players were reported by the session."
                : "No active players match the current search.");
            return;
        }

        if (ImGui::BeginChild("##network_player_list", ImVec2(165.0f, 268.0f), true))
        {
            for (const int playerId : sortedPlayers)
            {
                const auto& player = roster.players[static_cast<std::size_t>(playerId)];
                std::string label = player.name;
                if (player.local)
                    label += " [YOU]";
                if (player.freemodeHost)
                    label += " [HOST]";
                if (toolState.spectating && toolState.spectatingPlayer == player.id)
                    label += " [SPEC]";

                ImGui::PushID(playerId);
                if (ImGui::Selectable(label.c_str(), g_SelectedNetworkPlayer == playerId))
                    g_SelectedNetworkPlayer = playerId;
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("##network_player_details", ImVec2(0.0f, 268.0f), true))
        {
            if (IsSelectablePlayer(roster, g_SelectedNetworkPlayer))
            {
                RenderSelectedPlayer(
                    roster.players[static_cast<std::size_t>(g_SelectedNetworkPlayer)],
                    tools,
                    toolState);
            }
            else
            {
                ImGui::TextDisabled("Select an active player to inspect their live session state.");
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("500 ms roster refresh. Spectate/waypoint/teleport use live streamed player entities; private account IDs and network addresses are not collected.");
        SetV11Description("Online Players - searchable 32-slot roster with live Enhanced session details plus selected-player spectate, waypoint and local teleport tools.");
    }
}
