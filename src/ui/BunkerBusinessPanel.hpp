#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace Tutones::UI
{
    inline void RenderBunkerBusinessPanel() noexcept
    {
        using Game::Recovery::RecoveryRuntime;

        auto& runtime = RecoveryRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        static std::uint64_t lastRevision{};
        static int supplies{};
        static int product{};
        static const char* message = "Ready";

        if (lastRevision != snapshot.revision)
        {
            lastRevision = snapshot.revision;
            if (snapshot.bunker.readable)
            {
                supplies = std::clamp(snapshot.bunker.supplies, 0, 100);
                product = std::clamp(snapshot.bunker.product, 0, 100);
            }
        }

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##bunker_business_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Bunker");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced business controls");
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Recovery business runtime is offline.");
            }
            else if (!snapshot.sessionStarted)
            {
                ImGui::TextDisabled("Join GTA Online to read Bunker state.");
            }
            else if (!snapshot.bunker.owned)
            {
                ImGui::TextDisabled("No owned Bunker was detected.");
            }
            else if (!snapshot.bunker.readable)
            {
                ImGui::TextDisabled("Bunker state is not readable yet.");
            }
            else if (!snapshot.bunker.setup)
            {
                ImGui::TextDisabled("The owned Bunker has not completed setup.");
            }
            else
            {
                ImGui::Text("Property ID: %d", snapshot.bunker.propertyId);

                supplies = std::clamp(supplies, 0, 100);
                ImGui::SetNextItemWidth(300.0f);
                ImGui::SliderInt("Supplies", &supplies, 0, 100);
                DescribeLastV11Item("Choose the Bunker material/supply total for the owned and setup Bunker.");
                ImGui::SameLine();
                ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                if (ImGui::Button("Apply##bunker_supplies", ImVec2(92.0f, 0.0f)))
                    message = runtime.QueueSetBunkerSupplies(supplies) ? "Bunker supplies queued" : "Bunker supplies rejected";
                ImGui::EndDisabled();
                ImGui::TextDisabled("Current supplies: %d / 100", snapshot.bunker.supplies);

                product = std::clamp(product, 0, 100);
                ImGui::SetNextItemWidth(300.0f);
                ImGui::SliderInt("Product", &product, 0, 100);
                DescribeLastV11Item("Choose the Bunker product total for the owned and setup Bunker.");
                ImGui::SameLine();
                ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                if (ImGui::Button("Apply##bunker_product", ImVec2(92.0f, 0.0f)))
                    message = runtime.QueueSetBunkerProduct(product) ? "Bunker product queued" : "Bunker product rejected";
                ImGui::EndDisabled();
                ImGui::TextDisabled("Current product: %d / 100", snapshot.bunker.product);

                if (snapshot.actionPending)
                    ImGui::TextDisabled("Bunker action is running on the GTA script thread...");
                else
                    ImGui::TextDisabled("%s", message);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Use Bunker Tools for product value, sale multipliers, high-demand bonus, production times and Instant Sell.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Dedicated Bunker business page: stock controls plus the separate Bunker Tools launcher for tuning globals and gb_gunrunning Instant Sell.");
    }
}
