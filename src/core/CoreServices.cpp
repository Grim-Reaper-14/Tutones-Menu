#include "CoreServices.hpp"

#include "config/Config.hpp"
#include "filesystem/FileSystem.hpp"
#include "logging/Logger.hpp"

#include <filesystem>

namespace Tutones::Core
{
    Services& Services::Get() noexcept
    {
        static Services instance;
        return instance;
    }

    bool Services::Initialize(const std::filesystem::path& moduleDirectory)
    {
        if (m_Initialized)
            return true;
        if (moduleDirectory.empty())
            return false;

        m_ModuleDirectory = std::filesystem::weakly_canonical(moduleDirectory);

        auto& fileSystem = FileSystem::Service::Get();
        if (!fileSystem.Initialize(m_ModuleDirectory))
            return false;

        Logging::LoggerConfig loggerConfig;
        loggerConfig.minimumLevel = Config::Service::Get().Current().minimumLogLevel;
        loggerConfig.consoleEnabled = true;
        loggerConfig.debuggerEnabled = true;
        loggerConfig.fileEnabled = true;
        loggerConfig.filePath = fileSystem.RootPath(FileSystem::Root::Logs) / "tutones.log";
        loggerConfig.maxFileBytes = 8 * 1024 * 1024;
        loggerConfig.maxFiles = 5;

        if (!Logging::Logger::Get().Initialize(loggerConfig))
        {
            fileSystem.Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("core", "Tutones Menu core services starting");

        const auto configPath = fileSystem.RootPath(FileSystem::Root::Config) / "tutones.cfg";
        static_cast<void>(Config::Service::Get().Load(configPath));

        const auto& settings = Config::Service::Get().Current();
        Logging::Logger::Get().SetMinimumLevel(settings.minimumLogLevel);

        if (!Config::Service::Get().IsLoaded())
        {
            if (Config::Service::Get().Save(configPath))
                TUTONES_LOG_INFO("config", "Default configuration created");
            else
                TUTONES_LOG_WARN("config", "Failed to create default configuration");
        }

        TUTONES_LOG_INFO("core", "Core services initialized");
        m_Initialized = true;
        return true;
    }

    void Services::Shutdown() noexcept
    {
        if (!m_Initialized)
            return;

        TUTONES_LOG_INFO("core", "Core services shutting down");
        Logging::Logger::Get().Flush();
        Config::Service::Get().Reset();
        FileSystem::Service::Get().Shutdown();
        Logging::Logger::Get().Shutdown();
        m_ModuleDirectory.clear();
        m_Initialized = false;
    }

    bool Services::IsInitialized() const noexcept { return m_Initialized; }
    const std::filesystem::path& Services::ModuleDirectory() const noexcept { return m_ModuleDirectory; }
}
