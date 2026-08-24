#pragma once

#include "ThemeStorage.hpp"
#include "ThemeTexture.hpp"
#include "../core/config/MenuSettings.hpp"
#include "../core/logging/Logger.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Tutones::UI
{
    class ThemeManager final
    {
    public:
        static ThemeManager& Get() noexcept
        {
            static ThemeManager instance;
            return instance;
        }

        bool EnsureInitialized() noexcept
        {
            if (m_Initialized)
                return true;
            if (!m_Storage.Initialize())
                return false;

            m_Current = DefaultTheme();
            ApplyColors();
            m_Initialized = true;

            const auto requested = Core::Config::MenuSettingsService::Get().Current().ui.activeTheme;
            if (!requested.empty() && LoadTheme(requested))
                return true;

            std::string saved;
            if (m_Storage.Save("default", m_Current, saved))
            {
                m_CurrentFile = saved;
                Core::Config::MenuSettingsService::Get().Current().ui.activeTheme = saved;
            }

            MarkResourcesDirty();
            return true;
        }

        void SetFallbackFont(ImFont* font) noexcept
        {
            auto* context = ImGui::GetCurrentContext();
            if (context != m_Context)
            {
                ResetTextures();
                m_FontCache.clear();
                m_ActiveFont = nullptr;
                m_Context = context;
                m_ResourcesDirty = true;
                m_FontDirty = true;
            }

            m_FallbackFont = font;
            if (context && !m_ActiveFont)
                ImGui::GetIO().FontDefault = font;
        }

        void ApplyPendingResources() noexcept
        {
            if (!EnsureInitialized() || !ImGui::GetCurrentContext())
                return;

            if (!m_Embedded.Valid() && !m_EmbeddedLoadAttempted)
            {
                m_EmbeddedLoadAttempted = true;
                if (!m_Embedded.LoadEmbeddedBanner())
                    TUTONES_LOG_WARN("ui.theme", "Embedded theme banner fallback failed to load");
            }

            if (m_ResourcesDirty)
            {
                LoadSlot(m_Header, m_Current.headerImage, "header");
                LoadSlot(m_Footer, m_Current.footerImage, "footer");
                LoadSlot(m_Background, m_Current.backgroundImage, "background");
                m_ResourcesDirty = false;
            }

            if (m_FontDirty && m_FallbackFont)
                ApplyFont();
        }

        void ApplyColors() noexcept
        {
            V11Theme::Accent = m_Current.accent;
            V11Theme::AccentHover = m_Current.accentHover;
            V11Theme::AccentDark = m_Current.accentDark;
            V11Theme::WindowBg = m_Current.windowBg;
            V11Theme::BodyBg = m_Current.bodyBg;
            V11Theme::FooterBg = m_Current.footerBg;
            V11Theme::RailBg = m_Current.railBg;
            V11Theme::PanelBg = m_Current.panelBg;
            V11Theme::PanelBorder = m_Current.panelBorder;
            V11Theme::Separator = m_Current.separator;
            V11Theme::ControlBg = m_Current.controlBg;
            V11Theme::ControlHover = m_Current.controlHover;
            V11Theme::MutedText = m_Current.mutedText;
        }

        bool LoadTheme(std::string_view file) noexcept
        {
            if (!m_Initialized && !EnsureInitialized())
                return false;

            ThemeDefinition next;
            if (!m_Storage.Load(file, next))
                return false;

            m_Current = std::move(next);
            m_CurrentFile = std::filesystem::path(std::string(file)).filename().string();
            Core::Config::MenuSettingsService::Get().Current().ui.activeTheme = m_CurrentFile;
            ApplyColors();
            MarkResourcesDirty();
            return true;
        }

        bool SaveTheme(std::string_view name) noexcept
        {
            if (!EnsureInitialized())
                return false;

            m_Current.name = std::string(name);
            std::string file;
            if (!m_Storage.Save(name, m_Current, file))
                return false;

            m_CurrentFile = file;
            Core::Config::MenuSettingsService::Get().Current().ui.activeTheme = file;
            return true;
        }

        bool ReloadTheme() noexcept
        {
            m_Storage.Refresh();
            return !m_CurrentFile.empty() && LoadTheme(m_CurrentFile);
        }

        void Refresh() noexcept
        {
            if (EnsureInitialized())
                m_Storage.Refresh();
        }

        void ResetDefault() noexcept
        {
            m_Current = DefaultTheme();
            ApplyColors();
            MarkResourcesDirty();
        }

        void MarkResourcesDirty() noexcept
        {
            m_ResourcesDirty = true;
            m_FontDirty = true;
        }

        void MarkImagesDirty() noexcept
        {
            m_ResourcesDirty = true;
        }

        void MarkFontDirty() noexcept
        {
            m_FontDirty = true;
        }

        void ReleaseTextureResources() noexcept
        {
            ResetTextures();
            m_ResourcesDirty = true;
        }

        void ReleaseImGuiResources() noexcept
        {
            ReleaseTextureResources();
            m_FontCache.clear();
            m_FallbackFont = nullptr;
            m_ActiveFont = nullptr;
            m_Context = nullptr;
            m_FontDirty = true;
        }

        [[nodiscard]] ThemeDefinition& Current() noexcept
        {
            static_cast<void>(EnsureInitialized());
            return m_Current;
        }

        [[nodiscard]] const std::string& CurrentThemeFile() const noexcept { return m_CurrentFile; }
        [[nodiscard]] ImFont* ActiveFont() const noexcept { return m_ActiveFont; }
        [[nodiscard]] ImTextureRef HeaderTexture() const noexcept { return m_Header.Valid() ? m_Header.Ref() : m_Embedded.Ref(); }
        [[nodiscard]] ImTextureRef FooterTexture() const noexcept { return m_Footer.Valid() ? m_Footer.Ref() : m_Embedded.Ref(); }
        [[nodiscard]] ImTextureRef BackgroundTexture() const noexcept { return m_Background.Ref(); }
        [[nodiscard]] bool HeaderIsCustom() const noexcept { return m_Header.Valid(); }
        [[nodiscard]] bool FooterIsCustom() const noexcept { return m_Footer.Valid(); }

        [[nodiscard]] float HeaderAspect(const ImVec2& uvMin, const ImVec2& uvMax) const noexcept
        {
            return TextureRegionAspect(m_Header.Valid() ? m_Header : m_Embedded, uvMin, uvMax);
        }

        [[nodiscard]] float FooterAspect(const ImVec2& uvMin, const ImVec2& uvMax) const noexcept
        {
            return TextureRegionAspect(m_Footer.Valid() ? m_Footer : m_Embedded, uvMin, uvMax);
        }

        [[nodiscard]] float BackgroundAspect() const noexcept
        {
            return TextureRegionAspect(m_Background, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        }

        [[nodiscard]] ThemeStorage& Storage() noexcept { return m_Storage; }

    private:
        ThemeManager() = default;

        [[nodiscard]] static float TextureRegionAspect(
            const ThemeTexture& texture,
            const ImVec2& uvMin,
            const ImVec2& uvMax) noexcept
        {
            if (!texture.Valid() || texture.Width() == 0 || texture.Height() == 0)
                return 1.0f;

            const float uSpan = std::max(0.0001f, std::abs(uvMax.x - uvMin.x));
            const float vSpan = std::max(0.0001f, std::abs(uvMax.y - uvMin.y));
            const float regionWidth = static_cast<float>(texture.Width()) * uSpan;
            const float regionHeight = static_cast<float>(texture.Height()) * vSpan;
            return regionWidth / regionHeight;
        }

        void ApplyFont() noexcept
        {
            auto& io = ImGui::GetIO();

            if (m_Current.fontFile.empty())
            {
                m_ActiveFont = nullptr;
                io.FontDefault = m_FallbackFont;
                ImGui::GetStyle().FontSizeBase = m_FallbackFont ? m_FallbackFont->LegacySize : 15.0f;
                m_FontDirty = false;
                TUTONES_LOG_DEBUG("ui.fonts", "Restored embedded Tutones font");
                return;
            }

            const auto path = m_Storage.FontPath(m_Current.fontFile);
            std::error_code error;
            if (path.empty() || !std::filesystem::is_regular_file(path, error) || error)
            {
                m_ActiveFont = nullptr;
                io.FontDefault = m_FallbackFont;
                m_FontDirty = false;
                std::string message("Selected Windows font is unavailable: ");
                message += path.empty() ? m_Current.fontFile : path.string();
                TUTONES_LOG_WARN("ui.fonts", message);
                return;
            }

            const float size = std::clamp(m_Current.fontSize, 9.0f, 40.0f);
            const std::string key = path.string() + "#" + std::to_string(size);
            ImFont* font{};
            if (const auto it = m_FontCache.find(key); it != m_FontCache.end())
            {
                font = it->second;
            }
            else
            {
                ImFontConfig config{};
                config.OversampleH = 2;
                config.OversampleV = 1;
                font = io.Fonts->AddFontFromFileTTF(path.string().c_str(), size, &config);
                if (font)
                    m_FontCache.emplace(key, font);
            }

            if (!font)
            {
                m_ActiveFont = nullptr;
                io.FontDefault = m_FallbackFont;
                m_FontDirty = false;
                std::string message("Failed to load selected Windows font; using embedded fallback: ");
                message += path.string();
                TUTONES_LOG_WARN("ui.fonts", message);
                return;
            }

            m_ActiveFont = font;
            io.FontDefault = font;
            ImGui::GetStyle().FontSizeBase = size;
            m_FontDirty = false;

            std::string message("Applied Windows font: ");
            message += path.filename().string();
            message += " @ ";
            message += std::to_string(size);
            message += "px";
            TUTONES_LOG_INFO("ui.fonts", message);
        }

        void LoadSlot(ThemeTexture& slot, const std::string& name, const char* slotName) noexcept
        {
            if (name.empty())
            {
                slot.Reset();
                return;
            }

            const auto path = m_Storage.ImagePath(name);
            if (slot.LoadFile(path))
                return;

            slot.Reset();
            std::string message("Theme ");
            message += slotName ? slotName : "image";
            message += " failed to load: ";
            message += path.string();
            TUTONES_LOG_WARN("ui.theme", message);
        }

        void ResetTextures() noexcept
        {
            m_Header.Reset();
            m_Footer.Reset();
            m_Background.Reset();
            m_Embedded.Reset();
            m_EmbeddedLoadAttempted = false;
        }

        bool m_Initialized{};
        bool m_ResourcesDirty{true};
        bool m_FontDirty{true};
        bool m_EmbeddedLoadAttempted{};
        ImGuiContext* m_Context{};
        ImFont* m_FallbackFont{};
        ImFont* m_ActiveFont{};
        ThemeDefinition m_Current{DefaultTheme()};
        std::string m_CurrentFile;
        ThemeStorage m_Storage;
        ThemeTexture m_Embedded;
        ThemeTexture m_Header;
        ThemeTexture m_Footer;
        ThemeTexture m_Background;
        std::unordered_map<std::string, ImFont*> m_FontCache;
    };
}
