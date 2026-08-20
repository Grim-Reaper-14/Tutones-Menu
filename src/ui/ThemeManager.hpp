#pragma once

#include "ThemeStorage.hpp"
#include "ThemeTexture.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/logging/Logger.hpp"

#include <imgui.h>
#include <string>
#include <unordered_map>

namespace Tutones::UI
{
    class ThemeManager final
    {
    public:
        static ThemeManager& Get() noexcept { static ThemeManager v; return v; }

        bool EnsureInitialized() noexcept
        {
            if (m_Initialized) return true;
            if (!m_Storage.Initialize()) return false;
            m_Current=DefaultTheme(); ApplyColors(); m_Initialized=true;
            const auto requested=Core::Config::MenuSettingsService::Get().Current().ui.activeTheme;
            if (!requested.empty() && LoadTheme(requested)) return true;
            std::string saved;
            if (m_Storage.Save("default",m_Current,saved)) { m_CurrentFile=saved; Core::Config::MenuSettingsService::Get().Current().ui.activeTheme=saved; }
            MarkResourcesDirty(); return true;
        }

        void SetFallbackFont(ImFont* font) noexcept
        {
            auto* ctx=ImGui::GetCurrentContext();
            if (ctx!=m_Context) { ResetTextures(); m_FontCache.clear(); m_ActiveFont=nullptr; m_Context=ctx; m_ResourcesDirty=true; m_FontDirty=true; }
            m_FallbackFont=font;
            if (ctx && !m_ActiveFont) ImGui::GetIO().FontDefault=font;
        }

        void ApplyPendingResources() noexcept
        {
            if (!EnsureInitialized() || !ImGui::GetCurrentContext()) return;
            if (!m_Embedded.Valid()) static_cast<void>(m_Embedded.LoadEmbeddedBanner());
            if (m_ResourcesDirty)
            {
                LoadSlot(m_Header,m_Current.headerImage); LoadSlot(m_Footer,m_Current.footerImage); LoadSlot(m_Background,m_Current.backgroundImage);
                m_ResourcesDirty=false;
            }
            if (m_FontDirty && m_FallbackFont)
            {
                if (m_Current.fontFile.empty()) { m_ActiveFont=nullptr; ImGui::GetIO().FontDefault=m_FallbackFont; }
                else
                {
                    const auto path=m_Storage.FontPath(m_Current.fontFile);
                    const std::string key=path.string()+"#"+std::to_string(m_Current.fontSize);
                    auto it=m_FontCache.find(key);
                    if (it==m_FontCache.end())
                    {
                        ImFontConfig cfg{}; cfg.OversampleH=2; cfg.OversampleV=1;
                        ImFont* f=ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(),m_Current.fontSize,&cfg);
                        it=m_FontCache.emplace(key,f).first;
                    }
                    m_ActiveFont=it->second; ImGui::GetIO().FontDefault=m_ActiveFont?m_ActiveFont:m_FallbackFont;
                }
                m_FontDirty=false;
            }
        }

        void ApplyColors() noexcept
        {
            V11Theme::Accent=m_Current.accent; V11Theme::AccentHover=m_Current.accentHover; V11Theme::AccentDark=m_Current.accentDark;
            V11Theme::WindowBg=m_Current.windowBg; V11Theme::BodyBg=m_Current.bodyBg; V11Theme::FooterBg=m_Current.footerBg;
            V11Theme::RailBg=m_Current.railBg; V11Theme::PanelBg=m_Current.panelBg; V11Theme::PanelBorder=m_Current.panelBorder;
            V11Theme::Separator=m_Current.separator; V11Theme::ControlBg=m_Current.controlBg; V11Theme::ControlHover=m_Current.controlHover; V11Theme::MutedText=m_Current.mutedText;
        }

        bool LoadTheme(std::string_view file) noexcept
        {
            if (!m_Initialized && !EnsureInitialized()) return false;
            ThemeDefinition next; if(!m_Storage.Load(file,next))return false;
            m_Current=std::move(next); m_CurrentFile=std::filesystem::path(std::string(file)).filename().string();
            Core::Config::MenuSettingsService::Get().Current().ui.activeTheme=m_CurrentFile;
            ApplyColors(); MarkResourcesDirty(); return true;
        }

        bool SaveTheme(std::string_view name) noexcept
        {
            if (!EnsureInitialized()) return false;
            m_Current.name=std::string(name); std::string file;
            if(!m_Storage.Save(name,m_Current,file))return false;
            m_CurrentFile=file; Core::Config::MenuSettingsService::Get().Current().ui.activeTheme=file; return true;
        }
        bool ReloadTheme() noexcept { m_Storage.Refresh(); return !m_CurrentFile.empty() && LoadTheme(m_CurrentFile); }
        void Refresh() noexcept { if(EnsureInitialized())m_Storage.Refresh(); }
        void ResetDefault() noexcept { m_Current=DefaultTheme(); ApplyColors(); MarkResourcesDirty(); }
        void MarkResourcesDirty() noexcept { m_ResourcesDirty=true; m_FontDirty=true; }
        void MarkImagesDirty() noexcept { m_ResourcesDirty=true; }
        void MarkFontDirty() noexcept { m_FontDirty=true; }

        [[nodiscard]] ThemeDefinition& Current() noexcept { static_cast<void>(EnsureInitialized()); return m_Current; }
        [[nodiscard]] const std::string& CurrentThemeFile() const noexcept { return m_CurrentFile; }
        [[nodiscard]] ImFont* ActiveFont() const noexcept { return m_ActiveFont; }
        [[nodiscard]] ImTextureRef HeaderTexture() const noexcept { return m_Header.Valid()?m_Header.Ref():m_Embedded.Ref(); }
        [[nodiscard]] ImTextureRef FooterTexture() const noexcept { return m_Footer.Valid()?m_Footer.Ref():m_Embedded.Ref(); }
        [[nodiscard]] ImTextureRef BackgroundTexture() const noexcept { return m_Background.Ref(); }
        [[nodiscard]] bool HeaderIsCustom() const noexcept { return m_Header.Valid(); }
        [[nodiscard]] bool FooterIsCustom() const noexcept { return m_Footer.Valid(); }
        [[nodiscard]] ThemeStorage& Storage() noexcept { return m_Storage; }

    private:
        ThemeManager()=default;
        void LoadSlot(ThemeTexture& slot,const std::string& name) noexcept
        {
            if(name.empty()){slot.Reset();return;} if(!slot.LoadFile(m_Storage.ImagePath(name))){slot.Reset();TUTONES_LOG_WARN("ui.theme","Theme image failed to load; using fallback");}
        }
        void ResetTextures() noexcept { m_Header.Reset();m_Footer.Reset();m_Background.Reset();m_Embedded.Reset(); }

        bool m_Initialized{}; bool m_ResourcesDirty{true}; bool m_FontDirty{true};
        ImGuiContext* m_Context{}; ImFont* m_FallbackFont{}; ImFont* m_ActiveFont{};
        ThemeDefinition m_Current{DefaultTheme()}; std::string m_CurrentFile;
        ThemeStorage m_Storage; ThemeTexture m_Embedded,m_Header,m_Footer,m_Background;
        std::unordered_map<std::string,ImFont*> m_FontCache;
    };
}
