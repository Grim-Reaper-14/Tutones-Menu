#include "VehiclePaintPanel.hpp"

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

        constexpr std::array<PaintPalette, 10> PrimarySecondaryPalettes{
            PaintPalette::Normal,
            PaintPalette::Metallic,
            PaintPalette::Pearl,
            PaintPalette::Chrome,
            PaintPalette::Classic,
            PaintPalette::Matte,
            PaintPalette::Metals,
            PaintPalette::Utility,
            PaintPalette::Worn,
            PaintPalette::Chameleon,
        };

        constexpr std::array<const char*, 10> PaletteNames{{
            "Normal", "Metallic", "Pearlescent", "Chrome", "Classic",
            "Matte", "Metals", "Utility", "Worn", "Chameleon",
        }};

        struct PaintUiState final
        {
            VehicleHandle vehicle{};
            int primaryPalette{};
            int secondaryPalette{};
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

        [[nodiscard]] int PaletteIndexFromNative(NativePaintType type) noexcept
        {
            switch (type)
            {
            case NativePaintType::Normal: return 4;
            case NativePaintType::Metallic: return 1;
            case NativePaintType::Pearl: return 2;
            case NativePaintType::Matte: return 5;
            case NativePaintType::Metal: return 6;
            case NativePaintType::Chrome: return 3;
            }
            return 4;
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

        bool RenderNamedColorCombo(const char* label, std::span<const IndexedName> colors, int& value) noexcept
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
            g_PaintUi.primaryPalette = paint.primaryColor >= 161 ? 9 : PaletteIndexFromNative(paint.primaryPaintType);
            g_PaintUi.secondaryPalette = paint.secondaryColor >= 161 ? 9 : PaletteIndexFromNative(paint.secondaryPaintType);
            g_PaintUi.primaryIndex = paint.primaryColor >= 161 ? paint.primaryColor : paint.primaryModColor;
            g_PaintUi.secondaryIndex = paint.secondaryColor >= 161 ? paint.secondaryColor : paint.secondaryModColor;
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
            ImGui::TextColored(V11Theme::Accent, primary ? "Primary LSC Paint" : "Secondary LSC Paint");
            if (ImGui::Combo("Finish", &paletteIndex, PaletteNames.data(), static_cast<int>(PaletteNames.size())))
            {
                paletteIndex = std::clamp(paletteIndex, 0, static_cast<int>(PaletteNames.size()) - 1);
                ResetColorForPalette(PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)], colorIndex);
            }

            const PaintPalette palette = PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)];
            const auto colors = Game::VehicleCatalogs::ColorsForPalette(palette);
            static_cast<void>(RenderNamedColorCombo("Color", colors, colorIndex));

            ImGui::TextDisabled("Index %d", colorIndex);
            auto& service = VehiclePaintRuntime::Get().Service();
            if (ImGui::Button(applyLabel, ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = primary
                    ? service.QueuePrimary({palette, colorIndex})
                    : service.QueueSecondary({palette, colorIndex});
                QueueResult(queued, primary ? "Primary paint queued" : "Secondary paint queued", "Paint action rejected");
            }
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

            auto& service = VehiclePaintRuntime::Get().Service();
            if (ImGui::Button("Apply custom RGB", ImVec2(180.0f, 0.0f)))
            {
                const RgbColor rgb = ToRgb(color);
                const bool queued = primary ? service.QueueCustomPrimary(rgb) : service.QueueCustomSecondary(rgb);
                QueueResult(queued, "Custom RGB queued", "Custom RGB rejected");
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear custom", ImVec2(-1.0f, 0.0f)))
            {
                const bool queued = primary ? service.QueueClearCustomPrimary() : service.QueueClearCustomSecondary();
                QueueResult(queued, "Clear custom queued", "Clear custom rejected");
            }
            ImGui::PopID();
        }

        void RenderExtras() noexcept
        {
            auto& service = VehiclePaintRuntime::Get().Service();

            ImGui::TextColored(V11Theme::Accent, "Pearlescent Overlay");
            static_cast<void>(RenderNamedColorCombo("Pearlescent", Game::VehicleCatalogs::ClassicColors, g_PaintUi.pearlescent));
            if (ImGui::Button("Apply pearlescent", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(service.QueuePearlescent(g_PaintUi.pearlescent), "Pearlescent queued", "Pearlescent rejected");
            }

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
            const auto wheelColors = g_PaintUi.wheelFamily == 1
                ? std::span<const IndexedName>(Game::VehicleCatalogs::ChameleonColors)
                : std::span<const IndexedName>(Game::VehicleCatalogs::ClassicColors);
            static_cast<void>(RenderNamedColorCombo("Wheel color", wheelColors, g_PaintUi.wheel));
            if (ImGui::Button("Apply wheel color", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(service.QueueWheel(g_PaintUi.wheel), "Wheel color queued", "Wheel color rejected");
            }
        }

        void RenderStatus(const PaintServiceSnapshot& snapshot) noexcept
        {
            const auto& paint = snapshot.paint;
            ImGui::TextColored(V11Theme::Accent, "Paint Runtime Status");
            ImGui::Text("Vehicle: %d", paint.vehicle);
            ImGui::Text("Primary finish: %s", NativePaintTypeName(paint.primaryPaintType));
            ImGui::Text("Primary color: %d", paint.primaryModColor);
            ImGui::Text("Secondary finish: %s", NativePaintTypeName(paint.secondaryPaintType));
            ImGui::Text("Secondary color: %d", paint.secondaryModColor);
            ImGui::Text("Indexed pair: %d / %d", paint.primaryColor, paint.secondaryColor);
            ImGui::Text("Pearl / wheel: %d / %d", paint.pearlescentColor, paint.wheelColor);
            ImGui::Text("Custom primary: %s", paint.primaryCustom ? "yes" : "no");
            ImGui::Text("Custom secondary: %s", paint.secondaryCustom ? "yes" : "no");
            ImGui::Separator();
            ImGui::Text("Last operation: %s", OperationName(snapshot.lastOperation));
            if (snapshot.lastOperation != PaintOperation::None)
            {
                if (snapshot.lastOperationRejectedAsStale)
                    ImGui::TextDisabled("Rejected: vehicle changed before execution.");
                else
                    ImGui::Text("Result: %s", snapshot.lastOperationSucceeded ? "success" : "failed");
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
            ImGui::TextColored(V11Theme::Accent, "LSC Paint Catalog");
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
                ImGui::TextDisabled("Paint state is refreshing. Base paint reads no longer depend on optional custom metadata.");
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
