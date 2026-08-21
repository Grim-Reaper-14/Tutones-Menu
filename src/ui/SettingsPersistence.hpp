#pragma once

#include "PersistentMenuState.hpp"
#include "ThemeManager.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/filesystem/FileSystem.hpp"

namespace Tutones::UI::SettingsPersistence
{
    inline std::filesystem::path Path()
    {
        // Keep the same canonical file used by CoreServices startup and Application shutdown.
        return Core::FileSystem::Service::Get().UserRoot() / "menu_settings.json";
    }

    inline void Capture() noexcept
    {
        PersistentMenuStateDetail::CaptureLiveSettings();
        auto& settings = Core::Config::MenuSettingsService::Get().Current();
        if (!ThemeManager::Get().CurrentThemeFile().empty())
            settings.ui.activeTheme = ThemeManager::Get().CurrentThemeFile();
    }

    inline void Apply() noexcept
    {
        // Manual Load Settings uses the same safe stateful restore path as startup.
        // One-shot actions are intentionally absent from MenuSettingsData.
        PersistentMenuStateDetail::StageLoadedSettings();
        const auto& settings = Core::Config::MenuSettingsService::Get().Current();
        if (!settings.ui.activeTheme.empty())
            static_cast<void>(ThemeManager::Get().LoadTheme(settings.ui.activeTheme));
    }

    inline bool Save() noexcept
    {
        Capture();
        return Core::Config::MenuSettingsService::Get().Save(Path());
    }

    inline bool Load(bool refresh) noexcept
    {
        if (refresh)
            ThemeManager::Get().Refresh();
        if (!Core::Config::MenuSettingsService::Get().Load(Path()))
            return false;
        Apply();
        return true;
    }
}
