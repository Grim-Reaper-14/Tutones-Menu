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

            ImGui::Text("Player ID: %d", player.id);
            ImGui::Text("Ped state: %s", player.pedAvailable ? "available" : "unavailable");

            ImGui::SeparatorText("Progression");
            if (player.statsReadable)
            {
                ImGui::Text("Rank: %d", player.rank);
                ImGui::Text("RP: %d", player.rp);
                ImGui::Text("Money: $%d", player.money);
            }
            else
            {
                ImGui::TextDisabled("Rank / RP / money: unavailable");
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

        if (ImGui::BeginChild("##network_player_list", ImVec2(165.0f, 320.0f), true))
        {
            for (const int playerId : sortedPlayers)
            {
                const auto& player = roster.players[static_cast<std::size_t>(playerId)];
                std::string label = player.name;
                if (player.local)
                    label += " [YOU]";

                ImGui::PushID(playerId);
                if (ImGui::Selectable(label.c_str(), g_SelectedNetworkPlayer == playerId))
                    g_SelectedNetworkPlayer = playerId;
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("##network_player_details", ImVec2(0.0f, 320.0f), true))
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

        ImGui::TextDisabled("Roster refreshes every 500 ms. Deeper account and host-manager fields remain disabled until their Enhanced structures are verified.");
    }
}
