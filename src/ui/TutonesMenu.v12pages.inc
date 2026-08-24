namespace Tutones::UI
{
    namespace
    {
        template<typename Fn>
        void RenderV12PageSurface(Fn&& render) noexcept
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const float height = ImGui::GetWindowHeight();
            auto* draw = ImGui::GetWindowDrawList();

            // Legacy feature pages were built around x=226. Keep that coordinate
            // contract intact while placing every page on the same V12 dashboard
            // surface. This lets the UI be redesigned without touching runtime code.
            const ImVec2 min{windowPos.x + 206.0f, windowPos.y + 7.0f};
            const ImVec2 max{windowPos.x + 1025.0f, windowPos.y + std::max(505.0f, height - 12.0f)};

            draw->AddRectFilled(min, max, IM_COL32(8, 11, 16, 246), 8.0f);
            draw->AddRect(min, max, IM_COL32(77, 82, 91, 92), 8.0f, 0, 1.0f);
            draw->AddRectFilled(
                ImVec2(min.x + 1.0f, min.y + 1.0f),
                ImVec2(min.x + 4.0f, max.y - 1.0f),
                ImGui::GetColorU32(ImVec4(V11Theme::Accent.x, V11Theme::Accent.y, V11Theme::Accent.z, 0.78f)),
                4.0f);
            draw->AddLine(
                ImVec2(min.x + 16.0f, min.y + 1.0f),
                ImVec2(min.x + 174.0f, min.y + 1.0f),
                ImGui::GetColorU32(ImVec4(V11Theme::Accent.x, V11Theme::Accent.y, V11Theme::Accent.z, 0.55f)),
                1.5f);

            // One-column legacy pages intentionally occupy the left side of the
            // workspace. Until a page is converted to a native two-column V12
            // layout, use the spare area as a guide card instead of dead space.
            const ImVec2 guideMin{windowPos.x + 724.0f, windowPos.y + 18.0f};
            const ImVec2 guideMax{windowPos.x + 1008.0f, windowPos.y + 246.0f};
            draw->AddRectFilled(guideMin, guideMax, IM_COL32(10, 13, 18, 247), 7.0f);
            draw->AddRect(guideMin, guideMax, IM_COL32(77, 82, 91, 88), 7.0f, 0, 1.0f);
            draw->AddText(
                FontOrDefault(g_Bold),
                13.0f,
                ImVec2(guideMin.x + 16.0f, guideMin.y + 15.0f),
                ImGui::GetColorU32(V11Theme::Accent),
                "PAGE GUIDE");
            draw->AddText(
                FontOrDefault(g_Medium),
                12.0f,
                ImVec2(guideMin.x + 16.0f, guideMin.y + 46.0f),
                ImGui::GetColorU32(V11Theme::MutedText),
                CurrentV11Description() ? CurrentV11Description() : "Use the controls on this page to manage the selected feature.",
                nullptr,
                252.0f);
            draw->AddLine(
                ImVec2(guideMin.x + 16.0f, guideMax.y - 48.0f),
                ImVec2(guideMax.x - 16.0f, guideMax.y - 48.0f),
                ImGui::GetColorU32(V11Theme::Separator),
                1.0f);
            draw->AddText(
                FontOrDefault(g_Medium),
                11.0f,
                ImVec2(guideMin.x + 16.0f, guideMax.y - 34.0f),
                ImGui::GetColorU32(V11Theme::MutedText),
                "Hover controls for live help on the right rail.");

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
            ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);
            render();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }

        inline void RenderV12PlayerPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderPlayerPanel(page); });
        }

        inline void RenderV12PlayerOnlinePanel() noexcept
        {
            RenderV12PageSurface([] { RenderPlayerOnlinePanel(); });
        }

        inline void RenderV12WeaponPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderWeaponPanel(page); });
        }

        inline void RenderV12VehicleGeneralPanel() noexcept
        {
            RenderV12PageSurface([] { RenderVehicleGeneralPanel(); });
        }

        inline void RenderV12VehiclePaintPanel() noexcept
        {
            RenderV12PageSurface([] { RenderVehiclePaintPanel(); });
        }

        inline void RenderV12VehicleModificationPanel() noexcept
        {
            RenderV12PageSurface([] { RenderVehicleModificationPanel(); });
        }

        inline void RenderV12PersonalVehiclePanel() noexcept
        {
            RenderV12PageSurface([] { RenderPersonalVehiclePanel(); });
        }

        inline void RenderV12GamePanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderGamePanel(page); });
        }

        inline void RenderV12NetworkPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderNetworkPanel(page); });
        }

        inline void RenderV12WorldPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderWorldPanel(page); });
        }

        inline void RenderV12BusinessPanel() noexcept
        {
            RenderV12PageSurface([] { RenderBusinessPanel(); });
        }

        inline void RenderV12RecoveryPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderRecoveryPanel(page); });
        }

        inline void RenderV12CasinoPanel() noexcept
        {
            RenderV12PageSurface([] { RenderCasinoLuckyWheelPanel(); });
        }

        inline void RenderV12ProtectionPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderProtectionPanel(page); });
        }

        inline void RenderV12SettingsPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderSettingsPanel(page); });
        }

        inline void RenderV12MiscPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderMiscPanel(page); });
        }

        inline void RenderV12NativeToolsPanel(std::size_t page) noexcept
        {
            RenderV12PageSurface([&] { RenderNativeToolsPanel(page); });
        }
    }
}
