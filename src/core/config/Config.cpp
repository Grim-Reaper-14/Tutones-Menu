#include "Config.hpp"

#include "../filesystem/FileSystem.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace Tutones::Core::Config
{
    namespace
    {
        std::string Trim(std::string value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        bool ParseBool(std::string_view value, bool fallback)
        {
            if (value == "1" || value == "true" || value == "TRUE" || value == "on")
                return true;
            if (value == "0" || value == "false" || value == "FALSE" || value == "off")
                return false;
            return fallback;
        }

        std::uint64_t ParseUnsigned(std::string_view value, std::uint64_t fallback)
        {
            try
            {
                std::size_t consumed{};
                const auto parsed = std::stoull(std::string(value), &consumed, 10);
                return consumed == value.size() ? parsed : fallback;
            }
            catch (...) { return fallback; }
        }

        std::string LogLevelToString(Logging::LogLevel level)
        {
            return std::string(Logging::ToString(level));
        }

        Logging::LogLevel ParseLogLevel(std::string_view value, Logging::LogLevel fallback)
        {
            if (value == "TRACE") return Logging::LogLevel::Trace;
            if (value == "DEBUG") return Logging::LogLevel::Debug;
            if (value == "INFO") return Logging::LogLevel::Info;
            if (value == "WARN" || value == "WARNING") return Logging::LogLevel::Warning;
            if (value == "ERROR") return Logging::LogLevel::Error;
            if (value == "CRITICAL") return Logging::LogLevel::Critical;
            if (value == "OFF") return Logging::LogLevel::Off;
            return fallback;
        }
    }

    Service& Service::Get() noexcept
    {
        static Service instance;
        return instance;
    }

    bool Service::Load(const std::filesystem::path& path)
    {
        Reset();

        std::ifstream stream(path);
        if (!stream)
        {
            Logging::Logger::Get().Write(Logging::LogLevel::Debug, "config", "No config file found; using defaults");
            return false;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            line = Trim(std::move(line));
            if (line.empty() || line.front() == '#' || line.front() == ';')
                continue;

            const auto separator = line.find('=');
            if (separator == std::string::npos)
                continue;

            const auto key = Trim(line.substr(0, separator));
            const auto value = Trim(line.substr(separator + 1));

            if (key == "menu.name") m_Settings.menuName = value;
            else if (key == "config.version") m_Settings.configVersion = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.configVersion));
            else if (key == "menu.enabled") m_Settings.menuEnabled = ParseBool(value, m_Settings.menuEnabled);
            else if (key == "logging.debug") m_Settings.debugLogging = ParseBool(value, m_Settings.debugLogging);
            else if (key == "logging.minimum_level") m_Settings.minimumLogLevel = ParseLogLevel(value, m_Settings.minimumLogLevel);
            else if (key == "logging.console") m_Settings.consoleLogging = ParseBool(value, m_Settings.consoleLogging);
            else if (key == "logging.debugger") m_Settings.debuggerLogging = ParseBool(value, m_Settings.debuggerLogging);
            else if (key == "logging.file") m_Settings.fileLogging = ParseBool(value, m_Settings.fileLogging);
            else if (key == "logging.retention") m_Settings.logRetentionFiles = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.logRetentionFiles));
            else if (key == "logging.max_bytes") m_Settings.logMaxBytes = ParseUnsigned(value, m_Settings.logMaxBytes);
            else if (key == "ui.width") m_Settings.uiWidth = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.uiWidth));
            else if (key == "ui.height") m_Settings.uiHeight = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.uiHeight));
            else if (key == "ui.accent_r") m_Settings.accentR = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.accentR));
            else if (key == "ui.accent_g") m_Settings.accentG = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.accentG));
            else if (key == "ui.accent_b") m_Settings.accentB = static_cast<std::uint32_t>(ParseUnsigned(value, m_Settings.accentB));
        }

        m_Loaded = stream.good() || stream.eof();
        Logging::Logger::Get().Write(Logging::LogLevel::Info, "config", m_Loaded ? "Configuration loaded" : "Configuration load finished with errors");
        return m_Loaded;
    }

    bool Service::Save(const std::filesystem::path& path) const
    {
        std::error_code ec;
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
            return false;

        std::ofstream stream(path, std::ios::trunc);
        if (!stream)
            return false;

        stream << "# Tutones Menu configuration\n"
               << "config.version=" << m_Settings.configVersion << '\n'
               << "menu.name=" << m_Settings.menuName << '\n'
               << "menu.enabled=" << (m_Settings.menuEnabled ? "true" : "false") << '\n'
               << "logging.debug=" << (m_Settings.debugLogging ? "true" : "false") << '\n'
               << "logging.minimum_level=" << LogLevelToString(m_Settings.minimumLogLevel) << '\n'
               << "logging.console=" << (m_Settings.consoleLogging ? "true" : "false") << '\n'
               << "logging.debugger=" << (m_Settings.debuggerLogging ? "true" : "false") << '\n'
               << "logging.file=" << (m_Settings.fileLogging ? "true" : "false") << '\n'
               << "logging.retention=" << m_Settings.logRetentionFiles << '\n'
               << "logging.max_bytes=" << m_Settings.logMaxBytes << '\n'
               << "ui.width=" << m_Settings.uiWidth << '\n'
               << "ui.height=" << m_Settings.uiHeight << '\n'
               << "ui.accent_r=" << m_Settings.accentR << '\n'
               << "ui.accent_g=" << m_Settings.accentG << '\n'
               << "ui.accent_b=" << m_Settings.accentB << '\n';

        return stream.good();
    }

    void Service::Reset() noexcept
    {
        m_Settings = Settings{};
        m_Loaded = false;
    }

    const Settings& Service::Current() const noexcept { return m_Settings; }
    Settings& Service::Current() noexcept { return m_Settings; }
    bool Service::IsLoaded() const noexcept { return m_Loaded; }
}
