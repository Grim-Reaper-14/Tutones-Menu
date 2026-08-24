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

        ImGui::SetCursorPos(ImVec2(226, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, V11Theme::PanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border, V11Theme::PanelBorder);

        if (ImGui::BeginChild(
                "##menu_settings_panel",
                ImVec2(490, 430),
                true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::TextColored(V11Theme::Accent, "Menu Settings");
            ImGui::SameLine();
            ImGui::TextDisabled("settings + themes + media");
            ImGui::Separator();

            if (page == 0)
            {
                auto& uiSettings = Core::Config::MenuSettingsService::Get().Current().ui;

                ImGui::SeparatorText("Window");
                if (ImGui::Checkbox("Resizable Window", &uiSettings.resizable))
                    message = uiSettings.resizable ? "Window resizing enabled" : "Window resizing locked";
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Enable the lower-right ImGui resize grip. The last safe window size is saved with Menu Settings.");

                if (ImGui::Checkbox("Anchor Top Left", &uiSettings.anchorTopLeft))
                    message = uiSettings.anchorTopLeft ? "Menu anchored to the top-left" : "Top-left anchor disabled";
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Keep Tutones at the top-left of the GTA viewport with a small screen-edge margin.");

                ImGui::TextDisabled("Window size: %.0f x %.0f", uiSettings.menuWidth, uiSettings.menuHeight);
                ImGui::TextDisabled("Resize from the lower-right corner when Resizable Window is enabled.");

                ImGui::SeparatorText("Persistence");
                ImGui::TextWrapped(
                    "Persistent state is stored in menu_settings.json. Loading never executes one-shot commands such as Heal, Spawn, Clone, Join Session or Save Personal Vehicle.");

                if (ImGui::Button("Save Settings", ImVec2(-1, 0)))
                    message = SettingsPersistence::Save() ? "Settings saved" : "Settings save failed";
                if (ImGui::Button("Load Settings", ImVec2(-1, 0)))
                    message = SettingsPersistence::Load(false) ? "Settings loaded" : "Settings load failed";
                if (ImGui::Button("Reload Settings", ImVec2(-1, 0)))
                    message = SettingsPersistence::Load(true) ? "Folders refreshed; settings reloaded" : "Reload failed";

                ImGui::TextDisabled("File: %s", SettingsPersistence::Path().string().c_str());
                ImGui::TextDisabled(
                    "Active theme: %s",
                    themes.CurrentThemeFile().empty() ? "(default)" : themes.CurrentThemeFile().c_str());
            }
            else if (page == 1)
            {
                SettingsThemeEditor::Render(message);
            }
            else
            {
                auto& storage = themes.Storage();
                ImGui::TextWrapped(
                    "Image Loader and Font Loader read directly from the Tutones images and fonts folders. Add files, refresh, then select them from the Theme page.");

                if (ImGui::Button("Refresh Folder Catalogs", ImVec2(-1, 0)))
                {
                    themes.Refresh();
                    message = "Image and font folder catalogs refreshed";
                }
                if (ImGui::Button("Reapply Current Images + Font", ImVec2(-1, 0)))
                {
                    themes.MarkResourcesDirty();
                    message = "Theme resources queued for reapply";
                }
                if (ImGui::Button("Reload Active Theme File", ImVec2(-1, 0)))
                    message = themes.ReloadTheme() ? "Theme reloaded" : "No active theme to reload";

                ImGui::SeparatorText("Image Loader");
                ImGui::TextDisabled("Folder: %s", storage.ImagesDirectory().string().c_str());
                ImGui::Text("Images found: %zu", storage.ImageFiles().size());
                ImGui::TextDisabled("Supported: PNG, JPG/JPEG, BMP");

                if (ImGui::BeginChild("##settings_image_files", ImVec2(-1, 110), true))
                {
                    if (storage.ImageFiles().empty())
                        ImGui::TextDisabled("No images in folder.");
                    else
                        for (const auto& file : storage.ImageFiles())
                            ImGui::BulletText("%s", file.c_str());
                }
                ImGui::EndChild();

                ImGui::SeparatorText("Font Loader");
                ImGui::TextDisabled("Folder: %s", storage.FontsDirectory().string().c_str());
                ImGui::Text("Fonts found: %zu", storage.FontFiles().size());
                ImGui::TextDisabled("Supported: TTF, OTF, TTC");

                if (ImGui::BeginChild("##settings_font_files", ImVec2(-1, 110), true))
                {
                    if (storage.FontFiles().empty())
                        ImGui::TextDisabled("No fonts in folder.");
                    else
                        for (const auto& file : storage.FontFiles())
                            ImGui::BulletText("%s", file.c_str());
                }
                ImGui::EndChild();
            }

            ImGui::Separator();
            ImGui::Text("Status: %s", message.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
