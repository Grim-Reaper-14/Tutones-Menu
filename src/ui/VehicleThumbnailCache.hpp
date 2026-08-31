#pragma once

#include "ThemeTexture.hpp"
#include "VehicleThumbnailDownloader.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"

#include <imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

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
            StartArtworkSync();
            RefreshFromArtworkSync();

            if (classIndex < 0 || classIndex >= static_cast<int>(Game::VehicleCatalogs::VehicleClassNames.size()))
                return {};

            const auto index = static_cast<std::size_t>(classIndex);
            EnsureClassTexture(index);
            if (!m_ClassImages[index].Valid())
                return {};

            return VehicleThumbnailView{
                m_ClassImages[index].Ref(),
                m_ClassImages[index].Width(),
                m_ClassImages[index].Height(),
                true,
                m_ClassImagePaths[index],
            };
        }

        [[nodiscard]] VehicleThumbnailView VehicleThumbnail(std::string_view model, int classIndex) noexcept
        {
            EnsureContext();
            StartArtworkSync();
            RefreshFromArtworkSync();

            const std::string normalized = NormalizeModel(model);
            if (normalized != m_SelectedModel || (!m_SelectedFile.Valid() && NamedImageExists(normalized)))
            {
                m_SelectedFile.Reset();
                m_SelectedPath.clear();
                m_SelectedModel = normalized;
                static_cast<void>(TryLoadNamedTexture(m_SelectedFile, m_SelectedPath, normalized));
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

        [[nodiscard]] VehicleThumbnailSyncSnapshot SyncSnapshot() const
        {
            return VehicleThumbnailDownloader::Get().Snapshot();
        }

        void RetryFullSync() noexcept
        {
            VehicleThumbnailDownloader::Get().Restart();
        }

        void Refresh() noexcept
        {
            EnsureContext();
            ResetLoadedTextures();
        }

        void ReleaseImGuiResources() noexcept
        {
            ResetLoadedTextures();
            m_Context = nullptr;
        }

    private:
        VehicleThumbnailCache() = default;
        ~VehicleThumbnailCache() = default;
        VehicleThumbnailCache(const VehicleThumbnailCache&) = delete;
        VehicleThumbnailCache& operator=(const VehicleThumbnailCache&) = delete;

        [[nodiscard]] static const char* RepresentativeModel(std::size_t classIndex) noexcept
        {
            static constexpr std::array<const char*, 23> Models{{
                "panto",
                "tailgater",
                "baller",
                "sentinel",
                "dominator",
                "turismo2",
                "jester",
                "adder",
                "bati",
                "mesa3",
                "bulldozer",
                "towtruck",
                "speedo",
                "bmx",
                "speeder",
                "maverick",
                "luxor",
                "taxi",
                "police",
                "rhino",
                "phantom",
                "freight",
                "formula",
            }};

            return classIndex < Models.size() ? Models[classIndex] : nullptr;
        }

        static std::string NormalizeModel(std::string_view model)
        {
            std::string output;
            output.reserve(model.size());
            for (const unsigned char character : model)
            {
                if ((character >= 'a' && character <= 'z')
                    || (character >= '0' && character <= '9')
                    || character == '_'
                    || character == '-')
                {
                    output.push_back(static_cast<char>(character));
                }
                else if (character >= 'A' && character <= 'Z')
                {
                    output.push_back(static_cast<char>(character - 'A' + 'a'));
                }
                else
                {
                    output.push_back('_');
                }
            }
            return output;
        }

        void StartArtworkSync() noexcept
        {
            VehicleThumbnailDownloader::Get().EnsureStarted();
        }

        void RefreshFromArtworkSync() noexcept
        {
            const auto snapshot = VehicleThumbnailDownloader::Get().Snapshot();
            if (snapshot.generation == m_LastSyncGeneration)
                return;

            // Avoid rebuilding ImGui textures for every single completed network request.
            // Refresh in small batches while syncing, then perform a final refresh at completion.
            if (!snapshot.completed && snapshot.generation < m_LastSyncGeneration + 8u)
                return;

            m_LastSyncGeneration = snapshot.generation;
            ResetLoadedTextures();
        }

        void EnsureRootFolder() noexcept
        {
            if (!m_RootFolder.empty())
                return;

            m_RootFolder = VehicleThumbnailDownloader::Get().ThumbnailFolder();
            std::error_code error;
            std::filesystem::create_directories(m_RootFolder, error);
        }

        [[nodiscard]] bool NamedImageExists(std::string_view baseName) noexcept
        {
            EnsureRootFolder();
            static constexpr std::array<const char*, 5> Extensions{{".png", ".jpg", ".jpeg", ".bmp", ".webp"}};
            for (const char* extension : Extensions)
            {
                std::error_code error;
                if (std::filesystem::is_regular_file(
                        m_RootFolder / (std::string(baseName) + extension),
                        error)
                    && !error)
                {
                    return true;
                }
            }
            return false;
        }

        bool TryLoadNamedTexture(
            ThemeTexture& texture,
            std::filesystem::path& source,
            std::string_view baseName) noexcept
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
            if (index >= m_ClassImages.size() || m_ClassAttempted[index])
                return;

            m_ClassAttempted[index] = true;

            // Preserve support for explicitly supplied class_N artwork first.
            const std::string explicitClassImage = "class_" + std::to_string(index);
            if (TryLoadNamedTexture(m_ClassImages[index], m_ClassImagePaths[index], explicitClassImage))
                return;

            // Otherwise use an actual GTA vehicle from that class as the category picture.
            if (const char* representative = RepresentativeModel(index))
                static_cast<void>(TryLoadNamedTexture(m_ClassImages[index], m_ClassImagePaths[index], representative));
        }

        void ResetLoadedTextures() noexcept
        {
            m_SelectedFile.Reset();
            m_SelectedModel.clear();
            m_SelectedPath.clear();

            for (std::size_t i = 0; i < m_ClassImages.size(); ++i)
            {
                m_ClassImages[i].Reset();
                m_ClassImagePaths[i].clear();
                m_ClassAttempted[i] = false;
            }
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
        std::uint64_t m_LastSyncGeneration{};

        std::array<ThemeTexture, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassImages{};
        std::array<std::filesystem::path, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassImagePaths{};
        std::array<bool, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassAttempted{};

        ThemeTexture m_SelectedFile{};
        std::string m_SelectedModel{};
        std::filesystem::path m_SelectedPath{};
    };
}
