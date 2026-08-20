#include "NetworkPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/game/GameSessionRuntime.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"

#include <imgui.h>

#include <algorithm>

namespace Tutones::UI
{
    namespace
    {
        using Game::NetworkFeatures::NetworkRuntime;
        using Game::SessionFeatures::GameServiceAction;
        using Game::SessionFeatures::GameSessionRuntime;

        int g_AirstrikeDamage{250};
        const char* g_NetworkMessage{"Ready"};

        bool RenderToggleSwitch(const char* label, bool& value) noexcept
        {
            ImGui::PushID(label);
            const float height = ImGui::GetFrameHeight();
            const float width = height * 1.75f;
            const float radius = height * 0.5f;
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton("##switch", ImVec2(width, height));
            if (pressed)
                value = !value;
            const bool hovered = ImGui::IsItemHovered();
            const ImVec4 track = value
                ? (hovered ? V11Theme::AccentHover : V11Theme::Accent)
                : (hovered ? V11Theme::ControlHover : V11Theme::ControlBg);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(position, ImVec2(position.x + width, position.y + height), ImGui::GetColorU32(track), radius);
            const float knobX = value ? position.x + width - radius : position.x + radius;
            draw->AddCircleFilled(ImVec2(knobX, position.y + radius), std::max(2.0f, radius - 2.0f),
                ImGui::GetColorU32(ImVec4(0.94f, 0.97f, 1.0f, 1.0f)), 24);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextUnformatted(label);
            ImGui::PopID();
            return pressed;
        }

        const char* ServiceName(GameServiceAction action) noexcept
        {
            switch (action)
            {
            case GameServiceAction::None: return "None";
            case GameServiceAction::AirstrikeAhead: return "Airstrike Ahead";
            case GameServiceAction::AmmoDrop: return "Ammo Drop";
            case GameServiceAction::MinigunDrop: return "Minigun Drop";
            }
            return "Unknown";
        }

        void RenderServices(GameSessionRuntime& runtime) noexcept
        {
            const auto snapshot = runtime.Snapshot();
            g_AirstrikeDamage = std::clamp(g_AirstrikeDamage, 1, 1000);
            ImGui::TextColored(V11Theme::Accent, "Enhanced Online Services");
            ImGui::TextWrapped("These reproduce verified Enhanced gameplay effects directly; they do not fake Rockstar billing or service-mission state.");
            ImGui::Separator();
            ImGui::SliderInt("Airstrike damage", &g_AirstrikeDamage, 1, 1000);
            DescribeLastV11Item("Damage passed to the Enhanced WEAPON_AIRSTRIKE_ROCKET projectile burst.");

            ImGui::BeginDisabled(snapshot.servicePending);
            if (ImGui::Button("Airstrike Ahead", ImVec2(-1.0f, 0.0f)))
                g_NetworkMessage = runtime.QueueAirstrikeAhead(g_AirstrikeDamage) ? "Airstrike queued" : "Airstrike rejected";
            DescribeLastV11Item("Fire a five-rocket Enhanced airstrike pattern roughly 45 meters ahead of the local player.");
            if (ImGui::Button("Ammo Drop", ImVec2(220.0f, 0.0f)))
                g_NetworkMessage = runtime.QueueAmmoDrop() ? "Ammo Drop queued" : "Ammo Drop rejected";
            DescribeLastV11Item("Load prop_box_ammo02a and create the Enhanced PICKUP_AMMO_BULLET_MP drop in front of the player.");
            ImGui::SameLine();
            if (ImGui::Button("Minigun Drop", ImVec2(-1.0f, 0.0f)))
                g_NetworkMessage = runtime.QueueMinigunDrop() ? "Minigun Drop queued" : "Minigun Drop rejected";
            DescribeLastV11Item("Create the decompiled special ammo-drop variant with PICKUP_WEAPON_MINIGUN.");
            ImGui::EndDisabled();

            ImGui::TextDisabled("%s", snapshot.servicePending ? "Service action pending on the GTA script thread..." : g_NetworkMessage);
            if (snapshot.lastServiceAction != GameServiceAction::None)
                ImGui::Text("Last service: %s - %s", ServiceName(snapshot.lastServiceAction),
                    snapshot.lastServiceSucceeded ? "dispatched" : "failed");
        }

        void RenderQualityOfLife() noexcept
        {
            auto& runtime = NetworkRuntime::Get();
            const auto snapshot = runtime.Snapshot();
            bool silence = snapshot.silencePhoneCalls;
            bool deathBarriers = snapshot.disableDeathBarriers;

            ImGui::TextColored(V11Theme::Accent, "Online Quality of Life");
            ImGui::Separator();
            ImGui::BeginDisabled(!runtime.IsRunning());
            if (RenderToggleSwitch("Silence Phone Calls", silence))
                runtime.SetSilencePhoneCalls(silence);
            DescribeLastV11Item("Yim-style incoming-call suppression: when an Online call is active, set the verified phone state global to the skipped state.");
            if (RenderToggleSwitch("Disable Death Barriers", deathBarriers))
                runtime.SetDisableDeathBarriers(deathBarriers);
            DescribeLastV11Item("Apply Yim's current reversible Freemode bytecode patch that disables under-map death barriers and spectate-related random deaths.");
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::SeparatorText("Backend state");
            ImGui::Text("Online session: %s", snapshot.sessionStarted ? "started" : "not started");
            ImGui::Text("Phone globals: %s", snapshot.phoneGlobalsReady ? "ready" : "unavailable");
            ImGui::Text("Silenced calls: %llu", static_cast<unsigned long long>(snapshot.silencedCalls));
            if (snapshot.lastSilencedCaller >= 0)
                ImGui::Text("Last silenced caller ID: %d", snapshot.lastSilencedCaller);
            ImGui::Text("Script patch hook: %s", snapshot.patchHookActive ? "active" : "unavailable");
            ImGui::Text("Freemode program: %s", snapshot.freemodeLoaded ? "loaded" : "not loaded");
            ImGui::Text("Death barrier patch: %s", snapshot.deathBarrierApplied
                ? "applied"
                : (snapshot.deathBarrierSupported ? "supported / inactive" : "waiting / unsupported"));
        }

        void RenderPlayerState() noexcept
        {
            const auto state = Game::PlayerFeatures::OffRadarRuntime::Get().Snapshot();
            ImGui::TextColored(V11Theme::Accent, "Player Network State");
            ImGui::Separator();
            ImGui::Text("Off Radar: %s", state.enabled ? "requested" : "off");
            ImGui::Text("Broadcast state: %s", state.applied ? "applied" : "not applied");
            ImGui::Text("Freemode thread: %s", state.freemodeReady ? "ready" : "unavailable");
            ImGui::Text("Safe to modify: %s", state.safeToModify ? "yes" : "no");
            ImGui::Spacing();
            ImGui::TextWrapped("The canonical Off Radar toggle remains under Player > Online and uses verified Freemode broadcast globals rather than local HUD hiding.");
        }
    }

    void RenderNetworkPanel(std::size_t subtab) noexcept
    {
        const std::size_t index = subtab < 3 ? subtab : 0;
        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
        if (ImGui::BeginChild("##network_panel", ImVec2(490.0f, 430.0f), true))
        {
            constexpr const char* names[] = {"Services", "Quality of Life", "Player State"};
            ImGui::TextColored(V11Theme::Accent, "Network");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", names[index]);
            ImGui::Separator();
            if (index == 0)
                RenderServices(GameSessionRuntime::Get());
            else if (index == 1)
                RenderQualityOfLife();
            else
                RenderPlayerState();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
