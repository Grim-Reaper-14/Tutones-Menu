#pragma once

#include "ThemeTexture.hpp"
#include "VehicleThumbnailDownloader.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../game/vehicle/VehicleCatalogs.hpp"
#include "../render/Renderer.hpp"

#include <imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
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
            StartArtworkSync();
            ReapRetiredTextures();

            if (classIndex < 0 || classIndex >= static_cast<int>(Game::VehicleCatalogs::VehicleClassNames.size()))
                return {};

            const auto index = static_cast<std::size_t>(classIndex);
            EnsureClassTexture(index);
            if (!m_ClassImages[index] || !m_ClassImages[index]->Valid())
            {
                if (const char* representative = RepresentativeModel(index))
                    VehicleThumbnailDownloader::Get().Request(representative, classIndex);
                return {};
            }

            return VehicleThumbnailView{
                m_ClassImages[index]->Ref(),
                m_ClassImages[index]->Width(),
                m_ClassImages[index]->Height(),
                false,
                m_ClassImagePaths[index],
            };
        }

        [[nodiscard]] VehicleThumbnailView VehicleThumbnail(std::string_view model, int classIndex) noexcept
        {
            EnsureContext();
            StartArtworkSync();
            ReapRetiredTextures();

            const std::string normalized = NormalizeModel(model);
            if (normalized != m_SelectedModel)
            {
                RetireSelectedTexture();
                m_SelectedPath.clear();
                m_SelectedModel = normalized;
                TryLoadSelectedTexture(normalized);
            }
            else if ((!m_SelectedFile || !m_SelectedFile->Valid()) && NamedImageExists(normalized))
            {
                TryLoadSelectedTexture(normalized);
            }

            if (m_SelectedFile && m_SelectedFile->Valid())
            {
                return VehicleThumbnailView{
                    m_SelectedFile->Ref(),
                    m_SelectedFile->Width(),
                    m_SelectedFile->Height(),
                    true,
                    m_SelectedPath,
                };
            }

            // Ask the worker for the selected model first. The worker de-duplicates requests
            // and services this queue ahead of the remaining full-catalog sync.
            VehicleThumbnailDownloader::Get().Request(normalized, classIndex);
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
            const auto sync = VehicleThumbnailDownloader::Get().Snapshot();
            if (sync.running)
                return;

            const auto catalog = Game::Mods::VehicleModificationRuntime::Get().CatalogSnapshot();
            if (catalog.total != Game::VehicleCatalogs::VehicleModels.size()
                || catalog.ready < catalog.total
                || catalog.classes.size() != catalog.total)
            {
                return;
            }
            VehicleThumbnailDownloader::Get().Restart(catalog.classes);
        }

        void Refresh() noexcept
        {
            // Do not destroy a texture that may still be referenced by an in-flight D3D12
            // frame. Retire only the selected image and leave class textures resident.
            // Failed class loads are allowed to retry as the downloader generation advances.
            RetireSelectedTexture();
            m_SelectedPath.clear();
            for (std::size_t i = 0; i < m_ClassAttempted.size(); ++i)
            {
                if (!m_ClassImages[i] || !m_ClassImages[i]->Valid())
                {
                    m_ClassAttempted[i] = false;
                    m_ClassAttemptGeneration[i] = std::numeric_limits<std::uint64_t>::max();
                }
            }

            // Never join/cancel an active WinHTTP worker from the render thread. A retry is
            // accepted only after the current pass has finished.
            RetryFullSync();
        }

        void ReleaseImGuiResources() noexcept
        {
            ResetLoadedTextures();
            m_Context = nullptr;
            m_LastRetireMaintenanceFrame = -1;
        }

    private:
        struct RetiredTexture final
        {
            std::unique_ptr<ThemeTexture> texture{};
            std::uint64_t retireAfterFence{};
        };

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
            // Wait until GTA has classified the complete built-in catalog. Starting before
            // that point gave the downloader -1 class IDs and caused it to probe unrelated
            // folders repeatedly for every missing image.
            const auto catalog = Game::Mods::VehicleModificationRuntime::Get().CatalogSnapshot();
            if (catalog.total != Game::VehicleCatalogs::VehicleModels.size()
                || catalog.ready < catalog.total
                || catalog.classes.size() != catalog.total)
            {
                return;
            }
            VehicleThumbnailDownloader::Get().EnsureStarted(catalog.classes);
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

        void TryLoadSelectedTexture(std::string_view model) noexcept
        {
            if (model.empty())
                return;

            auto next = std::make_unique<ThemeTexture>();
            std::filesystem::path source;
            if (!TryLoadNamedTexture(*next, source, model))
                return;

            m_SelectedPath = std::move(source);
            m_SelectedFile = std::move(next);
        }

        void EnsureClassTexture(std::size_t index) noexcept
        {
            if (index >= m_ClassImages.size())
                return;
            if (m_ClassImages[index] && m_ClassImages[index]->Valid())
                return;

            const auto generation = VehicleThumbnailDownloader::Get().Snapshot().generation;
            if (m_ClassAttempted[index] && m_ClassAttemptGeneration[index] == generation)
                return;

            m_ClassAttempted[index] = true;
            m_ClassAttemptGeneration[index] = generation;

            auto next = std::make_unique<ThemeTexture>();
            std::filesystem::path source;
            const std::string explicitClassImage = "class_" + std::to_string(index);
            if (TryLoadNamedTexture(*next, source, explicitClassImage))
            {
                m_ClassImagePaths[index] = std::move(source);
                m_ClassImages[index] = std::move(next);
                return;
            }

            if (const char* representative = RepresentativeModel(index))
            {
                source.clear();
                if (TryLoadNamedTexture(*next, source, representative))
                {
                    m_ClassImagePaths[index] = std::move(source);
                    m_ClassImages[index] = std::move(next);
                }
            }
        }

        void RetireSelectedTexture() noexcept
        {
            if (!m_SelectedFile)
                return;

            if (!m_SelectedFile->Valid() || !m_Context || ImGui::GetCurrentContext() != m_Context)
            {
                m_SelectedFile.reset();
                return;
            }

            // The old preview can be released as soon as every DX12 frame submitted before
            // this UI change has completed. This uses the renderer's real fence instead of
            // guessing how many ImGui frames are enough.
            const auto retireFence = Render::Renderer::Get().LastSubmittedFenceValue();
            if (retireFence == 0
                || Render::Renderer::Get().CompletedFenceValue() >= retireFence)
            {
                m_SelectedFile.reset();
                return;
            }

            m_RetiredTextures.push_back(RetiredTexture{
                std::move(m_SelectedFile),
                retireFence,
            });
        }

        void ReapRetiredTextures() noexcept
        {
            if (!m_Context || ImGui::GetCurrentContext() != m_Context)
                return;

            const int frame = ImGui::GetFrameCount();
            if (frame == m_LastRetireMaintenanceFrame)
                return;
            m_LastRetireMaintenanceFrame = frame;

            const auto completedFence = Render::Renderer::Get().CompletedFenceValue();
            for (auto it = m_RetiredTextures.begin(); it != m_RetiredTextures.end();)
            {
                if (it->retireAfterFence == 0 || completedFence >= it->retireAfterFence)
                    it = m_RetiredTextures.erase(it);
                else
                    ++it;
            }
        }

        void ResetLoadedTextures() noexcept
        {
            m_SelectedFile.reset();
            m_SelectedModel.clear();
            m_SelectedPath.clear();
            m_RetiredTextures.clear();

            for (std::size_t i = 0; i < m_ClassImages.size(); ++i)
            {
                m_ClassImages[i].reset();
                m_ClassImagePaths[i].clear();
                m_ClassAttempted[i] = false;
                m_ClassAttemptGeneration[i] = std::numeric_limits<std::uint64_t>::max();
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
        int m_LastRetireMaintenanceFrame{-1};

        std::array<std::unique_ptr<ThemeTexture>, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassImages{};
        std::array<std::filesystem::path, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassImagePaths{};
        std::array<bool, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassAttempted{};
        std::array<std::uint64_t, Game::VehicleCatalogs::VehicleClassNames.size()> m_ClassAttemptGeneration{};

        std::unique_ptr<ThemeTexture> m_SelectedFile{};
        std::string m_SelectedModel{};
        std::filesystem::path m_SelectedPath{};
        std::vector<RetiredTexture> m_RetiredTextures{};
    };
}
