#include "NetworkPlayersPanel.hpp"

#include "V11Theme.hpp"
#include "../features/network/NetworkRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Tutones::UI
{
    namespace
    {
        int g_SelectedNetworkPlayer{-1};

        [[nodiscard]] bool IsSelectablePlayer(
            const Game::NetworkFeatures::NetworkPlayerRosterSnapshot& roster,
            int playerId) noexcept
        {
            return playerId >= 0
                && playerId < static_cast<int>(roster.players.size())
                && roster.players[static_cast<std::size_t>(playerId)].active;
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

        void RenderSelectedPlayer(const Game::NetworkFeatures::NetworkPlayerSnapshot& player) noexcept
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

            ImGui::SeparatorText("Session identity");
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

            ImGui::SeparatorText("Progression");
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

            ImGui::SeparatorText("Activity");
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

            ImGui::SeparatorText("Combat / history");
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

            ImGui::SeparatorText("Vitals");
            if (player.healthReadable)
                ImGui::Text("Health: %d / %d", player.health, player.maxHealth);
            else
                ImGui::TextDisabled("Health: unavailable");

            if (player.armourReadable)
                ImGui::Text("Armor: %d", player.armour);
            else
                ImGui::TextDisabled("Armor: unavailable");

            if (player.wantedReadable)
                ImGui::Text("Wanted level: %d", player.wantedLevel);
            else
                ImGui::TextDisabled("Wanted level: unavailable");

            ImGui::SeparatorText("Current vehicle");
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

                ImGui::TextUnformatted(vehicleTitle.c_str());
                ImGui::Text("Entity handle: %d", player.vehicle);
                ImGui::Text("Model hash: 0x%08X", static_cast<unsigned int>(player.vehicleModelHash));
                ImGui::Text("Class: %d", player.vehicleClass);
                ImGui::Text("Plate: %s", player.vehiclePlate.empty() ? "unavailable" : player.vehiclePlate.c_str());
            }
            else
            {
                ImGui::TextDisabled("On foot / vehicle unavailable");
            }

            ImGui::SeparatorText("Position");
            if (player.positionReadable)
                ImGui::Text("Coords: %.1f, %.1f, %.1f", player.x, player.y, player.z);
            else
                ImGui::TextDisabled("Coords: unavailable");

            if (player.distanceReadable)
                ImGui::Text("Distance: %.1f m", player.distance);
            else
                ImGui::TextDisabled("Distance: unavailable");

            ImGui::SeparatorText("Connection");
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

    void RenderNetworkPlayersPanel() noexcept
    {
        const auto snapshot = Game::NetworkFeatures::NetworkRuntime::Get().Snapshot();
        const auto& roster = snapshot.playerRoster;

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

        std::vector<int> sortedPlayers;
        sortedPlayers.reserve(static_cast<std::size_t>(roster.activeCount));
        for (const auto& player : roster.players)
        {
            if (player.active)
                sortedPlayers.push_back(player.id);
        }

        std::sort(sortedPlayers.begin(), sortedPlayers.end(), [&roster](int left, int right) {
            const auto& leftPlayer = roster.players[static_cast<std::size_t>(left)];
            const auto& rightPlayer = roster.players[static_cast<std::size_t>(right)];
            if (leftPlayer.name == rightPlayer.name)
                return left < right;
            return leftPlayer.name < rightPlayer.name;
        });

        if (sortedPlayers.empty())
        {
            ImGui::TextDisabled("No active Online players were reported by the session.");
            return;
        }

        if (ImGui::BeginChild("##network_player_list", ImVec2(165.0f, 292.0f), true))
        {
            for (const int playerId : sortedPlayers)
            {
                const auto& player = roster.players[static_cast<std::size_t>(playerId)];
                std::string label = player.name;
                if (player.local)
                    label += " [YOU]";
                if (player.freemodeHost)
                    label += " [HOST]";

                ImGui::PushID(playerId);
                if (ImGui::Selectable(label.c_str(), g_SelectedNetworkPlayer == playerId))
                    g_SelectedNetworkPlayer = playerId;
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("##network_player_details", ImVec2(0.0f, 292.0f), true))
        {
            if (IsSelectablePlayer(roster, g_SelectedNetworkPlayer))
            {
                RenderSelectedPlayer(roster.players[static_cast<std::size_t>(g_SelectedNetworkPlayer)]);
            }
            else
            {
                ImGui::TextDisabled("Select an active player to inspect their live session state.");
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("500 ms read-only refresh. Private account IDs and network addresses are intentionally not collected.");
    }
}
