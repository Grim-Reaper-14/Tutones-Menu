#pragma once

#include "DeveloperDiagnosticsPanel.hpp"
#include "V11Theme.hpp"
#include "../game/script/ScriptGlobal.hpp"
#include "../game/script/ScriptRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

namespace Tutones::UI
{
    namespace DeveloperDiagnosticsDetail
    {
        inline bool g_ValueDiffOpen{};
        inline int g_ValueDiffGlobalIndex{1985024};
        inline bool g_GlobalBaselineValid{};
        inline int g_GlobalBaselineIndex{-1};
        inline std::int64_t g_GlobalBaselineRaw{};
        inline bool g_LocalBaselineValid{};
        inline std::uint32_t g_LocalBaselineScriptHash{};
        inline int g_LocalBaselineIndex{-1};
        inline std::int64_t g_LocalBaselineRaw{};

        inline constexpr int MaxGlobalRangeWatch = 32;
        inline int g_GlobalRangeBase{1985024};
        inline int g_GlobalRangeCount{8};
        inline bool g_GlobalRangeBaselineValid{};
        inline int g_GlobalRangeBaselineBase{-1};
        inline int g_GlobalRangeBaselineCount{};
        inline std::array<std::int64_t, MaxGlobalRangeWatch> g_GlobalRangeBaseline{};
        inline std::array<bool, MaxGlobalRangeWatch> g_GlobalRangeBaselineReadable{};

        inline void RenderRawValue(const char* prefix, std::int64_t raw) noexcept
        {
            std::int32_t intValue{};
            float floatValue{};
            std::memcpy(&intValue, &raw, sizeof(intValue));
            std::memcpy(&floatValue, &raw, sizeof(floatValue));
            ImGui::Text(
                "%s raw 0x%016llX",
                prefix,
                static_cast<unsigned long long>(raw));
            ImGui::TextDisabled(
                "INT %d | BOOL %s | FLOAT %.6f",
                intValue,
                intValue != 0 ? "true" : "false",
                floatValue);
        }

        inline void RenderBaselineDelta(std::int64_t baseline, std::int64_t current) noexcept
        {
            std::int32_t baselineInt{};
            std::int32_t currentInt{};
            float baselineFloat{};
            float currentFloat{};
            std::memcpy(&baselineInt, &baseline, sizeof(baselineInt));
            std::memcpy(&currentInt, &current, sizeof(currentInt));
            std::memcpy(&baselineFloat, &baseline, sizeof(baselineFloat));
            std::memcpy(&currentFloat, &current, sizeof(currentFloat));

            const bool changed = baseline != current;
            ImGui::TextColored(
                changed ? ImVec4(1.0f, 0.72f, 0.20f, 1.0f) : ImVec4(0.30f, 0.90f, 0.45f, 1.0f),
                "%s",
                changed ? "CHANGED" : "UNCHANGED");
            ImGui::TextDisabled(
                "Baseline 0x%016llX | INT delta %+d | FLOAT delta %+.6f",
                static_cast<unsigned long long>(baseline),
                currentInt - baselineInt,
                currentFloat - baselineFloat);
        }

        inline void RenderGlobalRangeWatch(std::int64_t** globals) noexcept
        {
            ImGui::SeparatorText("Global Range Watch");
            ImGui::TextWrapped(
                "Watch up to 32 adjacent globals at once. Capture a baseline, perform one GTA action, then changed slots light up without writing anything.");
            ImGui::InputInt("Range base", &g_GlobalRangeBase, 1, 100);
            ImGui::InputInt("Range count", &g_GlobalRangeCount, 1, 4);
            g_GlobalRangeCount = std::clamp(g_GlobalRangeCount, 1, MaxGlobalRangeWatch);
            g_GlobalRangeBase = std::clamp(g_GlobalRangeBase, 0, 0xFFFFFF - g_GlobalRangeCount + 1);

            if (ImGui::Button("Capture Global Range Baseline", ImVec2(-1.0f, 0.0f)))
            {
                g_GlobalRangeBaselineValid = true;
                g_GlobalRangeBaselineBase = g_GlobalRangeBase;
                g_GlobalRangeBaselineCount = g_GlobalRangeCount;
                g_GlobalRangeBaselineReadable.fill(false);

                if (globals)
                {
                    for (int offset = 0; offset < g_GlobalRangeCount; ++offset)
                    {
                        const auto* slot = Game::Script::ScriptGlobal(
                            static_cast<std::size_t>(g_GlobalRangeBase + offset)).As<std::int64_t>(globals);
                        if (!slot)
                            continue;
                        g_GlobalRangeBaseline[static_cast<std::size_t>(offset)] = *slot;
                        g_GlobalRangeBaselineReadable[static_cast<std::size_t>(offset)] = true;
                    }
                }
            }

            const bool matchingBaseline = g_GlobalRangeBaselineValid
                && g_GlobalRangeBaselineBase == g_GlobalRangeBase
                && g_GlobalRangeBaselineCount == g_GlobalRangeCount;

            if (g_GlobalRangeBaselineValid && !matchingBaseline)
            {
                ImGui::TextDisabled(
                    "Range baseline belongs to Global_%d .. Global_%d.",
                    g_GlobalRangeBaselineBase,
                    g_GlobalRangeBaselineBase + g_GlobalRangeBaselineCount - 1);
            }

            if (ImGui::BeginTable(
                    "##global_range_watch",
                    4,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Global", ImGuiTableColumnFlags_WidthStretch, 1.3f);
                ImGui::TableSetupColumn("INT", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                for (int offset = 0; offset < g_GlobalRangeCount; ++offset)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const int index = g_GlobalRangeBase + offset;
                    ImGui::Text("Global_%d", index);

                    const auto* slot = globals
                        ? Game::Script::ScriptGlobal(static_cast<std::size_t>(index)).As<std::int64_t>(globals)
                        : nullptr;
                    if (!slot)
                    {
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("N/A");
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("--");
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextDisabled("UNREADABLE");
                        continue;
                    }

                    std::int32_t currentInt{};
                    std::memcpy(&currentInt, slot, sizeof(currentInt));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", currentInt);

                    const bool baselineReadable = matchingBaseline
                        && g_GlobalRangeBaselineReadable[static_cast<std::size_t>(offset)];
                    if (!baselineReadable)
                    {
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("--");
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextDisabled("NO BASELINE");
                        continue;
                    }

                    const auto baseline = g_GlobalRangeBaseline[static_cast<std::size_t>(offset)];
                    std::int32_t baselineInt{};
                    std::memcpy(&baselineInt, &baseline, sizeof(baselineInt));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%+d", currentInt - baselineInt);
                    ImGui::TableSetColumnIndex(3);
                    if (*slot != baseline)
                        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.20f, 1.0f), "CHANGED");
                    else
                        ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.45f, 1.0f), "SAME");
                }

