#pragma once

#include "ThemeDefinition.hpp"
#include "../core/filesystem/FileSystem.hpp"
#include <Windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace Tutones::UI
{
    class ThemeStorage final
    {
    public:
        bool Initialize() noexcept
        {
            auto& fs = Core::FileSystem::Service::Get();
            if (!fs.IsInitialized()) return false;
            m_Themes = fs.UserRoot() / "themes";
            m_Images = fs.UserRoot() / "images";
            if (!fs.EnsureDirectory(m_Themes) || !fs.EnsureDirectory(m_Images)) return false;
            std::array<wchar_t, MAX_PATH> windows{};
            const UINT n = ::GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
            if (n && n < windows.size()) m_Fonts = std::filesystem::path(windows.data()) / L"Fonts";
            Refresh();
            return true;
        }

        void Refresh() noexcept
        {
            m_ThemeFiles = Scan(m_Themes, {".json"});
            m_ImageFiles = Scan(m_Images, {".png", ".jpg", ".jpeg", ".bmp"});
            m_FontFiles = Scan(m_Fonts, {".ttf", ".otf", ".ttc"});
        }

        bool Load(std::string_view file, ThemeDefinition& out) const noexcept
        {
            const std::string leaf = SafeLeaf(file);
            if (leaf.empty()) return false;
            try
            {
                std::ifstream stream(m_Themes / leaf);
                if (!stream) return false;
                nlohmann::json j; stream >> j;
                if (!j.is_object()) return false;
                ThemeDefinition t = DefaultTheme();
                t.name = j.value("name", std::filesystem::path(leaf).stem().string());
                const auto c = j.value("colors", nlohmann::json::object());
                ReadColor(c,"accent",t.accent); ReadColor(c,"accent_hover",t.accentHover);
                ReadColor(c,"accent_dark",t.accentDark); ReadColor(c,"window_bg",t.windowBg);
                ReadColor(c,"body_bg",t.bodyBg); ReadColor(c,"footer_bg",t.footerBg);
                ReadColor(c,"rail_bg",t.railBg); ReadColor(c,"panel_bg",t.panelBg);
                ReadColor(c,"panel_border",t.panelBorder); ReadColor(c,"separator",t.separator);
                ReadColor(c,"control_bg",t.controlBg); ReadColor(c,"control_hover",t.controlHover);
                ReadColor(c,"muted_text",t.mutedText);
                const auto images = j.value("images", nlohmann::json::object());
                t.headerImage = SafeLeaf(images.value("header",std::string{}));
                t.footerImage = SafeLeaf(images.value("footer",std::string{}));
                t.backgroundImage = SafeLeaf(images.value("background",std::string{}));
                t.backgroundOpacity = std::clamp(j.value("background_opacity",t.backgroundOpacity),0.f,1.f);
                const auto font = j.value("font", nlohmann::json::object());
                t.fontFile = SafeLeaf(font.value("file",std::string{}));
                t.fontSize = std::clamp(font.value("size",t.fontSize),9.f,40.f);
                out = std::move(t); return true;
            }
            catch (...) { return false; }
        }

        bool Save(std::string_view requested, const ThemeDefinition& t, std::string& savedFile) noexcept
        {
            savedFile = ThemeFileName(requested);
            if (savedFile.empty()) return false;
            try
            {
                nlohmann::json j{
                    {"version",1},{"name",t.name},
                    {"colors",{{"accent",C(t.accent)},{"accent_hover",C(t.accentHover)},
                        {"accent_dark",C(t.accentDark)},{"window_bg",C(t.windowBg)},
                        {"body_bg",C(t.bodyBg)},{"footer_bg",C(t.footerBg)},
                        {"rail_bg",C(t.railBg)},{"panel_bg",C(t.panelBg)},
                        {"panel_border",C(t.panelBorder)},{"separator",C(t.separator)},
                        {"control_bg",C(t.controlBg)},{"control_hover",C(t.controlHover)},
                        {"muted_text",C(t.mutedText)}}},
                    {"images",{{"header",t.headerImage},{"footer",t.footerImage},{"background",t.backgroundImage}}},
                    {"background_opacity",t.backgroundOpacity},
                    {"font",{{"file",t.fontFile},{"size",t.fontSize}}}
                };
                std::ofstream stream(m_Themes / savedFile, std::ios::trunc);
                if (!stream) return false;
                stream << j.dump(2) << '\n';
                if (!stream.good()) return false;
                Refresh(); return true;
            }
            catch (...) { return false; }
        }

        [[nodiscard]] const auto& ThemeFiles() const noexcept { return m_ThemeFiles; }
        [[nodiscard]] const auto& ImageFiles() const noexcept { return m_ImageFiles; }
        [[nodiscard]] const auto& FontFiles() const noexcept { return m_FontFiles; }
        [[nodiscard]] const auto& ThemesDirectory() const noexcept { return m_Themes; }
        [[nodiscard]] const auto& ImagesDirectory() const noexcept { return m_Images; }
        [[nodiscard]] const auto& FontsDirectory() const noexcept { return m_Fonts; }
        [[nodiscard]] std::filesystem::path ImagePath(std::string_view f) const { return m_Images / SafeLeaf(f); }
        [[nodiscard]] std::filesystem::path FontPath(std::string_view f) const { return m_Fonts / SafeLeaf(f); }

    private:
        static std::string Lower(std::string s) { for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
        static std::string SafeLeaf(std::string_view s) { return s.empty()?std::string{}:std::filesystem::path(std::string(s)).filename().string(); }
        static std::vector<std::string> Scan(const std::filesystem::path& dir, std::initializer_list<const char*> exts)
        {
            std::vector<std::string> out; if (dir.empty()) return out;
            std::error_code ec; for (const auto& e : std::filesystem::directory_iterator(dir,ec))
            {
                if (ec) break; if (!e.is_regular_file(ec) || ec) continue;
                const std::string ext=Lower(e.path().extension().string());
                for (const char* allowed:exts) if(ext==allowed){out.push_back(e.path().filename().string());break;}
            }
            std::sort(out.begin(),out.end()); return out;
        }
        static std::string ThemeFileName(std::string_view input)
        {
            std::string s; for(char c:input){unsigned char u=static_cast<unsigned char>(c); if(std::isalnum(u)||c=='-'||c=='_'||c==' ')s+=c; else if(c=='.')break; else s+='_';}
            while(!s.empty()&&s.front()==' ')s.erase(s.begin()); while(!s.empty()&&s.back()==' ')s.pop_back();
            if(s.empty())s="theme"; return s+".json";
        }
        static nlohmann::json C(const ImVec4& v) { return nlohmann::json::array({v.x,v.y,v.z,v.w}); }
        static void ReadColor(const nlohmann::json& o,const char* key,ImVec4& v)
        {
            auto it=o.find(key); if(it==o.end()||!it->is_array()||it->size()!=4)return;
            try{v={std::clamp((*it)[0].get<float>(),0.f,1.f),std::clamp((*it)[1].get<float>(),0.f,1.f),std::clamp((*it)[2].get<float>(),0.f,1.f),std::clamp((*it)[3].get<float>(),0.f,1.f)};}catch(...){ }
        }
        std::filesystem::path m_Themes,m_Images,m_Fonts;
        std::vector<std::string> m_ThemeFiles,m_ImageFiles,m_FontFiles;
    };
}
