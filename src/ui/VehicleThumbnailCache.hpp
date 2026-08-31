#pragma once

#include "ThemeTexture.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::UI
{
    struct VehicleThumbnailView final
    {
        ImTextureRef texture{};
        std::uint32_t width{};
        std::uint32_t height{};
        bool custom{};
        std::filesystem::path source{};

        [[nodiscard]] bool Valid() const noexcept
        {
            return texture.GetTexID() != ImTextureID_Invalid;
        }
    };

    class VehicleThumbnailCache final
    {
    public:
        static VehicleThumbnailCache& Get() noexcept
        {
            static VehicleThumbnailCache instance;
            return instance;
        }

        [[nodiscard]] VehicleThumbnailView ClassThumbnail(int classIndex) noexcept
        {
            EnsureContext();
            if (classIndex < 0 || classIndex >= static_cast<int>(Game::VehicleCatalogs::VehicleClassNames.size()))
                return {};

            const auto index = static_cast<std::size_t>(classIndex);
            EnsureClassTexture(index);

            if (m_ClassCustom[index].Valid())
            {
                return VehicleThumbnailView{
                    m_ClassCustom[index].Ref(),
                    m_ClassCustom[index].Width(),
                    m_ClassCustom[index].Height(),
                    true,
                    m_ClassCustomPath[index],
                };
            }

            return VehicleThumbnailView{
                m_ClassGenerated[index].Ref(),
                m_ClassGenerated[index].width,
                m_ClassGenerated[index].height,
                false,
                {},
            };
        }

        [[nodiscard]] VehicleThumbnailView VehicleThumbnail(std::string_view model, int classIndex) noexcept
        {
            EnsureContext();
            const std::string normalized = NormalizeModel(model);
            if (normalized != m_SelectedModel)
            {
                m_SelectedFile.Reset();
                m_SelectedPath.clear();
                m_SelectedModel = normalized;
                TryLoadNamedTexture(m_SelectedFile, m_SelectedPath, normalized);
            }

            if (m_SelectedFile.Valid())
            {
                return VehicleThumbnailView{
                    m_SelectedFile.Ref(),
                    m_SelectedFile.Width(),
                    m_SelectedFile.Height(),
                    true,
                    m_SelectedPath,
                };
            }

            return ClassThumbnail(classIndex);
        }

        [[nodiscard]] std::filesystem::path ThumbnailFolder() noexcept
        {
            EnsureRootFolder();
            return m_RootFolder;
        }

        void Refresh() noexcept
        {
            EnsureContext();
            m_SelectedFile.Reset();
            m_SelectedModel.clear();
            m_SelectedPath.clear();
            for (std::size_t i = 0; i < m_ClassCustom.size(); ++i)
            {
                m_ClassCustom[i].Reset();
                m_ClassCustomPath[i].clear();
                m_ClassAttempted[i] = false;
            }
        }

        void ReleaseImGuiResources() noexcept
        {
            m_SelectedFile.Reset();
            for (auto& texture : m_ClassCustom)
                texture.Reset();
            for (auto& texture : m_ClassGenerated)
                texture.Reset();
            m_ClassAttempted.fill(false);
            m_SelectedModel.clear();
            m_SelectedPath.clear();
            m_Context = nullptr;
        }

    private:
        struct GeneratedTexture final
        {
            std::unique_ptr<ImTextureData> data{};
            ImGuiContext* context{};
            std::uint32_t width{};
            std::uint32_t height{};

            GeneratedTexture() = default;
            ~GeneratedTexture() { Reset(); }
            GeneratedTexture(const GeneratedTexture&) = delete;
            GeneratedTexture& operator=(const GeneratedTexture&) = delete;

            void Reset() noexcept
            {
                if (data && context && ImGui::GetCurrentContext() == context)
                    ImGui::UnregisterUserTexture(data.get());
                data.reset();
                context = nullptr;
                width = 0;
                height = 0;
            }

            [[nodiscard]] ImTextureRef Ref() const noexcept
            {
                return data ? data->GetTexRef() : ImTextureRef{};
            }

            [[nodiscard]] bool Valid() const noexcept
            {
                return data != nullptr;
            }
        };

        VehicleThumbnailCache() = default;
        ~VehicleThumbnailCache() = default;
        VehicleThumbnailCache(const VehicleThumbnailCache&) = delete;
        VehicleThumbnailCache& operator=(const VehicleThumbnailCache&) = delete;

        static constexpr std::uint32_t PreviewWidth = 240;
        static constexpr std::uint32_t PreviewHeight = 120;

        static void PutPixel(
            std::vector<std::uint8_t>& pixels,
            int x,
            int y,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b,
            std::uint8_t a = 255) noexcept
        {
            if (x < 0 || y < 0 || x >= static_cast<int>(PreviewWidth) || y >= static_cast<int>(PreviewHeight))
                return;
            const std::size_t offset = (static_cast<std::size_t>(y) * PreviewWidth + static_cast<std::size_t>(x)) * 4u;
            pixels[offset + 0] = r;
            pixels[offset + 1] = g;
            pixels[offset + 2] = b;
            pixels[offset + 3] = a;
        }

        static void FillRect(
            std::vector<std::uint8_t>& pixels,
            int x,
            int y,
            int width,
            int height,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            for (int py = y; py < y + height; ++py)
                for (int px = x; px < x + width; ++px)
                    PutPixel(pixels, px, py, r, g, b);
        }

        static void FillCircle(
            std::vector<std::uint8_t>& pixels,
            int cx,
            int cy,
            int radius,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            const int r2 = radius * radius;
            for (int y = -radius; y <= radius; ++y)
            {
                for (int x = -radius; x <= radius; ++x)
                {
                    if (x * x + y * y <= r2)
                        PutPixel(pixels, cx + x, cy + y, r, g, b);
                }
            }
        }

        static void DrawLine(
            std::vector<std::uint8_t>& pixels,
            int x0,
            int y0,
            int x1,
            int y1,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b,
            int thickness = 1) noexcept
        {
            const int dx = std::abs(x1 - x0);
            const int sx = x0 < x1 ? 1 : -1;
            const int dy = -std::abs(y1 - y0);
            const int sy = y0 < y1 ? 1 : -1;
            int error = dx + dy;

            for (;;)
            {
                FillCircle(pixels, x0, y0, std::max(0, thickness - 1), r, g, b);
                if (x0 == x1 && y0 == y1)
                    break;
                const int twice = error * 2;
                if (twice >= dy)
                {
                    error += dy;
                    x0 += sx;
                }
                if (twice <= dx)
                {
                    error += dx;
                    y0 += sy;
                }
            }
        }

        static void DrawWheel(std::vector<std::uint8_t>& pixels, int x, int y, int radius = 14) noexcept
        {
            FillCircle(pixels, x, y, radius, 11, 14, 19);
            FillCircle(pixels, x, y, radius - 4, 74, 81, 92);
            FillCircle(pixels, x, y, radius - 8, 18, 22, 29);
        }

        static void DrawCar(
            std::vector<std::uint8_t>& pixels,
            int classIndex,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            const bool tall = classIndex == 2 || classIndex == 10 || classIndex == 11 || classIndex == 12
                || classIndex == 17 || classIndex == 18 || classIndex == 19 || classIndex == 20;
            const bool wedge = classIndex == 6 || classIndex == 7;
            const bool longBody = classIndex == 4 || classIndex == 5;

            const int bodyX = longBody ? 28 : 35;
            const int bodyW = longBody ? 184 : 170;
            const int bodyY = tall ? 57 : 66;
            const int bodyH = tall ? 35 : 26;
            FillRect(pixels, bodyX, bodyY, bodyW, bodyH, r, g, b);

            if (wedge)
            {
                for (int row = 0; row < 27; ++row)
                {
                    const int start = 67 + row / 2;
                    const int width = 102 - row;
                    if (width > 0)
                        FillRect(pixels, start, 43 + row, width, 1, static_cast<std::uint8_t>(std::min(255, r + 18)), static_cast<std::uint8_t>(std::min(255, g + 18)), static_cast<std::uint8_t>(std::min(255, b + 18)));
                }
            }
            else
            {
                const int roofY = tall ? 31 : 45;
                const int roofH = tall ? 32 : 23;
                FillRect(pixels, 69, roofY, tall ? 102 : 92, roofH, static_cast<std::uint8_t>(std::min(255, r + 18)), static_cast<std::uint8_t>(std::min(255, g + 18)), static_cast<std::uint8_t>(std::min(255, b + 18)));
                FillRect(pixels, 78, roofY + 5, tall ? 84 : 74, roofH - 8, 31, 48, 63);
            }

            FillRect(pixels, bodyX + 8, bodyY + 6, 23, 6, 238, 212, 104);
            FillRect(pixels, bodyX + bodyW - 28, bodyY + 7, 20, 6, 210, 49, 50);
            DrawWheel(pixels, bodyX + 39, bodyY + bodyH, tall ? 15 : 14);
            DrawWheel(pixels, bodyX + bodyW - 39, bodyY + bodyH, tall ? 15 : 14);
        }

        static void DrawMotorcycle(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b,
            bool cycle) noexcept
        {
            DrawWheel(pixels, 72, 82, 18);
            DrawWheel(pixels, 168, 82, 18);
            DrawLine(pixels, 72, 82, 112, 53, r, g, b, 3);
            DrawLine(pixels, 112, 53, 146, 82, r, g, b, 3);
            DrawLine(pixels, 146, 82, 72, 82, r, g, b, 3);
            DrawLine(pixels, 112, 53, 130, 82, r, g, b, 3);
            DrawLine(pixels, 143, 49, 168, 82, 80, 88, 98, 2);
            DrawLine(pixels, 137, 48, 153, 45, 80, 88, 98, 2);
            if (!cycle)
            {
                FillRect(pixels, 100, 57, 47, 18, r, g, b);
                FillRect(pixels, 93, 52, 25, 8, static_cast<std::uint8_t>(std::min(255, r + 25)), static_cast<std::uint8_t>(std::min(255, g + 25)), static_cast<std::uint8_t>(std::min(255, b + 25)));
            }
        }

        static void DrawBoat(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            for (int row = 0; row < 34; ++row)
            {
                const int inset = row / 3;
                FillRect(pixels, 32 + inset, 63 + row, 176 - inset * 2, 1, r, g, b);
            }
            FillRect(pixels, 85, 42, 72, 24, 225, 232, 237);
            FillRect(pixels, 94, 47, 54, 15, 38, 61, 76);
            DrawLine(pixels, 18, 103, 220, 103, 43, 105, 150, 2);
        }

        static void DrawHelicopter(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            FillCircle(pixels, 114, 67, 30, r, g, b);
            FillRect(pixels, 112, 50, 72, 20, r, g, b);
            DrawLine(pixels, 178, 60, 211, 44, r, g, b, 3);
            DrawLine(pixels, 205, 34, 205, 56, 80, 88, 98, 2);
            DrawLine(pixels, 188, 45, 220, 45, 80, 88, 98, 2);
            DrawLine(pixels, 114, 31, 114, 20, 80, 88, 98, 2);
            DrawLine(pixels, 51, 20, 177, 20, 80, 88, 98, 2);
            FillRect(pixels, 93, 53, 34, 20, 34, 57, 73);
            DrawLine(pixels, 83, 93, 151, 93, 80, 88, 98, 2);
            DrawLine(pixels, 93, 83, 83, 93, 80, 88, 98, 2);
            DrawLine(pixels, 142, 83, 151, 93, 80, 88, 98, 2);
        }

        static void DrawPlane(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            FillRect(pixels, 42, 57, 157, 18, r, g, b);
            FillRect(pixels, 89, 31, 38, 69, static_cast<std::uint8_t>(std::min(255, r + 15)), static_cast<std::uint8_t>(std::min(255, g + 15)), static_cast<std::uint8_t>(std::min(255, b + 15)));
            FillRect(pixels, 54, 44, 28, 10, r, g, b);
            FillRect(pixels, 178, 45, 25, 10, r, g, b);
            FillRect(pixels, 47, 61, 28, 8, 35, 57, 73);
            DrawLine(pixels, 199, 58, 224, 66, r, g, b, 2);
        }

        static void DrawRail(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            FillRect(pixels, 37, 38, 166, 54, r, g, b);
            for (int x = 49; x <= 164; x += 29)
                FillRect(pixels, x, 47, 21, 18, 33, 57, 74);
            FillRect(pixels, 42, 74, 156, 7, 220, 225, 232);
            DrawWheel(pixels, 68, 93, 10);
            DrawWheel(pixels, 172, 93, 10);
            DrawLine(pixels, 20, 106, 220, 106, 97, 103, 112, 2);
        }

        static void DrawOpenWheel(
            std::vector<std::uint8_t>& pixels,
            std::uint8_t r,
            std::uint8_t g,
            std::uint8_t b) noexcept
        {
            DrawWheel(pixels, 58, 76, 17);
            DrawWheel(pixels, 58, 98, 17);
            DrawWheel(pixels, 182, 76, 17);
            DrawWheel(pixels, 182, 98, 17);
            FillRect(pixels, 83, 72, 74, 21, r, g, b);
            FillRect(pixels, 109, 50, 22, 26, static_cast<std::uint8_t>(std::min(255, r + 18)), static_cast<std::uint8_t>(std::min(255, g + 18)), static_cast<std::uint8_t>(std::min(255, b + 18)));
            FillRect(pixels, 40, 81, 34, 7, r, g, b);
            FillRect(pixels, 166, 81, 34, 7, r, g, b);
            FillRect(pixels, 101, 43, 38, 6, 80, 88, 98);
        }

        [[nodiscard]] static std::vector<std::uint8_t> BuildClassPixels(int classIndex)
        {
            std::vector<std::uint8_t> pixels(PreviewWidth * PreviewHeight * 4u, 255);
            const std::uint8_t accentR = static_cast<std::uint8_t>(72 + ((classIndex * 47) % 150));
            const std::uint8_t accentG = static_cast<std::uint8_t>(78 + ((classIndex * 29) % 140));
            const std::uint8_t accentB = static_cast<std::uint8_t>(92 + ((classIndex * 61) % 135));

            for (std::uint32_t y = 0; y < PreviewHeight; ++y)
            {
                for (std::uint32_t x = 0; x < PreviewWidth; ++x)
                {
                    const auto shade = static_cast<std::uint8_t>(12 + (y * 18u / PreviewHeight) + ((x / 20u) % 2u) * 2u);
                    PutPixel(pixels, static_cast<int>(x), static_cast<int>(y), shade, static_cast<std::uint8_t>(shade + 4), static_cast<std::uint8_t>(shade + 10));
                }
            }

            FillRect(pixels, 0, 101, static_cast<int>(PreviewWidth), 19, 17, 22, 29);
            DrawLine(pixels, 12, 101, 228, 101, 60, 67, 78, 1);

            switch (classIndex)
            {
            case 8:
                DrawMotorcycle(pixels, accentR, accentG, accentB, false);
                break;
            case 13:
                DrawMotorcycle(pixels, accentR, accentG, accentB, true);
                break;
            case 14:
                DrawBoat(pixels, accentR, accentG, accentB);
                break;
            case 15:
                DrawHelicopter(pixels, accentR, accentG, accentB);
                break;
            case 16:
                DrawPlane(pixels, accentR, accentG, accentB);
                break;
            case 21:
                DrawRail(pixels, accentR, accentG, accentB);
                break;
            case 22:
                DrawOpenWheel(pixels, accentR, accentG, accentB);
                break;
            default:
                DrawCar(pixels, classIndex, accentR, accentG, accentB);
                break;
            }

            return pixels;
        }

        static bool CreateGeneratedTexture(GeneratedTexture& output, int classIndex) noexcept
        {
            if (!ImGui::GetCurrentContext())
                return false;

            auto pixels = BuildClassPixels(classIndex);
            auto texture = std::make_unique<ImTextureData>();
            texture->Create(ImTextureFormat_RGBA32, static_cast<int>(PreviewWidth), static_cast<int>(PreviewHeight));
            if (!texture->GetPixels())
                return false;

            std::copy(pixels.begin(), pixels.end(), texture->GetPixels());
            output.Reset();
            ImGui::RegisterUserTexture(texture.get());
            output.context = ImGui::GetCurrentContext();
            output.width = PreviewWidth;
            output.height = PreviewHeight;
            output.data = std::move(texture);
            return true;
        }

        static std::string NormalizeModel(std::string_view model)
        {
            std::string out(model);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                if (std::isalnum(c) || c == '_' || c == '-')
                    return static_cast<char>(std::tolower(c));
                return '_';
            });
            return out;
        }

        void EnsureRootFolder() noexcept
        {
            if (!m_RootFolder.empty())
                return;

            const char* localAppData = std::getenv("LOCALAPPDATA");
            m_RootFolder = (localAppData && *localAppData)
                ? std::filesystem::path(localAppData) / "TutonesMenu" / "vehicle_thumbnails"
                : std::filesystem::path(".") / "TutonesMenu" / "vehicle_thumbnails";
            std::error_code error;
            std::filesystem::create_directories(m_RootFolder, error);
        }

        bool TryLoadNamedTexture(ThemeTexture& texture, std::filesystem::path& source, std::string_view baseName) noexcept
        {
            EnsureRootFolder();
            static constexpr std::array<const char*, 5> Extensions{{".png", ".jpg", ".jpeg", ".bmp", ".webp"}};
            for (const char* extension : Extensions)
            {
                const auto path = m_RootFolder / (std::string(baseName) + extension);
                std::error_code error;
                if (!std::filesystem::is_regular_file(path, error) || error)
                    continue;
                if (texture.LoadFile(path))
                {
                    source = path;
                    return true;
                }
            }
            return false;
        }

        void EnsureClassTexture(std::size_t index) noexcept
        {
            if (index >= m_ClassGenerated.size())
                return;

            if (!m_ClassAttempted[index])
            {
                m_ClassAttempted[index] = true;
                const std::string baseName = "class_" + std::to_string(index);
                static_cast<void>(TryLoadNamedTexture(m_ClassCustom[index], m_ClassCustomPath[index], baseName));
            }

            if (!m_ClassCustom[index].Valid() && !m_ClassGenerated[index].Valid())
                static_cast<void>(CreateGeneratedTexture(m_ClassGenerated[index], static_cast<int>(index)));
        }

        void EnsureContext() noexcept
        {
            auto* context = ImGui::GetCurrentContext();
            if (context == m_Context)
                return;

            ReleaseImGuiResources();
            m_Context = context;
        }

        ImGuiContext* m_Context{};
        std::filesystem::path m_RootFolder{};
        std::array<ThemeTexture, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassCustom{};
        std::array<GeneratedTexture, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassGenerated{};
        std::array<std::filesystem::path, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassCustomPath{};
        std::array<bool, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassAttempted{};
        ThemeTexture m_SelectedFile{};
        std::string m_SelectedModel{};
        std::filesystem::path m_SelectedPath{};
    };
}