                ImGui::EndTable();
            }
        }

        inline void RenderScriptValueDiffWindow() noexcept
        {
            if (!g_ValueDiffOpen)
                return;

            ImGui::SetNextWindowSize(ImVec2(560.0f, 650.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Tutones Script Value Diff", &g_ValueDiffOpen))
            {
                ImGui::End();
                return;
            }

            auto& runtime = Game::Script::ScriptRuntime::Get();
            auto** globals = runtime.Globals();

            ImGui::TextColored(V11Theme::Accent, "Baseline -> Action -> Compare");
            ImGui::TextWrapped("Capture values, perform one action in GTA, then watch exactly what changed. Reads only; this tool never writes globals or locals.");

            ImGui::SeparatorText("Single Global Watch");
            ImGui::InputInt("Global index", &g_ValueDiffGlobalIndex, 1, 100);
            g_ValueDiffGlobalIndex = std::clamp(g_ValueDiffGlobalIndex, 0, 0xFFFFFF);

            std::int64_t* globalSlot{};
            if (globals)
            {
                globalSlot = Game::Script::ScriptGlobal(static_cast<std::size_t>(g_ValueDiffGlobalIndex))
                    .As<std::int64_t>(globals);
            }

            if (!globalSlot)
            {
                ImGui::TextDisabled("Global_%d is unavailable.", g_ValueDiffGlobalIndex);
            }
            else
            {
                const std::int64_t current = *globalSlot;
                RenderRawValue("Current", current);
                if (ImGui::Button("Capture Global Baseline", ImVec2(-1.0f, 0.0f)))
                {
                    g_GlobalBaselineValid = true;
                    g_GlobalBaselineIndex = g_ValueDiffGlobalIndex;
                    g_GlobalBaselineRaw = current;
                }

                if (g_GlobalBaselineValid && g_GlobalBaselineIndex == g_ValueDiffGlobalIndex)
                    RenderBaselineDelta(g_GlobalBaselineRaw, current);
                else if (g_GlobalBaselineValid)
                    ImGui::TextDisabled("Baseline belongs to Global_%d.", g_GlobalBaselineIndex);
            }

            RenderGlobalRangeWatch(globals);

            ImGui::SeparatorText("Selected Script Local Watch");
            ImGui::Text(
                "Script 0x%08X | Local %d",
                g_SelectedScriptHash,
                g_ScriptLocalIndex);
            ImGui::TextDisabled("Script and local index follow the selection in Developer Diagnostics -> Scripts.");

            const auto localRaw = g_SelectedScriptHash != 0
                ? runtime.ReadLocalRaw(g_SelectedScriptHash, static_cast<std::size_t>(std::max(0, g_ScriptLocalIndex)))
                : std::nullopt;
            if (!localRaw)
            {
                ImGui::TextDisabled("Select a running script/local with a readable stack slot first.");
            }
            else
            {
                RenderRawValue("Current", *localRaw);
                if (ImGui::Button("Capture Local Baseline", ImVec2(-1.0f, 0.0f)))
                {
                    g_LocalBaselineValid = true;
                    g_LocalBaselineScriptHash = g_SelectedScriptHash;
                    g_LocalBaselineIndex = g_ScriptLocalIndex;
                    g_LocalBaselineRaw = *localRaw;
                }

                if (g_LocalBaselineValid
                    && g_LocalBaselineScriptHash == g_SelectedScriptHash
                    && g_LocalBaselineIndex == g_ScriptLocalIndex)
                {
                    RenderBaselineDelta(g_LocalBaselineRaw, *localRaw);
                }
                else if (g_LocalBaselineValid)
                {
                    ImGui::TextDisabled(
                        "Baseline belongs to script 0x%08X local %d.",
                        g_LocalBaselineScriptHash,
                        g_LocalBaselineIndex);
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Clear Baselines", ImVec2(-1.0f, 0.0f)))
            {
                g_GlobalBaselineValid = false;
                g_LocalBaselineValid = false;
                g_GlobalRangeBaselineValid = false;
                g_GlobalRangeBaselineReadable.fill(false);
            }

            ImGui::End();
        }

        inline void RenderScriptValueDiffLauncher(bool developerPage) noexcept
        {
            if (!developerPage)
                return;

            ImGui::SetCursorPos(ImVec2(724.0f, 324.0f));
            if (ImGui::Button("Open Script Value Diff", ImVec2(284.0f, 34.0f)))
                g_ValueDiffOpen = true;

            RenderScriptValueDiffWindow();
        }
    }
}
