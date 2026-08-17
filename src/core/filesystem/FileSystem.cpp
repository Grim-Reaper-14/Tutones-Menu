#include "FileSystem.hpp"

#include "../logging/Logger.hpp"

#include <fstream>
#include <iterator>

namespace Tutones::Core::FileSystem
{
    namespace
    {
        std::filesystem::path PathFor(const std::filesystem::path& module, Root root)
        {
            switch (root)
            {
            case Root::Module: return module;
            case Root::Data: return module / "data";
            case Root::Config: return module / "config";
            case Root::Logs: return module / "logs";
            case Root::Cache: return module / "cache";
            case Root::Assets: return module / "assets";
            case Root::Scripts: return module / "scripts";
            case Root::Dumps: return module / "dumps";
            }
            return module;
        }

        bool ContainsParentTraversal(const std::filesystem::path& path)
        {
            for (const auto& component : path)
                if (component == "..")
                    return true;
            return false;
        }
    }

    Service& Service::Get() noexcept
    {
        static Service instance;
        return instance;
    }

    bool Service::Initialize(const std::filesystem::path& moduleDirectory)
    {
        if (moduleDirectory.empty())
            return false;

        std::error_code ec;
        m_ModuleRoot = std::filesystem::weakly_canonical(moduleDirectory, ec);
        if (ec)
            m_ModuleRoot = std::filesystem::absolute(moduleDirectory, ec);
        if (m_ModuleRoot.empty())
            return false;

        for (const auto root : {Root::Data, Root::Config, Root::Logs, Root::Cache, Root::Assets, Root::Scripts, Root::Dumps})
        {
            if (!EnsureDirectory(root))
            {
                TUTONES_LOG_ERROR("filesystem", "Failed to create a Tutones data directory");
                return false;
            }
        }

        m_Initialized = true;
        TUTONES_LOG_INFO("filesystem", "Filesystem service initialized");
        return true;
    }

    void Service::Shutdown() noexcept
    {
        m_ModuleRoot.clear();
        m_Initialized = false;
    }

    std::filesystem::path Service::ModuleRoot() const { return m_ModuleRoot; }
    std::filesystem::path Service::RootPath(Root root) const { return PathFor(m_ModuleRoot, root); }

    std::filesystem::path Service::Resolve(Root root, std::filesystem::path relative) const
    {
        if (relative.empty())
            return RootPath(root);
        if (relative.is_absolute() || ContainsParentTraversal(relative))
            return {};

        return RootPath(root) / std::move(relative);
    }

    bool Service::EnsureDirectory(Root root) const noexcept { return EnsureDirectory(RootPath(root)); }

    bool Service::EnsureDirectory(const std::filesystem::path& path) const noexcept
    {
        if (path.empty())
            return false;
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
            return std::filesystem::is_directory(path, ec);
        return std::filesystem::create_directories(path, ec) && !ec;
    }

    bool Service::Exists(const std::filesystem::path& path) const noexcept
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && !ec;
    }

    bool Service::IsFile(const std::filesystem::path& path) const noexcept
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(path, ec) && !ec;
    }

    bool Service::IsDirectory(const std::filesystem::path& path) const noexcept
    {
        std::error_code ec;
        return std::filesystem::is_directory(path, ec) && !ec;
    }

    bool Service::ReadText(const std::filesystem::path& path, std::string& output) const noexcept
    {
        try
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return false;
            output.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            return true;
        }
        catch (...) { return false; }
    }

    std::string Service::ReadText(const std::filesystem::path& path) const
    {
        std::string output;
        static_cast<void>(ReadText(path, output));
        return output;
    }

    bool Service::WriteText(const std::filesystem::path& path, std::string_view content) const noexcept
    {
        try
        {
            if (!EnsureDirectory(path.parent_path()))
                return false;
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
                return false;
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            return stream.good();
        }
        catch (...) { return false; }
    }

    bool Service::AppendText(const std::filesystem::path& path, std::string_view content) const noexcept
    {
        try
        {
            if (!EnsureDirectory(path.parent_path()))
                return false;
            std::ofstream stream(path, std::ios::binary | std::ios::app);
            if (!stream)
                return false;
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            return stream.good();
        }
        catch (...) { return false; }
    }

    bool Service::ReadBinary(const std::filesystem::path& path, std::vector<std::byte>& output) const noexcept
    {
        try
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return false;
            stream.seekg(0, std::ios::end);
            const auto size = stream.tellg();
            if (size < 0)
                return false;
            stream.seekg(0, std::ios::beg);
            output.resize(static_cast<std::size_t>(size));
            if (!output.empty())
                stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
            return stream.good() || stream.eof();
        }
        catch (...) { return false; }
    }

    bool Service::WriteBinary(const std::filesystem::path& path, std::span<const std::byte> data) const noexcept
    {
        try
        {
            if (!EnsureDirectory(path.parent_path()))
                return false;
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
                return false;
            if (!data.empty())
                stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            return stream.good();
        }
        catch (...) { return false; }
    }

    bool Service::Copy(const std::filesystem::path& source, const std::filesystem::path& target, bool overwrite) const noexcept
    {
        try
        {
            if (!EnsureDirectory(target.parent_path()))
                return false;
            std::error_code ec;
            const auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
            std::filesystem::copy(source, target, options, ec);
            return !ec;
        }
        catch (...) { return false; }
    }

    bool Service::Move(const std::filesystem::path& source, const std::filesystem::path& target, bool overwrite) const noexcept
    {
        try
        {
            if (!EnsureDirectory(target.parent_path()))
                return false;
            std::error_code ec;
            if (overwrite && std::filesystem::exists(target, ec))
                std::filesystem::remove_all(target, ec);
            std::filesystem::rename(source, target, ec);
            return !ec;
        }
        catch (...) { return false; }
    }

    bool Service::Remove(const std::filesystem::path& path) const noexcept
    {
        std::error_code ec;
        return std::filesystem::remove(path, ec) && !ec;
    }

    bool Service::RemoveAll(const std::filesystem::path& path) const noexcept
    {
        std::error_code ec;
        static_cast<void>(std::filesystem::remove_all(path, ec));
        return !ec;
    }

    std::vector<std::filesystem::path> Service::ListFiles(const std::filesystem::path& directory, bool recursive) const
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!recursive)
        {
            for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
            {
                if (ec) break;
                if (entry.is_regular_file(ec) && !ec) result.emplace_back(entry.path());
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec))
            {
                if (ec) break;
                if (entry.is_regular_file(ec) && !ec) result.emplace_back(entry.path());
            }
        }
        return result;
    }

    std::vector<std::filesystem::path> Service::ListDirectories(const std::filesystem::path& directory, bool recursive) const
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!recursive)
        {
            for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
            {
                if (ec) break;
                if (entry.is_directory(ec) && !ec) result.emplace_back(entry.path());
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec))
            {
                if (ec) break;
                if (entry.is_directory(ec) && !ec) result.emplace_back(entry.path());
            }
        }
        return result;
    }

    bool Service::IsInitialized() const noexcept { return m_Initialized; }
}
