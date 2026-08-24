namespace Tutones::UI
{
    namespace
    {
        void ApplyV12Style() noexcept
        {
            auto& themes = ThemeManager::Get();
            const auto& file = themes.CurrentThemeFile();
            const auto& definition = themes.Current();
            const bool shippedDefault = file.empty()
                || file == "default.json"
                || definition.name == "Default V11"
                || definition.name == "Tutones V12 Red";

            if (shippedDefault)
            {
                V11Theme::Accent = {229.0f / 255.0f, 42.0f / 255.0f, 48.0f / 255.0f, 1.0f};
                V11Theme::AccentHover = {248.0f / 255.0f, 66.0f / 255.0f, 72.0f / 255.0f, 1.0f};
                V11Theme::AccentDark = {74.0f / 255.0f, 10.0f / 255.0f, 15.0f / 255.0f, 1.0f};
                V11Theme::WindowBg = {3.0f / 255.0f, 5.0f / 255.0f, 8.0f / 255.0f, 1.0f};
                V11Theme::BodyBg = {5.0f / 255.0f, 8.0f / 255.0f, 12.0f / 255.0f, 1.0f};
                V11Theme::FooterBg = {4.0f / 255.0f, 7.0f / 255.0f, 10.0f / 255.0f, 1.0f};
                V11Theme::RailBg = {7.0f / 255.0f, 10.0f / 255.0f, 14.0f / 255.0f, 0.985f};
                V11Theme::PanelBg = {9.0f / 255.0f, 12.0f / 255.0f, 17.0f / 255.0f, 0.985f};
                V11Theme::PanelBorder = {78.0f / 255.0f, 83.0f / 255.0f, 91.0f / 255.0f, 0.38f};
                V11Theme::Separator = {90.0f / 255.0f, 94.0f / 255.0f, 103.0f / 255.0f, 0.26f};
                V11Theme::ControlBg = {12.0f / 255.0f, 16.0f / 255.0f, 22.0f / 255.0f, 1.0f};
                V11Theme::ControlHover = {35.0f / 255.0f, 18.0f / 255.0f, 23.0f / 255.0f, 1.0f};
                V11Theme::MutedText = {151.0f / 255.0f, 157.0f / 255.0f, 168.0f / 255.0f, 1.0f};
            }

            ApplyV11Style();

            // V12 is a complete UI system, not only a color swap. These values
            // intentionally affect every existing feature page so legacy controls
            // inherit the same card/dashboard language without changing backends.
            auto& style = ImGui::GetStyle();
            style.WindowPadding = ImVec2(12.0f, 12.0f);
            style.FramePadding = ImVec2(11.0f, 7.0f);
            style.CellPadding = ImVec2(10.0f, 7.0f);
            style.ItemSpacing = ImVec2(10.0f, 9.0f);
            style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
            style.IndentSpacing = 18.0f;
            style.ScrollbarSize = 10.0f;
            style.GrabMinSize = 12.0f;

            style.ChildRounding = 7.0f;
            style.FrameRounding = 5.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 5.0f;
            style.TabRounding = 5.0f;

            style.ChildBorderSize = 1.0f;
            style.FrameBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;

            style.Colors[ImGuiCol_Text] = ImVec4(0.94f, 0.95f, 0.97f, 1.0f);
            style.Colors[ImGuiCol_TextDisabled] = V11Theme::MutedText;
            style.Colors[ImGuiCol_ChildBg] = V11Theme::PanelBg;
            style.Colors[ImGuiCol_PopupBg] = ImVec4(8.0f / 255.0f, 11.0f / 255.0f, 16.0f / 255.0f, 0.99f);
            style.Colors[ImGuiCol_Border] = V11Theme::PanelBorder;
            style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

            style.Colors[ImGuiCol_FrameBg] = V11Theme::ControlBg;
            style.Colors[ImGuiCol_FrameBgHovered] = V11Theme::ControlHover;
            style.Colors[ImGuiCol_FrameBgActive] = ImVec4(
                V11Theme::AccentDark.x,
                V11Theme::AccentDark.y,
                V11Theme::AccentDark.z,
                0.92f);

            style.Colors[ImGuiCol_Button] = V11Theme::ControlBg;
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.22f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.36f);

            style.Colors[ImGuiCol_Header] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.18f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.28f);
            style.Colors[ImGuiCol_HeaderActive] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.40f);

            style.Colors[ImGuiCol_CheckMark] = V11Theme::Accent;
            style.Colors[ImGuiCol_SliderGrab] = V11Theme::Accent;
            style.Colors[ImGuiCol_SliderGrabActive] = V11Theme::AccentHover;
            style.Colors[ImGuiCol_Separator] = V11Theme::Separator;
            style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(
                V11Theme::Accent.x,
                V11Theme::Accent.y,
                V11Theme::Accent.z,
                0.56f);
            style.Colors[ImGuiCol_SeparatorActive] = V11Theme::Accent;

            style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(5.0f / 255.0f, 8.0f / 255.0f, 12.0f / 255.0f, 0.82f);
            style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(75.0f / 255.0f, 28.0f / 255.0f, 32.0f / 255.0f, 0.90f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = V11Theme::Accent;
            style.Colors[ImGuiCol_ScrollbarGrabActive] = V11Theme::AccentHover;

            style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(12.0f / 255.0f, 16.0f / 255.0f, 22.0f / 255.0f, 1.0f);
            style.Colors[ImGuiCol_TableBorderStrong] = V11Theme::PanelBorder;
            style.Colors[ImGuiCol_TableBorderLight] = V11Theme::Separator;
            style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
        }
    }
}
