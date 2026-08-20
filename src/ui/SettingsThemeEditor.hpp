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
    inline char ThemeName[64]="custom";
    inline char FontFilter[72]{};
    inline std::string SelectedTheme;

    inline bool Contains(std::string_view text,std::string_view filter)
    {
        if(filter.empty())return true;std::string a(text),b(filter);
        auto f=[](unsigned char c){return static_cast<char>(std::tolower(c));};
        std::transform(a.begin(),a.end(),a.begin(),f);std::transform(b.begin(),b.end(),b.begin(),f);return a.find(b)!=std::string::npos;
    }
    inline bool ImageCombo(const char* label,std::string& value,const char* help)
    {
        auto& m=ThemeManager::Get();bool changed=false;const char* preview=value.empty()?"Embedded / None":value.c_str();
        ImGui::SetNextItemWidth(-1.f);if(ImGui::BeginCombo(label,preview))
        {
            if(ImGui::Selectable("Embedded / None",value.empty())){value.clear();changed=true;}
            for(const auto& f:m.Storage().ImageFiles())if(ImGui::Selectable(f.c_str(),value==f)){value=f;changed=true;}
            ImGui::EndCombo();
        }
        DescribeLastV11Item(help);if(changed)m.MarkImagesDirty();return changed;
    }
    inline void FontEditor(ThemeDefinition& t)
    {
        auto& m=ThemeManager::Get();const char* preview=t.fontFile.empty()?"Embedded Tutones Font":t.fontFile.c_str();
        ImGui::SetNextItemWidth(-1.f);if(ImGui::BeginCombo("Windows Font",preview))
        {
            ImGui::InputTextWithHint("##font_filter","Filter Windows fonts...",FontFilter,sizeof(FontFilter));
            if(ImGui::Selectable("Embedded Tutones Font",t.fontFile.empty())){t.fontFile.clear();m.MarkFontDirty();}
            ImGui::Separator();for(const auto& f:m.Storage().FontFiles())if(Contains(f,FontFilter)&&ImGui::Selectable(f.c_str(),t.fontFile==f)){t.fontFile=f;m.MarkFontDirty();}
            ImGui::EndCombo();
        }
        if(ImGui::SliderFloat("Font Size",&t.fontSize,9.f,32.f,"%.1f px")){t.fontSize=std::clamp(t.fontSize,9.f,32.f);m.MarkFontDirty();}
    }
    inline void Color(const char* name,ImVec4& c){if(ImGui::ColorEdit4(name,&c.x,ImGuiColorEditFlags_AlphaBar))ThemeManager::Get().ApplyColors();}

    inline void Render(std::string& message)
    {
        auto& m=ThemeManager::Get();auto& t=m.Current();auto& storage=m.Storage();
        ImGui::TextColored(V11Theme::Accent,"Named Theme Presets");
        const char* preview=SelectedTheme.empty()?(m.CurrentThemeFile().empty()?"Select theme...":m.CurrentThemeFile().c_str()):SelectedTheme.c_str();
        ImGui::SetNextItemWidth(-1.f);if(ImGui::BeginCombo("Theme File",preview)){for(const auto& f:storage.ThemeFiles())if(ImGui::Selectable(f.c_str(),SelectedTheme==f))SelectedTheme=f;ImGui::EndCombo();}
        ImGui::InputTextWithHint("##theme_name","Theme name for Save Theme",ThemeName,sizeof(ThemeName));
        if(ImGui::Button("Save Theme",ImVec2(150,0))){if(m.SaveTheme(ThemeName)){SelectedTheme=m.CurrentThemeFile();message="Theme saved";}else message="Theme save failed";}
        ImGui::SameLine();ImGui::BeginDisabled(SelectedTheme.empty());if(ImGui::Button("Load",ImVec2(90,0)))message=m.LoadTheme(SelectedTheme)?"Theme loaded":"Theme load failed";ImGui::EndDisabled();
        ImGui::SameLine();if(ImGui::Button("Reload Active",ImVec2(-1,0)))message=m.ReloadTheme()?"Theme reloaded":"Theme reload failed";
        if(ImGui::Button("Refresh Theme / Image / Font Folders",ImVec2(-1,0))){m.Refresh();message="Folders refreshed";}

        ImGui::Separator();ImGui::TextColored(V11Theme::Accent,"Images");
        ImageCombo("Header Banner",t.headerImage,"Image from Tutones Menu\\images for the header banner.");
        ImageCombo("Bottom Banner",t.footerImage,"Image from Tutones Menu\\images for the description banner.");
        ImageCombo("Background",t.backgroundImage,"Image from Tutones Menu\\images behind the menu body.");
        ImGui::SliderFloat("Background Opacity",&t.backgroundOpacity,0.f,1.f,"%.2f");
        ImGui::Separator();ImGui::TextColored(V11Theme::Accent,"Font");FontEditor(t);

        ImGui::Separator();ImGui::TextColored(V11Theme::Accent,"ImGui Theme Editor");
        Color("Accent",t.accent);Color("Accent Hover",t.accentHover);Color("Accent Dark",t.accentDark);
        Color("Window Background",t.windowBg);Color("Body Background",t.bodyBg);Color("Footer Background",t.footerBg);
        Color("Rail Background",t.railBg);Color("Panel Background",t.panelBg);Color("Panel Border",t.panelBorder);
        Color("Separator",t.separator);Color("Control Background",t.controlBg);Color("Control Hover",t.controlHover);Color("Muted Text",t.mutedText);
        if(ImGui::Button("Reset Live Theme to V11 Defaults",ImVec2(-1,0))){m.ResetDefault();message="Live theme reset; save it to persist";}
        ImGui::TextDisabled("Themes: %s",storage.ThemesDirectory().string().c_str());
        ImGui::TextDisabled("Images: %s",storage.ImagesDirectory().string().c_str());
        ImGui::TextDisabled("Windows fonts: %s",storage.FontsDirectory().string().c_str());
    }
}
