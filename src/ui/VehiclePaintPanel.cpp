#include "VehiclePaintPanel.hpp"

#include "V11Description.hpp"
#include "V11Theme.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../game/GameState.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

namespace Tutones::UI
{
    namespace
    {
        using namespace Game::Paint;
        using Game::VehicleCatalogs::IndexedName;

        // Match YimMenuV2's primary/secondary color families. These are named subsets
        // of GTA's global indexed vehicle colors; custom RGB remains a separate override.
        constexpr std::array<PaintPalette, 7> PrimarySecondaryPalettes{
            PaintPalette::Chrome,
            PaintPalette::Classic,
            PaintPalette::Matte,
            PaintPalette::Metals,
            PaintPalette::Utility,
            PaintPalette::Worn,
            PaintPalette::Chameleon,
        };

        constexpr std::array<const char*, 7> PaletteNames{{
            "Chrome", "Classic", "Matte", "Metals", "Utility", "Worn", "Chameleon",
        }};

        struct PaintUiState final
        {
            VehicleHandle vehicle{};
            int primaryPalette{1};
            int secondaryPalette{1};
            int primaryIndex{};
            int secondaryIndex{};
            int pearlescent{};
            int wheel{};
            int wheelFamily{}; // 0 classic, 1 chameleon
            float customPrimary[3]{};
            float customSecondary[3]{};
            const char* queueMessage{"Ready"};
        };

        PaintUiState g_PaintUi;

        [[nodiscard]] bool ContainsColor(std::span<const IndexedName> colors, int value) noexcept
        {
            for (const auto& entry : colors)
                if (entry.value == value)
                    return true;
            return false;
        }

        [[nodiscard]] int PaletteIndexFromColor(int value) noexcept
        {
            for (std::size_t i = 0; i < PrimarySecondaryPalettes.size(); ++i)
            {
                if (ContainsColor(Game::VehicleCatalogs::ColorsForPalette(PrimarySecondaryPalettes[i]), value))
                    return static_cast<int>(i);
            }

            // Unknown/vehicle-specific indexed colors stay editable through the broad Classic list.
            return 1;
        }

        [[nodiscard]] const char* NativePaintTypeName(NativePaintType type) noexcept
        {
            switch (type)
            {
            case NativePaintType::Normal: return "Normal / Classic";
            case NativePaintType::Metallic: return "Metallic";
            case NativePaintType::Pearl: return "Pearlescent";
            case NativePaintType::Matte: return "Matte";
            case NativePaintType::Metal: return "Metal";
            case NativePaintType::Chrome: return "Chrome";
            }
            return "Unknown";
        }

        [[nodiscard]] const char* OperationName(PaintOperation operation) noexcept
        {
            switch (operation)
            {
            case PaintOperation::None: return "None";
            case PaintOperation::Primary: return "Primary";
            case PaintOperation::Secondary: return "Secondary";
            case PaintOperation::Pearlescent: return "Pearlescent";
            case PaintOperation::Wheel: return "Wheel color";
            case PaintOperation::CustomPrimary: return "Custom primary";
            case PaintOperation::CustomSecondary: return "Custom secondary";
            case PaintOperation::ClearCustomPrimary: return "Clear custom primary";
            case PaintOperation::ClearCustomSecondary: return "Clear custom secondary";
            }
            return "Unknown";
        }

        [[nodiscard]] const char* PreviewFor(std::span<const IndexedName> colors, int value) noexcept
        {
            for (const auto& entry : colors)
                if (entry.value == value)
                    return entry.name;
            return "Choose color";
        }

