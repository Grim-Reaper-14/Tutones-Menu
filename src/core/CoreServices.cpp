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

        const auto configPath = fileSystem.RootPath(FileSystem::Root::Config) / "tutones.cfg";
        const bool loadedConfig = Config::Service::Get().Load(configPath);
        const auto& settings = Config::Service::Get().Current();

        Logging::LoggerConfig loggerConfig;
        loggerConfig.minimumLevel = settings.minimumLogLevel;
        loggerConfig.consoleEnabled = settings.consoleLogging;
        loggerConfig.debuggerEnabled = settings.debuggerLogging;
        loggerConfig.fileEnabled = settings.fileLogging;
        loggerConfig.filePath = fileSystem.RootPath(FileSystem::Root::Logs) / "tutones.log";
        loggerConfig.maxFileBytes = settings.logMaxBytes;
        loggerConfig.maxFiles = settings.logRetentionFiles;

        if (!Logging::Logger::Get().Initialize(loggerConfig))
        {
            fileSystem.Shutdown();
            return false;
        }

        TUTONES_LOG_INFO("core", "Tutones Menu core services starting");
        TUTONES_LOG_INFO("filesystem", "Filesystem service ready");
        TUTONES_LOG_INFO("config", loadedConfig ? "Configuration loaded" : "Using default configuration");

        if (!loadedConfig)
        {
            if (Config::Service::Get().Save(configPath))
                TUTONES_LOG_INFO("config", "Default configuration created");
            else
                TUTONES_LOG_WARN("config", "Failed to create default configuration");
        }

        m_Initialized = true;
        TUTONES_LOG_INFO("core", "Core services initialized");
        return true;
    }

    void Services::Shutdown() noexcept
    {
        if (!m_Initialized)
            return;

        TUTONES_LOG_INFO("core", "Core services shutting down");
        Logging::Logger::Get().Flush();
        Config::Service::Get().Reset();
        Logging::Logger::Get().Shutdown();
        FileSystem::Service::Get().Shutdown();
        m_ModuleDirectory.clear();
        m_Initialized = false;
    }

    bool Services::IsInitialized() const noexcept { return m_Initialized; }
    const std::filesystem::path& Services::ModuleDirectory() const noexcept { return m_ModuleDirectory; }
}
