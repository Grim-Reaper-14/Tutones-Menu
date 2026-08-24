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
        }
    }
}
