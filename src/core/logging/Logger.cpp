#include "Logger.hpp"

#include <Windows.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Tutones::Core::Logging
{
    namespace
    {
        std::uint64_t CurrentThreadId() noexcept
        {
            return static_cast<std::uint64_t>(::GetCurrentThreadId());
        }

        std::uint64_t UnixMilliseconds() noexcept
        {
            const auto now = std::chrono::system_clock::now();
            return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
        }

        std::string FormatTimestamp(std::uint64_t milliseconds)
        {
            const auto time = static_cast<std::time_t>(milliseconds / 1000);
            std::tm local{};
            localtime_s(&local, &time);

            std::ostringstream stream;
            stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
                   << '.' << std::setw(3) << std::setfill('0') << (milliseconds % 1000);
            return stream.str();
        }

        std::string FormatRecord(const LogRecord& record)
        {
            std::ostringstream stream;
            stream << '[' << FormatTimestamp(record.unixMilliseconds) << "] [#" << record.sequence
                   << "] [T" << record.threadId << "] [" << ToString(record.level)
                   << "] [" << record.category << "] " << record.message
                   << " (" << record.file << ':' << record.line << " " << record.function << ")";
            return stream.str();
        }

        class ConsoleSink final : public ILogSink
        {
        public:
            void Write(const LogRecord& record) noexcept override
            {
                try
                {
                    std::cout << FormatRecord(record) << '\n';
                    if (record.level >= LogLevel::Warning)
                        std::cout.flush();
                }
                catch (...) {}
            }

            void Flush() noexcept override
            {
                try { std::cout.flush(); } catch (...) {}
            }
        };

        class DebuggerSink final : public ILogSink
        {
        public:
            void Write(const LogRecord& record) noexcept override
            {
                try
                {
                    const auto line = FormatRecord(record) + '\n';
                    ::OutputDebugStringA(line.c_str());
                }
                catch (...) {}
            }

            void Flush() noexcept override {}
        };

        class FileSink final : public ILogSink
        {
        public:
            explicit FileSink(LoggerConfig config)
                : m_Config(std::move(config))
            {
                Open();
            }

            void Write(const LogRecord& record) noexcept override
            {
                try
                {
                    if (!m_Stream.is_open())
                        Open();
                    if (!m_Stream.is_open())
                        return;

                    m_Stream << FormatRecord(record) << '\n';

                    if (record.level >= LogLevel::Error || (m_Config.flushOnWarningOrHigher && record.level >= LogLevel::Warning))
                        m_Stream.flush();

                    if (m_Config.maxFileBytes > 0)
                    {
                        std::error_code ec;
                        const auto currentSize = std::filesystem::exists(m_Config.filePath, ec)
                            ? std::filesystem::file_size(m_Config.filePath, ec)
                            : 0;
                        if (!ec && currentSize >= m_Config.maxFileBytes)
                            Rotate();
                    }
                }
                catch (...) {}
            }

            void Flush() noexcept override
            {
                try { if (m_Stream.is_open()) m_Stream.flush(); } catch (...) {}
            }

        private:
            void Open() noexcept
            {
                try
                {
                    if (m_Config.filePath.empty())
                        return;
                    if (!m_Config.filePath.parent_path().empty())
                        std::filesystem::create_directories(m_Config.filePath.parent_path());
                    m_Stream.open(m_Config.filePath, std::ios::out | std::ios::app);
                }
                catch (...) {}
            }

            void Rotate() noexcept
            {
                try
                {
                    m_Stream.flush();
                    m_Stream.close();

                    const auto base = m_Config.filePath;
                    const auto maxFiles = m_Config.maxFiles > 0 ? m_Config.maxFiles : 1;
                    for (std::size_t i = maxFiles; i > 0; --i)
                    {
                        const auto source = i == 1 ? base : std::filesystem::path(base.string() + '.' + std::to_string(i - 1));
                        const auto target = std::filesystem::path(base.string() + '.' + std::to_string(i));
                        std::error_code ec;
                        std::filesystem::remove(target, ec);
                        if (std::filesystem::exists(source, ec))
                            std::filesystem::rename(source, target, ec);
                    }
                    Open();
                }
                catch (...) {}
            }

            LoggerConfig m_Config;
            std::ofstream m_Stream;
        };
    }

    Logger& Logger::Get() noexcept
    {
        static Logger instance;
        return instance;
    }

    bool Logger::Initialize(const LoggerConfig& config)
    {
        std::scoped_lock lock(m_Mutex);
        m_Config = config;
        m_Sinks.clear();

        if (config.consoleEnabled)
            m_Sinks.emplace_back(std::make_shared<ConsoleSink>());
        if (config.debuggerEnabled)
            m_Sinks.emplace_back(std::make_shared<DebuggerSink>());
        if (config.fileEnabled && !config.filePath.empty())
            m_Sinks.emplace_back(std::make_shared<FileSink>(config));

        m_NextSequence = 1;
        m_Initialized = true;
        return true;
    }

    void Logger::Shutdown() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        for (const auto& sink : m_Sinks)
            if (sink)
                sink->Flush();
        m_Sinks.clear();
        m_Initialized = false;
    }

    void Logger::SetMinimumLevel(LogLevel level) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Config.minimumLevel = level;
    }

    LogLevel Logger::MinimumLevel() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Config.minimumLevel;
    }

    void Logger::AddSink(std::shared_ptr<ILogSink> sink)
    {
        if (!sink)
            return;
        std::scoped_lock lock(m_Mutex);
        m_Sinks.emplace_back(std::move(sink));
    }

    void Logger::ClearSinks() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Sinks.clear();
    }

    void Logger::Write(LogLevel level, std::string_view category, std::string_view message, const std::source_location& location) noexcept
    {
        try
        {
            std::scoped_lock lock(m_Mutex);
            if (!m_Initialized || level < m_Config.minimumLevel || level == LogLevel::Off)
                return;

            LogRecord record;
            record.level = level;
            record.sequence = m_NextSequence++;
            record.threadId = CurrentThreadId();
            record.unixMilliseconds = UnixMilliseconds();
            record.category = category;
            record.message = message;
            record.file = location.file_name();
            record.function = location.function_name();
            record.line = location.line();

            for (const auto& sink : m_Sinks)
                if (sink)
                    sink->Write(record);

            if (level >= LogLevel::Critical || (m_Config.flushOnWarningOrHigher && level >= LogLevel::Warning))
                for (const auto& sink : m_Sinks)
                    if (sink)
                        sink->Flush();
        }
        catch (...) {}
    }

    void Logger::Flush() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        for (const auto& sink : m_Sinks)
            if (sink)
                sink->Flush();
    }

    bool Logger::IsInitialized() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Initialized;
    }

    void Category::Trace(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Trace, m_Name, message, location); }
    void Category::Debug(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Debug, m_Name, message, location); }
    void Category::Info(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Info, m_Name, message, location); }
    void Category::Warning(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Warning, m_Name, message, location); }
    void Category::Error(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Error, m_Name, message, location); }
    void Category::Critical(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Critical, m_Name, message, location); }
}
