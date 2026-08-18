#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::Core::FileSystem
{
    enum class Root
    {
        Module,
        Data,
        Config,
        Logs,
        Cache,
        Assets,
        Scripts,
        Dumps,
        SavedVehicles,
    };

    class Service final
    {
    public:
        static Service& Get() noexcept;

        bool Initialize(const std::filesystem::path& moduleDirectory);
        void Shutdown() noexcept;

        [[nodiscard]] std::filesystem::path ModuleRoot() const;
        [[nodiscard]] std::filesystem::path UserRoot() const;
        [[nodiscard]] std::filesystem::path RootPath(Root root) const;
        [[nodiscard]] std::filesystem::path Resolve(Root root, std::filesystem::path relative) const;

        bool EnsureDirectory(Root root) const noexcept;
        bool EnsureDirectory(const std::filesystem::path& path) const noexcept;

        [[nodiscard]] bool Exists(const std::filesystem::path& path) const noexcept;
        [[nodiscard]] bool IsFile(const std::filesystem::path& path) const noexcept;
        [[nodiscard]] bool IsDirectory(const std::filesystem::path& path) const noexcept;

        bool ReadText(const std::filesystem::path& path, std::string& output) const noexcept;
        [[nodiscard]] std::string ReadText(const std::filesystem::path& path) const;
        bool WriteText(const std::filesystem::path& path, std::string_view content) const noexcept;
        bool AppendText(const std::filesystem::path& path, std::string_view content) const noexcept;

        bool ReadBinary(const std::filesystem::path& path, std::vector<std::byte>& output) const noexcept;
        bool WriteBinary(const std::filesystem::path& path, std::span<const std::byte> data) const noexcept;

        bool Copy(const std::filesystem::path& source, const std::filesystem::path& target, bool overwrite = false) const noexcept;
        bool Move(const std::filesystem::path& source, const std::filesystem::path& target, bool overwrite = false) const noexcept;
        bool Remove(const std::filesystem::path& path) const noexcept;
        bool RemoveAll(const std::filesystem::path& path) const noexcept;

        [[nodiscard]] std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& directory, bool recursive = false) const;
        [[nodiscard]] std::vector<std::filesystem::path> ListDirectories(const std::filesystem::path& directory, bool recursive = false) const;

        [[nodiscard]] bool IsInitialized() const noexcept;

    private:
        Service() = default;
        ~Service() = default;
        Service(const Service&) = delete;
        Service& operator=(const Service&) = delete;

        std::filesystem::path m_ModuleRoot;
        std::filesystem::path m_UserRoot;
        bool m_Initialized{};
    };

    [[nodiscard]] inline Service& Get()
    {
        return Service::Get();
    }

    [[nodiscard]] inline std::filesystem::path RootPath(Root root)
    {
        return Service::Get().RootPath(root);
    }
}
