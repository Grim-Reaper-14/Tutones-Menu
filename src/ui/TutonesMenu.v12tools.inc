#include "DeveloperDiagnosticsPanel.hpp"
#include "EnhancedUtilityPanel.hpp"

namespace Tutones::UI
{
    namespace
    {
        inline void RenderV12NativeToolsDashboard(std::size_t page) noexcept
        {
            if (page == 3)
                RenderDeveloperDiagnosticsPanel();
            else
                RenderV12NativeToolsPanel(page);

            auto& toolkit = Game::NativeTools::EnhancedNativeToolkit::Get();
            toolkit.RequestSample();
            const auto state = toolkit.Snapshot();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 min{windowPos.x + 724.0f, windowPos.y + 18.0f};
            const ImVec2 max{windowPos.x + 1008.0f, windowPos.y + 315.0f};
            auto* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(min, max, IM_COL32(10, 13, 18, 249), 7.0f);
            draw->AddRect(min, max, IM_COL32(77, 82, 91, 94), 7.0f, 0, 1.0f);
            draw->AddRectFilled(min, ImVec2(min.x + 3.0f, max.y), ImGui::GetColorU32(V11Theme::Accent), 3.0f);
            draw->AddText(FontOrDefault(g_Bold), 13.0f, ImVec2(min.x + 16.0f, min.y + 15.0f), ImGui::GetColorU32(V11Theme::Accent), "TOOLS STATUS");
            draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 16.0f, min.y + 36.0f), ImGui::GetColorU32(V11Theme::MutedText), "Live Enhanced Native Toolkit state");
            draw->AddLine(ImVec2(min.x + 16.0f, min.y + 58.0f), ImVec2(max.x - 16.0f, min.y + 58.0f), ImGui::GetColorU32(V11Theme::Separator));

            float y = min.y + 76.0f;
            const auto row = [&](const char* label, const char* value, bool good = false)
            {
                draw->AddText(FontOrDefault(g_Medium), 11.0f, ImVec2(min.x + 16.0f, y), ImGui::GetColorU32(V11Theme::MutedText), label);
                const float textWidth = ImGui::CalcTextSize(value).x;
                draw->AddText(
                    FontOrDefault(g_Medium),
                    11.0f,
                    ImVec2(max.x - 16.0f - textWidth, y),
                    good ? IM_COL32(46, 224, 105, 255) : IM_COL32(239, 242, 247, 255),
                    value);
                y += 22.0f;
            };

            char weapon[24]{};
            char tint[16]{};
            char interior[16]{};
            char bodyguards[16]{};
            char probe[32]{};
            std::snprintf(weapon, sizeof(weapon), "0x%08X", state.selectedWeapon);
            std::snprintf(tint, sizeof(tint), "%d", state.weaponTint);
            std::snprintf(interior, sizeof(interior), "%d", state.currentInterior);
            std::snprintf(bodyguards, sizeof(bodyguards), "%zu", state.bodyguardCount);
            if (state.lastProbeResolved)
                std::snprintf(probe, sizeof(probe), "0x%llX", static_cast<unsigned long long>(state.lastProbeAddress));
            else
                std::snprintf(probe, sizeof(probe), "UNRESOLVED");

            row("Native backend", state.nativeReady ? "READY" : "WAITING", state.nativeReady);
            row("Selected weapon", state.selectedWeapon ? weapon : "NONE");
            row("Weapon tint", state.weaponTint >= 0 ? tint : "N/A");
            row("Freecam", state.freecamEnabled ? "ON" : "OFF", state.freecamEnabled);
            row("Vehicle performance", state.vehiclePerformanceEnabled ? "ON" : "OFF", state.vehiclePerformanceEnabled);
            row("Interior", interior);
            row("Bodyguards", bodyguards);
            row("Last native probe", probe, state.lastProbeResolved);
            row("Action", state.pending ? "PENDING" : (state.haveResult ? (state.lastSucceeded ? "SUCCESS" : "FAILED") : "READY"), state.lastSucceeded);

            // Vehicle and World native pages now expose the larger Enhanced
            // utility suite in a separate window so the existing panel layout
            // remains stable while the new controls have room to breathe.
            RenderEnhancedUtilityLauncher(page);
        }
    }
}
