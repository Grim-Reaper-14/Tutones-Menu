#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/recovery/SpecialCargoToolsRuntime.hpp"

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
        using Game::Recovery::SpecialCargoToolsRuntime;

        auto& runtime = RecoveryRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        auto& tools = SpecialCargoToolsRuntime::Get();
        const auto toolState = tools.Snapshot();

        static std::uint64_t lastRevision{};
        static std::array<int, 5> crates{};
        static const char* message = "Ready";
        static int sourcingAmount = 1;
        static int cargoType = -1;
        static int lupeSpecialItem = 0;
        static bool lupeSpecialAvailable = false;
        static int buyCooldown = 300000;
        static int sellCooldown = 1800000;
        static int priceTier = 0;
        static int cratePrice = 0;
        static int uniqueSpecialIndex = 0;

        constexpr std::array<const char*, 12> CargoTypeNames{
            "Any", "Medical Supplies", "Tobacco & Alcohol", "Art & Antiques",
            "Electronic Goods", "Weapons & Ammo", "Narcotics", "Gemstones",
            "Animal Materials", "Counterfeit Goods", "Jewelry", "Bullion"
        };
        constexpr std::array<const char*, 6> LupeSpecialNames{
            "Ornamental Egg", "Golden Minigun", "Extra Large Diamond",
            "Sasquatch Hide", "Film Reel", "Rare Pocket Watch"
        };
        constexpr std::array<const char*, 21> PriceTierNames{
            "1 crate (f_15825)", "2 crates (f_15826)", "3 crates (f_15827)",
            "4-5 crates (f_15828)", "6-7 crates (f_15829)", "8-9 crates (f_15830)",
            "10-14 crates (f_15831)", "15-19 crates (f_15832)", "20-24 crates (f_15833)",
            "25-29 crates (f_15834)", "30-34 crates (f_15835)", "35-39 crates (f_15836)",
            "40-44 crates (f_15837)", "45-49 crates (f_15838)", "50-59 crates (f_15839)",
            "60-69 crates (f_15840)", "70-79 crates (f_15841)", "80-89 crates (f_15842)",
            "90-99 crates (f_15843)", "100-110 crates (f_15844)", "111 crates (f_15845)"
        };
        constexpr std::array<const char*, 6> UniqueSpecialNames{
            "Ornamental Egg", "Golden Minigun", "Large Diamond",
            "Rare Hide", "Film Reel", "Rare Pocket Watch"
        };
        constexpr std::array<int, 6> UniqueSpecialValues{2, 4, 6, 7, 8, 9};

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

        ImGui::SetCursorPos(ImVec2(226.0f, 52.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##special_cargo_business_panel", ImVec2(490.0f, 394.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Special Cargo");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
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
                    ImGui::Text("Warehouse %d | Property %d", warehouse.slot + 1, warehouse.propertyId);

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
                    ImGui::SameLine();
                    ImGui::BeginDisabled(snapshot.actionPending || !snapshot.statsReady);
                    if (ImGui::Button("Apply", ImVec2(92.0f, 0.0f)))
                        message = runtime.QueueSetWarehouseCrates(static_cast<int>(index), crates[index]) ? "Cargo write queued" : "Cargo write rejected";
                    ImGui::EndDisabled();
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

            if (ImGui::CollapsingHeader("Special Cargo Tools", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SeparatorText("Lupe Sourcing");
                sourcingAmount = std::clamp(sourcingAmount, 1, 111);
                ImGui::SetNextItemWidth(180.0f);
                ImGui::SliderInt("Sourcing amount", &sourcingAmount, 1, 111);

                int cargoComboIndex = cargoType + 1;
                ImGui::SetNextItemWidth(280.0f);
                if (ImGui::Combo("Cargo type", &cargoComboIndex, CargoTypeNames.data(), static_cast<int>(CargoTypeNames.size())))
                    cargoType = cargoComboIndex - 1;

                ImGui::SetNextItemWidth(280.0f);
                ImGui::Combo("Special item", &lupeSpecialItem, LupeSpecialNames.data(), static_cast<int>(LupeSpecialNames.size()));
                ImGui::Checkbox("Special item available", &lupeSpecialAvailable);

                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Apply Lupe sourcing globals", ImVec2(-1.0f, 0.0f)))
                    tools.QueueSourcingSettings(sourcingAmount, cargoType, lupeSpecialItem, lupeSpecialAvailable);
                ImGui::EndDisabled();

                ImGui::SeparatorText("Mission Controls");
                ImGui::SetNextItemWidth(190.0f);
                ImGui::InputInt("Buy cooldown (ms)", &buyCooldown, 1000, 10000);
                ImGui::SetNextItemWidth(190.0f);
                ImGui::InputInt("Sell cooldown (ms)", &sellCooldown, 1000, 10000);
                buyCooldown = std::max(0, buyCooldown);
                sellCooldown = std::max(0, sellCooldown);

                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Apply cooldown globals", ImVec2(220.0f, 0.0f)))
                    tools.QueueCooldowns(buyCooldown, sellCooldown);
                ImGui::SameLine();
                if (ImGui::Button("Remove cooldowns", ImVec2(-1.0f, 0.0f)))
                    tools.QueueCooldowns(0, 0);
                if (ImGui::Button("Instant Buy (active mission)", ImVec2(220.0f, 0.0f)))
                    tools.QueueInstantBuy();
                ImGui::SameLine();
                if (ImGui::Button("Instant Sell (active mission)", ImVec2(-1.0f, 0.0f)))
                    tools.QueueInstantSell();
                ImGui::EndDisabled();

                ImGui::SeparatorText("Crate Price");
                ImGui::SetNextItemWidth(300.0f);
                ImGui::Combo("Price tier", &priceTier, PriceTierNames.data(), static_cast<int>(PriceTierNames.size()));
                cratePrice = std::max(0, cratePrice);
                ImGui::SetNextItemWidth(190.0f);
                ImGui::InputInt("Price value", &cratePrice, 1000, 10000);
                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Apply selected price", ImVec2(-1.0f, 0.0f)))
                    tools.QueueCratePrice(priceTier, cratePrice);
                ImGui::EndDisabled();

                ImGui::SeparatorText("Unique Special Cargo");
                ImGui::SetNextItemWidth(300.0f);
                ImGui::Combo("Unique item", &uniqueSpecialIndex, UniqueSpecialNames.data(), static_cast<int>(UniqueSpecialNames.size()));
                ImGui::BeginDisabled(toolState.pending);
                if (ImGui::Button("Enable selected unique item", ImVec2(-1.0f, 0.0f)))
                    tools.QueueUniqueSpecialItem(UniqueSpecialValues[static_cast<std::size_t>(uniqueSpecialIndex)]);
                ImGui::EndDisabled();

                if (toolState.pending)
                    ImGui::TextDisabled("Special Cargo action queued on the GTA script thread...");
                else if (toolState.haveResult)
                    ImGui::TextDisabled("%s: %s", toolState.lastSucceeded ? "Success" : "Failed", toolState.message.c_str());
                else
                    ImGui::TextDisabled("Ready. Mission-local actions require their matching contraband script to be active.");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        SetV11Description("Special Cargo is fully rendered inside its business tab: warehouse stock, Lupe sourcing, cooldowns, mission locals, crate prices and unique cargo.");
    }
}
