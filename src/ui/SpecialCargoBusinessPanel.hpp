#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::UI
{
    inline void RenderSpecialCargoBusinessPanel() noexcept
    {
        using Game::Recovery::RecoveryRuntime;

        auto& runtime = RecoveryRuntime::Get();
        const auto snapshot = runtime.Snapshot();

        static std::uint64_t lastRevision{};
        static std::array<int, 5> crates{};
        static const char* message = "Ready";

        if (lastRevision != snapshot.revision)
        {
            lastRevision = snapshot.revision;
            for (std::size_t index = 0; index < snapshot.warehouses.size(); ++index)
            {
                const auto& warehouse = snapshot.warehouses[index];
                if (warehouse.readable)
                    crates[index] = std::max(0, warehouse.crates);
            }
        }

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##special_cargo_business_panel", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Special Cargo");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced business controls");
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Recovery business runtime is offline.");
            }
            else if (!snapshot.sessionStarted)
            {
                ImGui::TextDisabled("Join GTA Online to read Special Cargo warehouse state.");
            }
            else
            {
                bool anyOwned{};
                for (std::size_t index = 0; index < snapshot.warehouses.size(); ++index)
                {
                    const auto& warehouse = snapshot.warehouses[index];
                    if (!warehouse.owned)
                        continue;

                    anyOwned = true;
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::Text("Warehouse %d  |  Property %d", warehouse.slot + 1, warehouse.propertyId);

                    if (!warehouse.readable || warehouse.capacity <= 0)
                    {
                        ImGui::TextDisabled("Warehouse state is not readable yet.");
                        ImGui::Separator();
                        ImGui::PopID();
                        continue;
                    }

                    crates[index] = std::clamp(crates[index], 0, warehouse.capacity);
                    ImGui::SetNextItemWidth(300.0f);
                    ImGui::SliderInt("Crates", &crates[index], 0, warehouse.capacity);
                    DescribeLastV11Item("Choose the Special Cargo crate count for this owned warehouse.");
                    ImGui::SameLine();

                    ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                    if (ImGui::Button("Apply", ImVec2(92.0f, 0.0f)))
                        message = runtime.QueueSetWarehouseCrates(static_cast<int>(index), crates[index]) ? "Cargo write queued" : "Cargo write rejected";
                    ImGui::EndDisabled();
                    DescribeLastV11Item("Write the selected crate count and verify the operation through the existing Recovery business backend.");
                    ImGui::TextDisabled("Current: %d / %d", warehouse.crates, warehouse.capacity);
                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (!anyOwned)
                    ImGui::TextDisabled("No owned Special Cargo warehouse was detected.");

                if (snapshot.actionPending)
                    ImGui::TextDisabled("Special Cargo action is running on the GTA script thread...");
                else
                    ImGui::TextDisabled("%s", message);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Use Special Cargo Tools for Lupe sourcing, cooldowns, mission locals, crate-price globals and unique cargo.");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Dedicated Special Cargo business page: owned warehouses and crate stock, with the separate Special Cargo Tools launcher for sourcing and mission controls.");
    }
}
