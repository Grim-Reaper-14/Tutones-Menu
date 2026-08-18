#include "VehiclePaintPanel.hpp"

#include "../features/vehicle/VehiclePaintRuntime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace Tutones::UI
{
    namespace
    {
        using namespace Game::Paint;

        constexpr ImVec4 Accent{147.0f / 255.0f, 190.0f / 255.0f, 66.0f / 255.0f, 1.0f};
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

        struct PaintUiState final
        {
            VehicleHandle vehicle{};
            int primaryPalette{};
            int secondaryPalette{};
            int primaryIndex{};
            int secondaryIndex{};
            int pearlescent{};
            int wheel{};
            float customPrimary[3]{};
            float customSecondary[3]{};
            const char* queueMessage{"Ready"};
        };

        PaintUiState g_PaintUi;

        [[nodiscard]] int PaletteIndexFromNative(NativePaintType type) noexcept
        {
            switch (type)
            {
            case NativePaintType::Normal: return 0;
            case NativePaintType::Metallic: return 1;
            case NativePaintType::Pearl: return 2;
            case NativePaintType::Matte: return 5;
            case NativePaintType::Metal: return 6;
            case NativePaintType::Chrome: return 3;
            }
            return 0;
        }

        [[nodiscard]] const char* NativePaintTypeName(NativePaintType type) noexcept
        {
            switch (type)
            {
            case NativePaintType::Normal: return "Normal";
            case NativePaintType::Metallic: return "Metallic";
            case NativePaintType::Pearl: return "Pearl";
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
            case PaintOperation::Wheel: return "Wheel";
            case PaintOperation::CustomPrimary: return "Custom primary";
            case PaintOperation::CustomSecondary: return "Custom secondary";
            case PaintOperation::ClearCustomPrimary: return "Clear custom primary";
            case PaintOperation::ClearCustomSecondary: return "Clear custom secondary";
            }
            return "Unknown";
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

        void ClampPaintIndex(PaintPalette palette, int& colorIndex) noexcept
        {
            if (palette == PaintPalette::Chameleon)
                colorIndex = std::clamp(colorIndex, 161, 223);
            else
                colorIndex = std::clamp(colorIndex, 0, 160);
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
            ImGui::TextUnformatted(primary ? "Primary finish" : "Secondary finish");
            if (ImGui::Combo(
                    "Finish",
                    &paletteIndex,
                    "Normal\0Metallic\0Pearl\0Chrome\0Classic\0Matte\0Metal\0Utility\0Worn\0Chameleon\0\0"))
            {
                ClampPaintIndex(PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)], colorIndex);
            }

            const PaintPalette palette = PrimarySecondaryPalettes[static_cast<std::size_t>(paletteIndex)];
            const int minIndex = palette == PaintPalette::Chameleon ? 161 : 0;
            const int maxIndex = palette == PaintPalette::Chameleon ? 223 : 160;
            ImGui::SliderInt("Color index", &colorIndex, minIndex, maxIndex);

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
            ImGui::TextUnformatted(primary ? "Custom primary RGB" : "Custom secondary RGB");
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

            ImGui::TextUnformatted("Pearlescent overlay");
            ImGui::SliderInt("Pearlescent index", &g_PaintUi.pearlescent, 0, 160);
            if (ImGui::Button("Apply pearlescent", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(
                    service.QueuePearlescent(g_PaintUi.pearlescent),
                    "Pearlescent queued",
                    "Pearlescent rejected");
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Wheel color");
            ImGui::SliderInt("Wheel index", &g_PaintUi.wheel, 0, 223);
            ImGui::TextDisabled("0-160 standard/alloy/classic, 161-223 chameleon");
            if (ImGui::Button("Apply wheel color", ImVec2(-1.0f, 0.0f)))
            {
                QueueResult(service.QueueWheel(g_PaintUi.wheel), "Wheel color queued", "Wheel color rejected");
            }
        }

        void RenderStatus(const PaintServiceSnapshot& snapshot) noexcept
        {
            const auto& runtime = VehiclePaintRuntime::Get();
            const auto& paint = snapshot.paint;

            ImGui::Text("Runtime: %s", runtime.IsRunning() ? "online" : "offline");
            if (!paint.valid)
            {
                ImGui::TextDisabled("No tracked vehicle paint snapshot yet.");
                ImGui::TextDisabled("Enter a vehicle and wait for the GTA script-thread refresh.");
                return;
            }

            ImGui::Separator();
            ImGui::Text("Vehicle handle: %d", paint.vehicle);
            ImGui::Text("Primary: %s / %d", NativePaintTypeName(paint.primaryPaintType), paint.primaryModColor);
            ImGui::Text("Secondary: %s / %d", NativePaintTypeName(paint.secondaryPaintType), paint.secondaryModColor);
            ImGui::Text("Indexed pair: %d / %d", paint.primaryColor, paint.secondaryColor);
            ImGui::Text("Pearlescent / wheel: %d / %d", paint.pearlescentColor, paint.wheelColor);
            ImGui::Text("Custom primary: %s", paint.primaryCustom ? "yes" : "no");
            ImGui::Text("Custom secondary: %s", paint.secondaryCustom ? "yes" : "no");

            ImGui::Separator();
            ImGui::Text("Last operation: %s", OperationName(snapshot.lastOperation));
            if (snapshot.lastOperation == PaintOperation::None)
                ImGui::TextDisabled("No paint write has run yet.");
            else if (snapshot.lastOperationRejectedAsStale)
                ImGui::TextDisabled("Rejected because the current vehicle changed before execution.");
            else
                ImGui::Text("Result: %s", snapshot.lastOperationSucceeded ? "success" : "failed");
        }
    }

    void RenderVehiclePaintPanel() noexcept
    {
        auto& runtime = VehiclePaintRuntime::Get();
        const PaintServiceSnapshot snapshot = runtime.Snapshot();
        SyncEditor(snapshot.paint);

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 26.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));

        if (ImGui::BeginChild("##vehicle_paint_editor", ImVec2(490.0f, 430.0f), true))
        {
            ImGui::TextColored(Accent, "Vehicle Paint");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", g_PaintUi.queueMessage);
            ImGui::Separator();

            if (!runtime.IsRunning())
            {
                ImGui::TextDisabled("Vehicle paint runtime is offline.");
            }
            else if (!snapshot.paint.valid)
            {
                ImGui::TextDisabled("No vehicle detected yet. Paint actions are disabled until a valid vehicle snapshot is available.");
            }
            else if (ImGui::BeginTabBar("##vehicle_paint_tabs"))
            {
                ImGui::BeginDisabled(!snapshot.paint.valid);
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
                if (ImGui::BeginTabItem("Extras"))
                {
                    RenderExtras();
                    ImGui::EndTabItem();
                }
                ImGui::EndDisabled();
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
