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
        static std::string message{"Ready"};auto& themes=ThemeManager::Get();static_cast<void>(themes.EnsureInitialized());
        ImGui::SetCursorPos(ImVec2(226,16));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(14,12));ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,3.f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,V11Theme::PanelBg);ImGui::PushStyleColor(ImGuiCol_Border,V11Theme::PanelBorder);
        if(ImGui::BeginChild("##menu_settings_panel",ImVec2(490,430),true,ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::TextColored(V11Theme::Accent,"Menu Settings");ImGui::SameLine();ImGui::TextDisabled("settings + themes + media");ImGui::Separator();
            if(page==0)
            {
                ImGui::TextWrapped("Persistent state is stored in menu_settings.json. Loading never executes one-shot commands such as Heal, Spawn, Clone, Join Session or Save Personal Vehicle.");
                if(ImGui::Button("Save Settings",ImVec2(-1,0)))message=SettingsPersistence::Save()?"Settings saved":"Settings save failed";
                if(ImGui::Button("Load Settings",ImVec2(-1,0)))message=SettingsPersistence::Load(false)?"Settings loaded":"Settings load failed";
                if(ImGui::Button("Reload Settings",ImVec2(-1,0)))message=SettingsPersistence::Load(true)?"Folders refreshed; settings reloaded":"Reload failed";
                ImGui::TextDisabled("File: %s",SettingsPersistence::Path().string().c_str());
                ImGui::TextDisabled("Active theme: %s",themes.CurrentThemeFile().empty()?"(default)":themes.CurrentThemeFile().c_str());
            }
            else if(page==1) SettingsThemeEditor::Render(message);
            else
            {
                ImGui::TextWrapped("Refresh folder catalogs after adding images, themes or Windows fonts while GTA is running.");
                if(ImGui::Button("Refresh Folder Catalogs",ImVec2(-1,0))){themes.Refresh();message="Folder catalogs refreshed";}
                if(ImGui::Button("Reapply Current Images + Font",ImVec2(-1,0))){themes.MarkResourcesDirty();message="Theme resources queued for reapply";}
                if(ImGui::Button("Reload Active Theme File",ImVec2(-1,0)))message=themes.ReloadTheme()?"Theme reloaded":"No active theme to reload";
                ImGui::TextDisabled("Images: PNG, JPG/JPEG, BMP");ImGui::TextDisabled("Fonts: TTF, OTF, TTC");
            }
            ImGui::Separator();ImGui::Text("Status: %s",message.c_str());
        }
        ImGui::EndChild();ImGui::PopStyleColor(2);ImGui::PopStyleVar(2);
    }
}
