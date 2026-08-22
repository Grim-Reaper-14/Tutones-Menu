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
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Tutones::UI
{
    class ThemeStorage final
    {
    public:
        bool Initialize() noexcept
        {
            auto& fs = Core::FileSystem::Service::Get();
            if (!fs.IsInitialized())
                return false;

            m_Themes = fs.UserRoot() / "themes";
            m_Images = fs.UserRoot() / "images";

            // Images/themes are Tutones-owned folders. Fonts are discovered from
            // the Windows installation's Fonts directory (normally C:\\Windows\\Fonts).
            std::array<wchar_t, MAX_PATH> windowsDirectory{};
            const UINT length = ::GetWindowsDirectoryW(
                windowsDirectory.data(),
                static_cast<UINT>(windowsDirectory.size()));
            if (length != 0 && length < windowsDirectory.size())
                m_Fonts = std::filesystem::path(windowsDirectory.data()) / L"Fonts";
            else
                m_Fonts.clear();

            if (!fs.EnsureDirectory(m_Themes) || !fs.EnsureDirectory(m_Images))
                return false;

            Refresh();
            return true;
        }

        void Refresh() noexcept
        {
            RefreshThemes();
            RefreshImages();
            RefreshFonts();
        }

        void RefreshThemes() noexcept
        {
            m_ThemeFiles = Scan(m_Themes, {".json"});
        }

        void RefreshImages() noexcept
        {
            m_ImageFiles = Scan(m_Images, {".png", ".jpg", ".jpeg", ".bmp"});
        }

        void RefreshFonts() noexcept
        {
            m_FontFiles = Scan(m_Fonts, {".ttf", ".otf", ".ttc"});
        }

        bool Load(std::string_view file, ThemeDefinition& out) const noexcept
        {
            const std::string leaf = SafeLeaf(file);
            if (leaf.empty())
                return false;

            try
            {
                std::ifstream stream(m_Themes / leaf);
                if (!stream)
                    return false;

                nlohmann::json j;
                stream >> j;
                if (!j.is_object())
                    return false;

                ThemeDefinition t = DefaultTheme();
                t.name = j.value("name", std::filesystem::path(leaf).stem().string());

                const auto colors = j.value("colors", nlohmann::json::object());
                ReadColor(colors, "accent", t.accent);
                ReadColor(colors, "accent_hover", t.accentHover);
                ReadColor(colors, "accent_dark", t.accentDark);
                ReadColor(colors, "window_bg", t.windowBg);
                ReadColor(colors, "body_bg", t.bodyBg);
                ReadColor(colors, "footer_bg", t.footerBg);
                ReadColor(colors, "rail_bg", t.railBg);
                ReadColor(colors, "panel_bg", t.panelBg);
                ReadColor(colors, "panel_border", t.panelBorder);
                ReadColor(colors, "separator", t.separator);
                ReadColor(colors, "control_bg", t.controlBg);
                ReadColor(colors, "control_hover", t.controlHover);
                ReadColor(colors, "muted_text", t.mutedText);

                const auto images = j.value("images", nlohmann::json::object());
                t.headerImage = SafeLeaf(images.value("header", std::string{}));
                t.footerImage = SafeLeaf(images.value("footer", std::string{}));
                t.backgroundImage = SafeLeaf(images.value("background", std::string{}));
                t.backgroundOpacity = std::clamp(
                    j.value("background_opacity", t.backgroundOpacity), 0.0f, 1.0f);

                const auto font = j.value("font", nlohmann::json::object());
                t.fontFile = SafeLeaf(font.value("file", std::string{}));
                t.fontSize = std::clamp(font.value("size", t.fontSize), 9.0f, 40.0f);

                out = std::move(t);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool Save(std::string_view requested, const ThemeDefinition& t, std::string& savedFile) noexcept
        {
            savedFile = ThemeFileName(requested);
            if (savedFile.empty())
                return false;

            try
            {
                nlohmann::json j{
                    {"version", 1},
                    {"name", t.name},
                    {"colors", {
                        {"accent", C(t.accent)},
                        {"accent_hover", C(t.accentHover)},
                        {"accent_dark", C(t.accentDark)},
                        {"window_bg", C(t.windowBg)},
                        {"body_bg", C(t.bodyBg)},
                        {"footer_bg", C(t.footerBg)},
                        {"rail_bg", C(t.railBg)},
                        {"panel_bg", C(t.panelBg)},
                        {"panel_border", C(t.panelBorder)},
                        {"separator", C(t.separator)},
                        {"control_bg", C(t.controlBg)},
                        {"control_hover", C(t.controlHover)},
                        {"muted_text", C(t.mutedText)}
                    }},
                    {"images", {
                        {"header", t.headerImage},
                        {"footer", t.footerImage},
                        {"background", t.backgroundImage}
                    }},
                    {"background_opacity", t.backgroundOpacity},
                    {"font", {
                        {"file", t.fontFile},
                        {"size", t.fontSize}
                    }}
                };

                std::ofstream stream(m_Themes / savedFile, std::ios::trunc);
                if (!stream)
                    return false;

                stream << j.dump(2) << '\n';
                if (!stream.good())
                    return false;

                RefreshThemes();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] const std::vector<std::string>& ThemeFiles() const noexcept { return m_ThemeFiles; }
        [[nodiscard]] const std::vector<std::string>& ImageFiles() const noexcept { return m_ImageFiles; }
        [[nodiscard]] const std::vector<std::string>& FontFiles() const noexcept { return m_FontFiles; }

        [[nodiscard]] const std::filesystem::path& ThemesDirectory() const noexcept { return m_Themes; }
        [[nodiscard]] const std::filesystem::path& ImagesDirectory() const noexcept { return m_Images; }
        [[nodiscard]] const std::filesystem::path& FontsDirectory() const noexcept { return m_Fonts; }

        [[nodiscard]] std::filesystem::path ImagePath(std::string_view file) const
        {
            const std::string leaf = SafeLeaf(file);
            return leaf.empty() ? std::filesystem::path{} : m_Images / leaf;
        }

        [[nodiscard]] std::filesystem::path FontPath(std::string_view file) const
        {
            const std::string leaf = SafeLeaf(file);
            return leaf.empty() ? std::filesystem::path{} : m_Fonts / leaf;
        }

    private:
        static std::string Lower(std::string value)
        {
            for (char& c : value)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        }

        static std::string SafeLeaf(std::string_view value)
        {
            return value.empty()
                ? std::string{}
                : std::filesystem::path(std::string(value)).filename().string();
        }

        static std::vector<std::string> Scan(
            const std::filesystem::path& directory,
            std::initializer_list<const char*> extensions)
        {
            std::vector<std::string> files;
            if (directory.empty())
                return files;

            std::error_code error;
            std::filesystem::directory_iterator iterator(directory, error);
            if (error)
                return files;

            for (const auto& entry : iterator)
            {
                if (error)
                    break;
                if (!entry.is_regular_file(error) || error)
                {
                    error.clear();
                    continue;
                }

                const std::string extension = Lower(entry.path().extension().string());
                for (const char* allowed : extensions)
                {
                    if (extension == allowed)
                    {
                        files.push_back(entry.path().filename().string());
                        break;
                    }
                }
            }

            std::sort(files.begin(), files.end(), [](const std::string& lhs, const std::string& rhs) {
                return Lower(lhs) < Lower(rhs);
            });
            return files;
        }

        static std::string ThemeFileName(std::string_view input)
        {
            std::string value;
            for (char c : input)
            {
                const unsigned char u = static_cast<unsigned char>(c);
                if (std::isalnum(u) || c == '-' || c == '_' || c == ' ')
                    value += c;
                else if (c == '.')
                    break;
                else
                    value += '_';
            }

            while (!value.empty() && value.front() == ' ')
                value.erase(value.begin());
            while (!value.empty() && value.back() == ' ')
                value.pop_back();
            if (value.empty())
                value = "theme";
            return value + ".json";
        }

        static nlohmann::json C(const ImVec4& value)
        {
            return nlohmann::json::array({value.x, value.y, value.z, value.w});
        }

        static void ReadColor(const nlohmann::json& object, const char* key, ImVec4& value)
        {
            const auto it = object.find(key);
            if (it == object.end() || !it->is_array() || it->size() != 4)
                return;

            try
            {
                value = {
                    std::clamp((*it)[0].get<float>(), 0.0f, 1.0f),
                    std::clamp((*it)[1].get<float>(), 0.0f, 1.0f),
                    std::clamp((*it)[2].get<float>(), 0.0f, 1.0f),
                    std::clamp((*it)[3].get<float>(), 0.0f, 1.0f)
                };
            }
            catch (...)
            {
            }
        }

        std::filesystem::path m_Themes;
        std::filesystem::path m_Images;
        std::filesystem::path m_Fonts;
        std::vector<std::string> m_ThemeFiles;
        std::vector<std::string> m_ImageFiles;
        std::vector<std::string> m_FontFiles;
    };
}
