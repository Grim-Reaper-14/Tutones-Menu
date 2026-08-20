#pragma once

#include "ThemeManager.hpp"
#include "V11Description.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Tutones::UI::SettingsThemeEditor
{
    inline char ThemeName[64] = "custom";
    inline char FontFilter[72]{};
    inline std::string SelectedTheme;

    inline bool Contains(std::string_view text, std::string_view filter)
    {
        if (filter.empty())
            return true;

        std::string a(text);
        std::string b(filter);
        const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::transform(a.begin(), a.end(), a.begin(), lower);
        std::transform(b.begin(), b.end(), b.begin(), lower);
        return a.find(b) != std::string::npos;
    }

    inline bool ImageCombo(const char* label, std::string& value, const char* help)
    {
        auto& manager = ThemeManager::Get();
        bool changed = false;
        const char* preview = value.empty() ? "Embedded / None" : value.c_str();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("Embedded / None", value.empty()))
            {
                value.clear();
                changed = true;
            }

            for (const auto& file : manager.Storage().ImageFiles())
            {
                if (ImGui::Selectable(file.c_str(), value == file))
                {
                    value = file;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        DescribeLastV11Item(help);
        if (changed)
            manager.MarkImagesDirty();
        return changed;
    }

    inline void FontEditor(ThemeDefinition& theme)
    {
        auto& manager = ThemeManager::Get();
        const char* preview = theme.fontFile.empty() ? "Embedded Tutones Font" : theme.fontFile.c_str();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("Folder Font", preview))
        {
            ImGui::InputTextWithHint(
                "##font_filter",
                "Filter fonts folder...",
                FontFilter,
                sizeof(FontFilter));

            if (ImGui::Selectable("Embedded Tutones Font", theme.fontFile.empty()))
            {
                theme.fontFile.clear();
                manager.MarkFontDirty();
            }

            ImGui::Separator();
            for (const auto& file : manager.Storage().FontFiles())
            {
                if (!Contains(file, FontFilter))
                    continue;
                if (ImGui::Selectable(file.c_str(), theme.fontFile == file))
                {
                    theme.fontFile = file;
                    manager.MarkFontDirty();
                }
            }
            ImGui::EndCombo();
        }
        DescribeLastV11Item("Loads TTF, OTF or TTC files directly from the Tutones Menu\\fonts folder.");

        if (ImGui::SliderFloat("Font Size", &theme.fontSize, 9.0f, 32.0f, "%.1f px"))
        {
            theme.fontSize = std::clamp(theme.fontSize, 9.0f, 32.0f);
            manager.MarkFontDirty();
        }
    }

    inline void RenderFileList(
        const char* title,
        const std::filesystem::path& directory,
        const std::vector<std::string>& files,
        const char* emptyText)
    {
        ImGui::TextColored(V11Theme::Accent, "%s", title);
        ImGui::TextDisabled("Folder: %s", directory.string().c_str());
        ImGui::TextDisabled("Files found: %zu", files.size());

        if (ImGui::BeginChild(title, ImVec2(-1.0f, 118.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            if (files.empty())
            {
                ImGui::TextDisabled("%s", emptyText);
            }
            else
            {
                for (const auto& file : files)
                    ImGui::BulletText("%s", file.c_str());
            }
        }
        ImGui::EndChild();
    }

    inline void Color(const char* name, ImVec4& color)
    {
        if (ImGui::ColorEdit4(name, &color.x, ImGuiColorEditFlags_AlphaBar))
            ThemeManager::Get().ApplyColors();
    }

    inline void Render(std::string& message)
    {
        auto& manager = ThemeManager::Get();
        auto& theme = manager.Current();
        auto& storage = manager.Storage();

        ImGui::TextColored(V11Theme::Accent, "Named Theme Presets");
        const char* preview = SelectedTheme.empty()
            ? (manager.CurrentThemeFile().empty() ? "Select theme..." : manager.CurrentThemeFile().c_str())
            : SelectedTheme.c_str();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("Theme File", preview))
        {
            for (const auto& file : storage.ThemeFiles())
            {
                if (ImGui::Selectable(file.c_str(), SelectedTheme == file))
                    SelectedTheme = file;
            }
            ImGui::EndCombo();
        }

        ImGui::InputTextWithHint(
            "##theme_name",
            "Theme name for Save Theme",
            ThemeName,
            sizeof(ThemeName));

        if (ImGui::Button("Save Theme", ImVec2(150.0f, 0.0f)))
        {
            if (manager.SaveTheme(ThemeName))
            {
                SelectedTheme = manager.CurrentThemeFile();
                message = "Theme saved";
            }
            else
            {
                message = "Theme save failed";
            }
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(SelectedTheme.empty());
        if (ImGui::Button("Load", ImVec2(90.0f, 0.0f)))
            message = manager.LoadTheme(SelectedTheme) ? "Theme loaded" : "Theme load failed";
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Reload Active", ImVec2(-1.0f, 0.0f)))
            message = manager.ReloadTheme() ? "Theme reloaded" : "Theme reload failed";

        if (ImGui::Button("Refresh Theme / Image / Font Folders", ImVec2(-1.0f, 0.0f)))
        {
            manager.Refresh();
            message = "Folders refreshed";
        }

        ImGui::Separator();
        ImGui::TextColored(V11Theme::Accent, "Images");
        ImGui::TextDisabled("Image loader reads directly from: %s", storage.ImagesDirectory().string().c_str());
        ImageCombo("Header Banner", theme.headerImage, "Image from Tutones Menu\\images for the header banner.");
        ImageCombo("Bottom Banner", theme.footerImage, "Image from Tutones Menu\\images for the description banner.");
        ImageCombo("Background", theme.backgroundImage, "Image from Tutones Menu\\images behind the menu body.");
        ImGui::SliderFloat("Background Opacity", &theme.backgroundOpacity, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::TextColored(V11Theme::Accent, "Font");
        ImGui::TextDisabled("Font loader reads directly from: %s", storage.FontsDirectory().string().c_str());
        FontEditor(theme);

        ImGui::Separator();
        RenderFileList(
            "Images Folder Contents",
            storage.ImagesDirectory(),
            storage.ImageFiles(),
            "No images found. Add PNG, JPG/JPEG or BMP files to the images folder, then refresh.");

        ImGui::Spacing();
        RenderFileList(
            "Fonts Folder Contents",
            storage.FontsDirectory(),
            storage.FontFiles(),
            "No fonts found. Add TTF, OTF or TTC files to the fonts folder, then refresh.");

        ImGui::Separator();
        ImGui::TextColored(V11Theme::Accent, "ImGui Theme Editor");
        Color("Accent", theme.accent);
        Color("Accent Hover", theme.accentHover);
        Color("Accent Dark", theme.accentDark);
        Color("Window Background", theme.windowBg);
        Color("Body Background", theme.bodyBg);
        Color("Footer Background", theme.footerBg);
        Color("Rail Background", theme.railBg);
        Color("Panel Background", theme.panelBg);
        Color("Panel Border", theme.panelBorder);
        Color("Separator", theme.separator);
        Color("Control Background", theme.controlBg);
        Color("Control Hover", theme.controlHover);
        Color("Muted Text", theme.mutedText);

        if (ImGui::Button("Reset Live Theme to V11 Defaults", ImVec2(-1.0f, 0.0f)))
        {
            manager.ResetDefault();
            message = "Live theme reset; save it to persist";
        }

        ImGui::TextDisabled("Themes: %s", storage.ThemesDirectory().string().c_str());
        ImGui::TextDisabled("Images: %s", storage.ImagesDirectory().string().c_str());
        ImGui::TextDisabled("Fonts: %s", storage.FontsDirectory().string().c_str());
    }
}
