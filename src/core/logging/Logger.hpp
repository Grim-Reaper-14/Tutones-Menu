#pragma once

#include "LogLevel.hpp"
#include "LogRecord.hpp"
#include "LogSink.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::Core::Logging
{
    struct LoggerConfig final
    {
        LogLevel minimumLevel{LogLevel::Trace};
        bool consoleEnabled{true};
        bool debuggerEnabled{true};
        bool fileEnabled{true};
        bool flushOnWarningOrHigher{true};
        std::filesystem::path filePath{};
        std::uintmax_t maxFileBytes{8 * 1024 * 1024};
        std::size_t maxFiles{5};
    };

    class Logger final
    {
    public:
        static Logger& Get() noexcept;

        bool Initialize(const LoggerConfig& config);
        void Shutdown() noexcept;

        void SetMinimumLevel(LogLevel level) noexcept;
        [[nodiscard]] LogLevel MinimumLevel() const noexcept;

        void AddSink(std::shared_ptr<ILogSink> sink);
        void ClearSinks() noexcept;

        void Write(
            LogLevel level,
            std::string_view category,
            std::string_view message,
            const std::source_location& location = std::source_location::current()) noexcept;

        void Flush() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;

    private:
        Logger() = default;
        ~Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        mutable std::mutex m_Mutex;
        LoggerConfig m_Config{};
        std::vector<std::shared_ptr<ILogSink>> m_Sinks;
        bool m_Initialized{};
        std::uint64_t m_NextSequence{1};
    };

    class Category final
    {
    public:
        explicit Category(std::string_view name) : m_Name(name) {}

        void Trace(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;
        void Debug(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;
        void Info(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;
        void Warning(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;
        void Error(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;
        void Critical(std::string_view message, const std::source_location& location = std::source_location::current()) const noexcept;

    private:
        std::string m_Name;
    };

    inline Category Log(std::string_view category)
    {
        return Category(category);
    }
}

#define TUTONES_LOG_TRACE(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Trace, (category), (message), std::source_location::current())
#define TUTONES_LOG_DEBUG(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Debug, (category), (message), std::source_location::current())
#define TUTONES_LOG_INFO(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Info, (category), (message), std::source_location::current())
#define TUTONES_LOG_WARN(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Warning, (category), (message), std::source_location::current())
#define TUTONES_LOG_ERROR(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Error, (category), (message), std::source_location::current())
#define TUTONES_LOG_CRITICAL(category, message) ::Tutones::Core::Logging::Logger::Get().Write(::Tutones::Core::Logging::LogLevel::Critical, (category), (message), std::source_location::current())