        bool RenderNamedColorCombo(const char* label, std::span<const IndexedName> colors, int& value, const char* description) noexcept
        {
            bool changed = false;
            if (colors.empty())
                return false;

            if (ImGui::BeginCombo(label, PreviewFor(colors, value)))
            {
                for (const auto& entry : colors)
                {
                    const bool selected = entry.value == value;
                    if (ImGui::Selectable(entry.name, selected))
                    {
                        value = entry.value;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            DescribeLastV11Item(description);
            return changed;
        }

        void ResetColorForPalette(PaintPalette palette, int& colorIndex) noexcept
        {
            const auto colors = Game::VehicleCatalogs::ColorsForPalette(palette);
            if (!colors.empty())
                colorIndex = colors.front().value;
        }

        void SyncEditor(const VehiclePaintState& paint) noexcept
        {
            if (!paint.valid)
            {
                g_PaintUi.vehicle = 0;
                return;
            }
            if (g_PaintUi.vehicle == paint.vehicle)
                return;

            g_PaintUi.vehicle = paint.vehicle;
            g_PaintUi.primaryPalette = PaletteIndexFromColor(paint.primaryColor);
            g_PaintUi.secondaryPalette = PaletteIndexFromColor(paint.secondaryColor);
            g_PaintUi.primaryIndex = paint.primaryColor;
            g_PaintUi.secondaryIndex = paint.secondaryColor;
            g_PaintUi.pearlescent = paint.pearlescentColor;
            g_PaintUi.wheel = paint.wheelColor;
            g_PaintUi.wheelFamily = paint.wheelColor >= 161 ? 1 : 0;

            if (paint.primaryCustom)
            {
                g_PaintUi.customPrimary[0] = static_cast<float>(paint.customPrimary.red) / 255.0f;
                g_PaintUi.customPrimary[1] = static_cast<float>(paint.customPrimary.green) / 255.0f;
                g_PaintUi.customPrimary[2] = static_cast<float>(paint.customPrimary.blue) / 255.0f;
            }
            if (paint.secondaryCustom)
            {
                g_PaintUi.customSecondary[0] = static_cast<float>(paint.customSecondary.red) / 255.0f;
                g_PaintUi.customSecondary[1] = static_cast<float>(paint.customSecondary.green) / 255.0f;
                g_PaintUi.customSecondary[2] = static_cast<float>(paint.customSecondary.blue) / 255.0f;
            }
        }

        [[nodiscard]] RgbColor ToRgb(const float color[3]) noexcept
        {
            const auto convert = [](float value) noexcept {
                return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            return {convert(color[0]), convert(color[1]), convert(color[2])};
        }

        void QueueResult(bool queued, const char* accepted, const char* rejected) noexcept
        {
            g_PaintUi.queueMessage = queued ? accepted : rejected;
        }

        void RenderBasePaintEditor(
            const char* id,
            const char* applyLabel,
            int& paletteIndex,
            int& colorIndex,
            bool primary) noexcept
        {
            ImGui::PushID(id);
            ImGui::TextColored(V11Theme::Accent, primary ? "Primary Paint" : "Secondary Paint");
            if (ImGui::Combo("Color family", &paletteIndex, PaletteNames.data(), static_cast<int>(PaletteNames.size())))
            {
                paletteIndex = std::clamp(paletteIndex, 0, static_cast<int>(PaletteNames.size()) - 1);
                ResetColorForPalette(PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)], colorIndex);
            }
            DescribeLastV11Item(primary
                ? "Choose the Yim-style GTA indexed color family used for the vehicle's primary paint."
                : "Choose the Yim-style GTA indexed color family used for the vehicle's secondary paint.");

            const PaintPalette palette = PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)];
            const auto colors = Game::VehicleCatalogs::ColorsForPalette(palette);
            static_cast<void>(RenderNamedColorCombo(
                "Color",
                colors,
                colorIndex,
                primary
                    ? "Choose the global GTA primary color index from the selected family."
                    : "Choose the global GTA secondary color index from the selected family."));

            ImGui::TextDisabled("GTA color index %d", colorIndex);
            auto& service = VehiclePaintRuntime::Get().Service();
            if (ImGui::Button(applyLabel, ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = primary
                    ? service.QueuePrimary({palette, colorIndex})
                    : service.QueueSecondary({palette, colorIndex});
                QueueResult(
                    queued,
                    primary ? "Primary queued for read-back" : "Secondary queued for read-back",
                    "Paint action rejected");
            }
            DescribeLastV11Item(primary
                ? "Apply the selected primary color while preserving secondary, then verify GTA read-back."
                : "Apply the selected secondary color while preserving primary, then verify GTA read-back.");
            ImGui::PopID();
        }

        void RenderCustomEditor(const char* id, float color[3], bool primary, bool active) noexcept
        {
            ImGui::PushID(id);
            ImGui::Separator();
            ImGui::TextColored(V11Theme::Accent, primary ? "Custom Primary RGB" : "Custom Secondary RGB");
            ImGui::SameLine();
            ImGui::TextDisabled(active ? "active" : "inactive");
            ImGui::ColorEdit3("RGB", color, ImGuiColorEditFlags_NoAlpha);
            DescribeLastV11Item(primary
                ? "Pick a custom RGB override for the primary vehicle color."
                : "Pick a custom RGB override for the secondary vehicle color.");

            auto& service = VehiclePaintRuntime::Get().Service();
            if (ImGui::Button("Apply custom RGB", ImVec2(180.0f, 0.0f)))
            {
                const RgbColor rgb = ToRgb(color);
                const bool queued = primary ? service.QueueCustomPrimary(rgb) : service.QueueCustomSecondary(rgb);
                QueueResult(queued, "Custom RGB queued for read-back", "Custom RGB rejected");
            }
            DescribeLastV11Item(primary
                ? "Apply and verify the selected custom RGB override for the primary color."
                : "Apply and verify the selected custom RGB override for the secondary color.");
            ImGui::SameLine();
            if (ImGui::Button("Clear custom", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = primary ? service.QueueClearCustomPrimary() : service.QueueClearCustomSecondary();
                QueueResult(queued, "Clear custom queued for read-back", "Clear custom rejected");
            }
            DescribeLastV11Item(primary
                ? "Remove the custom primary RGB override and verify indexed paint is active again."
                : "Remove the custom secondary RGB override and verify indexed paint is active again.");
            ImGui::PopID();
        }

        void RenderExtras() noexcept
        {
            auto& service = VehiclePaintRuntime::Get().Service();

            ImGui::TextColored(V11Theme::Accent, "Pearlescent Overlay");
            static_cast<void>(RenderNamedColorCombo(
                "Pearlescent",
                Game::VehicleCatalogs::ClassicColors,
                g_PaintUi.pearlescent,
                "Choose the indexed pearlescent overlay color used with compatible vehicle paint finishes."));
            if (ImGui::Button("Apply pearlescent", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(service.QueuePearlescent(g_PaintUi.pearlescent), "Pearlescent queued for read-back", "Pearlescent rejected");
            }
            DescribeLastV11Item("Apply the selected pearlescent overlay while preserving wheel color, then verify read-back.");

            ImGui::Separator();
            ImGui::TextColored(V11Theme::Accent, "Wheel Color");
            const char* wheelFamilies[] = {"LSC / Classic", "Chameleon"};
            if (ImGui::Combo("Wheel color family", &g_PaintUi.wheelFamily, wheelFamilies, 2))
            {
                const auto colors = g_PaintUi.wheelFamily == 1
                    ? std::span<const IndexedName>(Game::VehicleCatalogs::ChameleonColors)
                    : std::span<const IndexedName>(Game::VehicleCatalogs::ClassicColors);
                if (!colors.empty())
                    g_PaintUi.wheel = colors.front().value;
            }
            DescribeLastV11Item("Switch the wheel-color catalog between standard LSC/classic colors and supported chameleon colors.");
            const auto wheelColors = g_PaintUi.wheelFamily == 1
                ? std::span<const IndexedName>(Game::VehicleCatalogs::ChameleonColors)
                : std::span<const IndexedName>(Game::VehicleCatalogs::ClassicColors);
            static_cast<void>(RenderNamedColorCombo(
                "Wheel color",
                wheelColors,
                g_PaintUi.wheel,
                "Choose the indexed wheel color from the selected wheel-color family."));
            if (ImGui::Button("Apply wheel color", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(service.QueueWheel(g_PaintUi.wheel), "Wheel color queued for read-back", "Wheel color rejected");
            }
            DescribeLastV11Item("Apply the selected wheel color while preserving pearlescent, then verify read-back.");
        }

        void RenderStatus(const PaintServiceSnapshot& snapshot) noexcept
        {
            const auto& paint = snapshot.paint;
            ImGui::TextColored(V11Theme::Accent, "Paint Runtime Status");
            ImGui::Text("Vehicle: %d", paint.vehicle);
            ImGui::Text("Primary indexed color: %d", paint.primaryColor);
            ImGui::Text("Secondary indexed color: %d", paint.secondaryColor);
            ImGui::Text("Pearl / wheel: %d / %d", paint.pearlescentColor, paint.wheelColor);
            ImGui::Text("Custom primary: %s", paint.primaryCustom ? "yes" : "no");
            ImGui::Text("Custom secondary: %s", paint.secondaryCustom ? "yes" : "no");
            ImGui::TextDisabled(
                "Native metadata: P %s/%d | S %s/%d",
                NativePaintTypeName(paint.primaryPaintType),
                paint.primaryModColor,
                NativePaintTypeName(paint.secondaryPaintType),
                paint.secondaryModColor);
            ImGui::Separator();
            ImGui::Text("Last operation: %s", OperationName(snapshot.lastOperation));
            if (snapshot.lastOperation != PaintOperation::None)
            {
                if (snapshot.lastOperationRejectedAsStale)
                    ImGui::TextDisabled("Result: vehicle changed before execution.");
                else
                    ImGui::Text("Result: %s", snapshot.lastOperationSucceeded ? "verified" : "failed verification");
            }
        }
    }

    void RenderVehiclePaintPanel() noexcept
    {
        auto& runtime = Game::Paint::VehiclePaintRuntime::Get();
        const auto snapshot = runtime.Snapshot();
        const auto gameState = Game::GameState::Get().Snapshot();
        SyncEditor(snapshot.paint);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild("##vehicle_paint", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(V11Theme::Accent, "Vehicle Paint");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", g_PaintUi.queueMessage);
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle paint runtime is offline.");
            }
            else if (!gameState.inVehicle || gameState.vehicle == 0)
            {
                ImGui::TextDisabled("No vehicle detected. Enter a vehicle first.");
            }
            else if (!snapshot.paint.valid)
            {
                ImGui::Text("Vehicle detected: %d", gameState.vehicle);
                ImGui::TextDisabled("Paint state is refreshing from GTA indexed colors.");
            }
            else if (ImGui::BeginTabBar("##vehicle_paint_tabs"))
            {
                if (ImGui::BeginTabItem("Primary"))
                {
                    RenderBasePaintEditor("primary", "Apply primary paint", g_PaintUi.primaryPalette, g_PaintUi.primaryIndex, true);
                    RenderCustomEditor("primary_custom", g_PaintUi.customPrimary, true, snapshot.paint.primaryCustom);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Secondary"))
                {
                    RenderBasePaintEditor("secondary", "Apply secondary paint", g_PaintUi.secondaryPalette, g_PaintUi.secondaryIndex, false);
                    RenderCustomEditor("secondary_custom", g_PaintUi.customSecondary, false, snapshot.paint.secondaryCustom);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Pearl / Wheels"))
                {
                    RenderExtras();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Status"))
                {
                    RenderStatus(snapshot);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
