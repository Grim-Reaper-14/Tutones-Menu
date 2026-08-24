#pragma once

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/recovery/SpecialCargoToolsRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace Tutones::UI
{
    inline void RenderSpecialCargoToolsControl() noexcept
    {
        using Game::Recovery::SpecialCargoToolsRuntime;

        auto& runtime = SpecialCargoToolsRuntime::Get();
        const auto state = runtime.Snapshot();

        ImGui::SetCursorPos(ImVec2(575.0f, 380.0f));
        ImGui::BeginDisabled(state.pending);
        if (ImGui::Button("Special Cargo Tools...", ImVec2(141.0f, 28.0f)))
            ImGui::OpenPopup("##special_cargo_tools_popup");
        ImGui::EndDisabled();
        DescribeLastV11Item("Open Enhanced 1.73 Special Cargo controls for Lupe sourcing, cooldowns, mission locals, crate price globals, and unique special cargo.");

        ImGui::SetNextWindowSize(ImVec2(560.0f, 575.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##special_cargo_tools_popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
        {
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
                "Any",
                "Medical Supplies",
                "Tobacco & Alcohol",
                "Art & Antiques",
                "Electronic Goods",
                "Weapons & Ammo",
                "Narcotics",
                "Gemstones",
                "Animal Materials",
                "Counterfeit Goods",
                "Jewelry",
                "Bullion"
            };

            constexpr std::array<const char*, 6> LupeSpecialNames{
                "Ornamental Egg",
                "Golden Minigun",
                "Extra Large Diamond",
                "Sasquatch Hide",
                "Film Reel",
                "Rare Pocket Watch"
            };

            constexpr std::array<const char*, 21> PriceTierNames{
                "1 crate  (f_15825)",
                "2 crates (f_15826)",
                "3 crates (f_15827)",
                "4-5 crates (f_15828)",
                "6-7 crates (f_15829)",
                "8-9 crates (f_15830)",
                "10-14 crates (f_15831)",
                "15-19 crates (f_15832)",
                "20-24 crates (f_15833)",
                "25-29 crates (f_15834)",
                "30-34 crates (f_15835)",
                "35-39 crates (f_15836)",
                "40-44 crates (f_15837)",
                "45-49 crates (f_15838)",
                "50-59 crates (f_15839)",
                "60-69 crates (f_15840)",
                "70-79 crates (f_15841)",
                "80-89 crates (f_15842)",
                "90-99 crates (f_15843)",
                "100-110 crates (f_15844)",
                "111 crates (f_15845)"
            };

            constexpr std::array<const char*, 6> UniqueSpecialNames{
                "Ornamental Egg",
                "Golden Minigun",
                "Large Diamond",
                "Rare Hide",
                "Film Reel",
                "Rare Pocket Watch"
            };
            constexpr std::array<int, 6> UniqueSpecialValues{2, 4, 6, 7, 8, 9};

            ImGui::TextColored(V11Theme::Accent, "Special Cargo Tools");
            ImGui::SameLine();
            ImGui::TextDisabled("Enhanced 1.73 / b1158.13");
            ImGui::Separator();

            ImGui::TextColored(V11Theme::Accent, "Lupe Sourcing");
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

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply Lupe sourcing globals", ImVec2(-1.0f, 0.0f)))
                runtime.QueueSourcingSettings(sourcingAmount, cargoType, lupeSpecialItem, lupeSpecialAvailable);
            ImGui::EndDisabled();
            DescribeLastV11Item("Write Global_1882762.f_13, f_16, f_14 and f_15 using the selected Enhanced sourcing values.");

            ImGui::SeparatorText("Mission Controls");
            ImGui::SetNextItemWidth(190.0f);
            ImGui::InputInt("Buy cooldown (ms)", &buyCooldown, 1000, 10000);
            ImGui::SetNextItemWidth(190.0f);
            ImGui::InputInt("Sell cooldown (ms)", &sellCooldown, 1000, 10000);
            buyCooldown = std::max(0, buyCooldown);
            sellCooldown = std::max(0, sellCooldown);

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply cooldown globals", ImVec2(255.0f, 0.0f)))
                runtime.QueueCooldowns(buyCooldown, sellCooldown);
            ImGui::SameLine();
            if (ImGui::Button("Remove cooldowns", ImVec2(-1.0f, 0.0f)))
                runtime.QueueCooldowns(0, 0);
            ImGui::EndDisabled();
            DescribeLastV11Item("Write Global_262145.f_15592 and f_15593. Defaults shown are 300000ms buy and 1800000ms sell from the supplied Enhanced mapping.");

            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Instant Buy (active mission)", ImVec2(255.0f, 0.0f)))
                runtime.QueueInstantBuy();
            ImGui::SameLine();
            if (ImGui::Button("Instant Sell (active mission)", ImVec2(-1.0f, 0.0f)))
                runtime.QueueInstantSell();
            ImGui::EndDisabled();
            DescribeLastV11Item("Apply the supplied gb_contraband_buy or gb_contraband_sell locals. The matching script must currently be active.");

            ImGui::SeparatorText("Crate Price Global");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::Combo("Price tier", &priceTier, PriceTierNames.data(), static_cast<int>(PriceTierNames.size()));
            cratePrice = std::max(0, cratePrice);
            ImGui::SetNextItemWidth(190.0f);
            ImGui::InputInt("Price value", &cratePrice, 1000, 10000);
            cratePrice = std::max(0, cratePrice);
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Apply selected price", ImVec2(-1.0f, 0.0f)))
                runtime.QueueCratePrice(priceTier, cratePrice);
            ImGui::EndDisabled();
            DescribeLastV11Item("Write the selected Global_262145.f_15825 through f_15845 crate-price tier. No price values are assumed by Tutones.");

            ImGui::SeparatorText("Unique Special Cargo");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::Combo("Unique item", &uniqueSpecialIndex, UniqueSpecialNames.data(), static_cast<int>(UniqueSpecialNames.size()));
            ImGui::BeginDisabled(state.pending);
            if (ImGui::Button("Enable selected unique item", ImVec2(-1.0f, 0.0f)))
                runtime.QueueUniqueSpecialItem(UniqueSpecialValues[static_cast<std::size_t>(uniqueSpecialIndex)]);
            ImGui::EndDisabled();
            DescribeLastV11Item("Set Global_1950921 to the selected supplied unique-item value, then set Global_1951074 = true.");

            if (state.pending)
                ImGui::TextDisabled("Special Cargo action queued on the GTA script thread...");
            else if (state.haveResult)
                ImGui::TextDisabled("%s: %s", state.lastSucceeded ? "Success" : "Failed", state.message.c_str());
            else
                ImGui::TextDisabled("Ready. Mission-local actions require their matching contraband script to be active.");

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(-1.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }
}
