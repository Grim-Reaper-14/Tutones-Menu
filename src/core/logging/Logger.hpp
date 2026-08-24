#pragma once

#include "LogLevel.hpp"
#include "LogRecord.hpp"
#include "LogSink.hpp"

#include <Windows.h>
#ifdef SetConsoleTitle
#undef SetConsoleTitle
#endif

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
        bool consoleAllocateWindow{true};
        bool consoleUseColors{true};
        bool consoleShowSourceOnWarning{true};
        bool consoleDisableQuickEdit{true};
        bool consoleDisableCloseButton{true};
        std::wstring consoleTitle{L"Tutones Menu | Enhanced Runtime Diagnostics"};
        std::uint16_t consoleBufferWidth{180};
        std::uint16_t consoleBufferLines{5000};

        bool debuggerEnabled{true};
        bool fileEnabled{true};
        bool flushOnWarningOrHigher{true};
        std::filesystem::path filePath{};
        std::uintmax_t maxFileBytes{8 * 1024 * 1024};
        std::size_t maxFiles{5};
    };

    struct LoggerStats final
    {
        bool initialized{};
        bool consoleAvailable{};
        bool consoleOwned{};
        std::uint64_t startedUnixMilliseconds{};
        std::uint64_t total{};
        std::uint64_t filtered{};
        std::uint64_t trace{};
        std::uint64_t debug{};
        std::uint64_t info{};
        std::uint64_t warning{};
        std::uint64_t error{};
        std::uint64_t critical{};
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
        [[nodiscard]] bool ConsoleAvailable() const noexcept;
        [[nodiscard]] LoggerStats SnapshotStats() const noexcept;

        bool SetConsoleVisible(bool visible) noexcept;
        bool SetConsoleTitle(std::wstring_view title) noexcept;

    private:
        Logger() = default;
        ~Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void CountRecord(LogLevel level) noexcept;
        void ReleaseOwnedConsole() noexcept;

        mutable std::mutex m_Mutex;
        LoggerConfig m_Config{};
        LoggerStats m_Stats{};
        std::vector<std::shared_ptr<ILogSink>> m_Sinks;
        bool m_Initialized{};
        bool m_ConsoleAvailable{};
        bool m_OwnsConsole{};
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
