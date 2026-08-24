#pragma once

#include "SettingsPersistence.hpp"
#include "SettingsThemeEditor.hpp"
#include "V11Description.hpp"
#include "V11Theme.hpp"
#include <imgui.h>
#include <string>

namespace Tutones::UI
{
    inline void RenderSettingsPanel(std::size_t page) noexcept
    {
        static std::string message{"Ready"};
        auto& themes = ThemeManager::Get();
        static_cast<void>(themes.EnsureInitialized());

        ImGui::SetCursorPos(ImVec2(226.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild(
                "##menu_settings_v2",
                ImVec2(780.0f, 500.0f),
                true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::TextColored(V11Theme::Accent, "SETTINGS");
            ImGui::SameLine();
            ImGui::TextDisabled(page == 0 ? "GENERAL" : page == 1 ? "THEME" : "CONTROLS & RESOURCES");
            ImGui::TextDisabled("Tutones V2 configuration, persistence, media and font resources.");
            ImGui::Separator();

            if (page == 0)
            {
                auto& uiSettings = Core::Config::MenuSettingsService::Get().Current().ui;

                if (ImGui::BeginTable("##settings_general_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##settings_window_card", ImVec2(0.0f, 346.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "WINDOW");
                        ImGui::TextDisabled("Dashboard placement and sizing");
                        ImGui::Separator();

                        if (ImGui::Checkbox("Resizable Window", &uiSettings.resizable))
                            message = uiSettings.resizable ? "Window resizing enabled" : "Window resizing locked";
                        DescribeLastV11Item("Enable the lower-right ImGui resize grip. The last safe V2 window size is saved with Menu Settings.");

                        if (ImGui::Checkbox("Anchor Top Left", &uiSettings.anchorTopLeft))
                            message = uiSettings.anchorTopLeft ? "Menu anchored to the top-left" : "Top-left anchor disabled";
                        DescribeLastV11Item("Keep Tutones at the top-left of the GTA viewport with a small screen-edge margin.");

                        ImGui::Spacing();
                        ImGui::SeparatorText("Current Size");
                        ImGui::Text("%.0f x %.0f", uiSettings.menuWidth, uiSettings.menuHeight);
                        ImGui::TextDisabled("V2 minimum: 1460 x 820");
                        ImGui::TextWrapped("Resize from the lower-right corner when Resizable Window is enabled. The dashboard rails keep their fixed minimum layout.");
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##settings_persistence_card", ImVec2(0.0f, 346.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "PERSISTENCE");
                        ImGui::TextDisabled("Save and restore menu state");
                        ImGui::Separator();

                        ImGui::TextWrapped("Loading restores persistent settings only. One-shot commands such as Heal, Spawn, Clone, Join Session and Save Personal Vehicle are never replayed.");
                        ImGui::Spacing();

                        if (ImGui::Button("SAVE SETTINGS", ImVec2(-1.0f, 34.0f)))
                            message = SettingsPersistence::Save() ? "Settings saved" : "Settings save failed";
                        if (ImGui::Button("LOAD SETTINGS", ImVec2(-1.0f, 34.0f)))
                            message = SettingsPersistence::Load(false) ? "Settings loaded" : "Settings load failed";
                        if (ImGui::Button("RELOAD SETTINGS", ImVec2(-1.0f, 34.0f)))
                            message = SettingsPersistence::Load(true) ? "Folders refreshed; settings reloaded" : "Reload failed";

                        ImGui::Spacing();
                        ImGui::TextDisabled("Active theme");
                        ImGui::TextWrapped("%s", themes.CurrentThemeFile().empty() ? "(V2 default)" : themes.CurrentThemeFile().c_str());
                        ImGui::TextDisabled("Settings file");
                        ImGui::TextWrapped("%s", SettingsPersistence::Path().string().c_str());
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
            else if (page == 1)
            {
                if (ImGui::BeginChild("##settings_theme_v2", ImVec2(0.0f, 390.0f), true))
                {
                    ImGui::TextColored(V11Theme::Accent, "THEME STUDIO");
                    ImGui::TextDisabled("Colors, dashboard images, backgrounds and fonts");
                    ImGui::Separator();
                    SettingsThemeEditor::Render(message);
                }
                ImGui::EndChild();
            }
            else
            {
                auto& storage = themes.Storage();

                ImGui::TextWrapped("Resource catalogs read directly from the Tutones images and fonts folders. Add files, refresh the catalog, then select them from Theme Studio.");
                ImGui::Spacing();

                if (ImGui::Button("REFRESH CATALOGS", ImVec2(180.0f, 34.0f)))
                {
                    themes.Refresh();
                    message = "Image and font folder catalogs refreshed";
                }
                ImGui::SameLine();
                if (ImGui::Button("REAPPLY RESOURCES", ImVec2(190.0f, 34.0f)))
                {
                    themes.MarkResourcesDirty();
                    message = "Theme resources queued for reapply";
                }
                ImGui::SameLine();
                if (ImGui::Button("RELOAD THEME FILE", ImVec2(-1.0f, 34.0f)))
                    message = themes.ReloadTheme() ? "Theme reloaded" : "No active theme to reload";

                ImGui::Spacing();
                if (ImGui::BeginTable("##settings_resources_columns", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##settings_images_card", ImVec2(0.0f, 292.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "IMAGE LOADER");
                        ImGui::Text("Images found: %zu", storage.ImageFiles().size());
                        ImGui::TextDisabled("PNG / JPG / JPEG / BMP");
                        ImGui::Separator();
                        ImGui::TextDisabled("%s", storage.ImagesDirectory().string().c_str());
                        ImGui::Spacing();

                        if (storage.ImageFiles().empty())
                            ImGui::TextDisabled("No images in folder.");
                        else
                            for (const auto& file : storage.ImageFiles())
                                ImGui::BulletText("%s", file.c_str());
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    if (ImGui::BeginChild("##settings_fonts_card", ImVec2(0.0f, 292.0f), true))
                    {
                        ImGui::TextColored(V11Theme::Accent, "FONT LOADER");
                        ImGui::Text("Fonts found: %zu", storage.FontFiles().size());
                        ImGui::TextDisabled("TTF / OTF / TTC");
                        ImGui::Separator();
                        ImGui::TextDisabled("%s", storage.FontsDirectory().string().c_str());
                        ImGui::Spacing();

                        if (storage.FontFiles().empty())
                            ImGui::TextDisabled("No fonts in folder.");
                        else
                            for (const auto& file : storage.FontFiles())
                                ImGui::BulletText("%s", file.c_str());
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("STATUS");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
