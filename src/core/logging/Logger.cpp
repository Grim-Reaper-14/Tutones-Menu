#include "Logger.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Tutones::Core::Logging
{
    namespace
    {
        constexpr std::size_t TimeWidth = 12;
        constexpr std::size_t SequenceWidth = 7;
        constexpr std::size_t ThreadWidth = 8;
        constexpr std::size_t LevelWidth = 9;
        constexpr std::size_t CategoryWidth = 20;
        constexpr std::string_view RowSeparator = "------------------------------------------------------------------------------------------------------------------------\n";

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
            stream << std::put_time(&local, "%H:%M:%S")
                   << '.' << std::setw(3) << std::setfill('0') << (milliseconds % 1000);
            return stream.str();
        }

        std::string PadLeft(std::string value, std::size_t width)
        {
            if (value.size() < width)
                value.insert(value.begin(), width - value.size(), ' ');
            return value;
        }

        std::string PadRight(std::string value, std::size_t width)
        {
            if (value.size() > width)
                value.resize(width);
            else if (value.size() < width)
                value.append(width - value.size(), ' ');
            return value;
        }

        std::string ShortFile(std::string_view file)
        {
            const auto slash = file.find_last_of("/\\");
            return slash == std::string_view::npos ? std::string(file) : std::string(file.substr(slash + 1));
        }

        std::string FormatConsoleRecord(const LogRecord& record, bool includeSource)
        {
            std::ostringstream stream;
            stream << PadRight(FormatTimestamp(record.unixMilliseconds), TimeWidth) << " | "
                   << PadLeft("#" + std::to_string(record.sequence), SequenceWidth) << " | "
                   << PadLeft("T" + std::to_string(record.threadId), ThreadWidth) << " | "
                   << PadRight(std::string(ToString(record.level)), LevelWidth) << " | "
                   << PadRight(std::string(record.category), CategoryWidth) << " | "
                   << record.message;

            if (includeSource)
                stream << "  [" << ShortFile(record.file) << ':' << record.line << ']';

            return stream.str();
        }

        std::string FormatDetailedRecord(const LogRecord& record)
        {
            std::ostringstream stream;
            stream << '[' << FormatTimestamp(record.unixMilliseconds) << "] [#" << record.sequence
                   << "] [T" << record.threadId << "] [" << ToString(record.level)
                   << "] [" << record.category << "] " << record.message
                   << " (" << record.file << ':' << record.line << " " << record.function << ')';
            return stream.str();
        }

        WORD ConsoleColorForLevel(LogLevel level) noexcept
        {
            switch (level)
            {
            case LogLevel::Trace: return FOREGROUND_INTENSITY;
            case LogLevel::Debug: return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case LogLevel::Info: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            case LogLevel::Warning: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case LogLevel::Error: return FOREGROUND_RED | FOREGROUND_INTENSITY;
            case LogLevel::Critical: return BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case LogLevel::Off: break;
            }
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }

        bool RedirectStandardStreamsToConsole() noexcept
        {
            FILE* stream{};
            const bool outOk = freopen_s(&stream, "CONOUT$", "w", stdout) == 0;
            const bool errOk = freopen_s(&stream, "CONOUT$", "w", stderr) == 0;
            const bool inOk = freopen_s(&stream, "CONIN$", "r", stdin) == 0;

            std::ios::sync_with_stdio(true);
            std::cout.clear();
            std::cerr.clear();
            std::clog.clear();
            return outOk && errOk && inOk;
        }

        bool ConfigureConsole(const LoggerConfig& config) noexcept
        {
            const auto window = ::GetConsoleWindow();
            const auto output = ::GetStdHandle(STD_OUTPUT_HANDLE);
            const auto input = ::GetStdHandle(STD_INPUT_HANDLE);

            if (!window || output == INVALID_HANDLE_VALUE || output == nullptr)
                return false;

            ::SetConsoleOutputCP(CP_UTF8);
            ::SetConsoleCP(CP_UTF8);
            ::SetConsoleTitleW(config.consoleTitle.c_str());

            if (config.consoleBufferWidth > 0 && config.consoleBufferLines > 0)
            {
                CONSOLE_SCREEN_BUFFER_INFO info{};
                if (::GetConsoleScreenBufferInfo(output, &info))
                {
                    COORD size{};
                    size.X = static_cast<SHORT>(config.consoleBufferWidth);
                    size.Y = static_cast<SHORT>(config.consoleBufferLines);
                    ::SetConsoleScreenBufferSize(output, size);
                }
            }

            if (config.consoleDisableQuickEdit && input != INVALID_HANDLE_VALUE && input != nullptr)
            {
                DWORD mode{};
                if (::GetConsoleMode(input, &mode))
                {
                    mode |= ENABLE_EXTENDED_FLAGS;
                    mode &= ~ENABLE_QUICK_EDIT_MODE;
                    ::SetConsoleMode(input, mode);
                }
            }

            if (config.consoleDisableCloseButton)
            {
                if (auto* menu = ::GetSystemMenu(window, FALSE))
                {
                    ::EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
                    ::DrawMenuBar(window);
                }
            }

            return true;
        }

        void PrintBanner(const LoggerConfig& config)
        {
            std::cout
                << '\n'
                << "========================================================================================================================\n"
                << "                                              TUTONES MENU\n"
                << "                                      ENHANCED RUNTIME DIAGNOSTICS\n"
                << "========================================================================================================================\n"
                << " Console: ACTIVE | UTF-8: ON | Colors: " << (config.consoleUseColors ? "ON" : "OFF")
                << " | File log: " << (config.fileEnabled ? "ON" : "OFF")
                << " | Debugger: " << (config.debuggerEnabled ? "ON" : "OFF") << '\n';

            if (config.fileEnabled && !config.filePath.empty())
                std::cout << " Log file: " << config.filePath.string() << '\n';

            std::cout
                << "------------------------------------------------------------------------------------------------------------------------\n"
                << PadRight("TIME", TimeWidth) << " | "
                << PadRight("SEQ", SequenceWidth) << " | "
                << PadRight("THREAD", ThreadWidth) << " | "
                << PadRight("LEVEL", LevelWidth) << " | "
                << PadRight("CATEGORY", CategoryWidth) << " | MESSAGE\n"
                << RowSeparator;
        }

        class ConsoleSink final : public ILogSink
        {
        public:
            explicit ConsoleSink(LoggerConfig config)
                : m_Config(std::move(config)), m_Output(::GetStdHandle(STD_OUTPUT_HANDLE))
            {
                if (m_Output != INVALID_HANDLE_VALUE && m_Output != nullptr)
                {
                    CONSOLE_SCREEN_BUFFER_INFO info{};
                    if (::GetConsoleScreenBufferInfo(m_Output, &info))
                        m_DefaultAttributes = info.wAttributes;
                }
            }

            void Write(const LogRecord& record) noexcept override
            {
                try
                {
                    const bool showSource = m_Config.consoleShowSourceOnWarning && record.level >= LogLevel::Warning;

                    if (m_Config.consoleUseColors && m_Output != INVALID_HANDLE_VALUE && m_Output != nullptr)
                        ::SetConsoleTextAttribute(m_Output, ConsoleColorForLevel(record.level));

                    std::cout << FormatConsoleRecord(record, showSource) << '\n';

                    if (m_Config.consoleUseColors && m_Output != INVALID_HANDLE_VALUE && m_Output != nullptr)
                        ::SetConsoleTextAttribute(m_Output, m_DefaultAttributes);

                    if (record.level >= LogLevel::Warning)
                        std::cout.flush();
                }
                catch (...) {}
            }

            void Flush() noexcept override
            {
                try { std::cout.flush(); } catch (...) {}
            }

        private:
            LoggerConfig m_Config;
            HANDLE m_Output{INVALID_HANDLE_VALUE};
            WORD m_DefaultAttributes{FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE};
        };

        class DebuggerSink final : public ILogSink
        {
        public:
            void Write(const LogRecord& record) noexcept override
            {
                try
                {
                    const auto line = FormatDetailedRecord(record) + '\n';
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

                    m_Stream << FormatDetailedRecord(record) << '\n';

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

        for (const auto& sink : m_Sinks)
            if (sink)
                sink->Flush();

        m_Sinks.clear();
        ReleaseOwnedConsole();

        m_Config = config;
        m_NextSequence = 1;
        m_Stats = {};
        m_Stats.startedUnixMilliseconds = UnixMilliseconds();

        if (config.consoleEnabled)
        {
            if (::GetConsoleWindow())
            {
                m_ConsoleAvailable = RedirectStandardStreamsToConsole() && ConfigureConsole(config);
            }
            else if (config.consoleAllocateWindow && ::AllocConsole())
            {
                m_OwnsConsole = true;
                m_ConsoleAvailable = RedirectStandardStreamsToConsole() && ConfigureConsole(config);
            }

            if (m_ConsoleAvailable)
            {
                m_Sinks.emplace_back(std::make_shared<ConsoleSink>(config));
                PrintBanner(config);
            }
            else
            {
                ::OutputDebugStringA("[Tutones] Console logging requested but the Win32 console could not be initialized.\n");
            }
        }

        if (config.debuggerEnabled)
            m_Sinks.emplace_back(std::make_shared<DebuggerSink>());
        if (config.fileEnabled && !config.filePath.empty())
            m_Sinks.emplace_back(std::make_shared<FileSink>(config));

        m_Initialized = true;
        m_Stats.initialized = true;
        m_Stats.consoleAvailable = m_ConsoleAvailable;
        m_Stats.consoleOwned = m_OwnsConsole;
        return true;
    }

    void Logger::Shutdown() noexcept
    {
        std::scoped_lock lock(m_Mutex);

        if (!m_Initialized)
        {
            ReleaseOwnedConsole();
            return;
        }

        for (const auto& sink : m_Sinks)
            if (sink)
                sink->Flush();

        if (m_ConsoleAvailable)
        {
            std::cout
                << "------------------------------------------------------------------------------------------------------------------------\n"
                << " Logger shutdown | total=" << m_Stats.total
                << " warn=" << m_Stats.warning
                << " error=" << m_Stats.error
                << " critical=" << m_Stats.critical
                << " filtered=" << m_Stats.filtered << '\n';
            std::cout.flush();
        }

        m_Sinks.clear();
        m_Initialized = false;
        m_Stats.initialized = false;
        ReleaseOwnedConsole();
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
            if (!m_Initialized || level == LogLevel::Off)
                return;

            if (level < m_Config.minimumLevel)
            {
                ++m_Stats.filtered;
                return;
            }

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

            CountRecord(level);

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

    bool Logger::ConsoleAvailable() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_ConsoleAvailable;
    }

    LoggerStats Logger::SnapshotStats() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Stats;
    }

    bool Logger::SetConsoleVisible(bool visible) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const auto window = ::GetConsoleWindow();
        if (!window)
            return false;
        return ::ShowWindow(window, visible ? SW_SHOW : SW_HIDE) != FALSE;
    }

    bool Logger::SetConsoleTitle(std::wstring_view title) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_ConsoleAvailable || title.empty())
            return false;
        const std::wstring owned(title);
        return ::SetConsoleTitleW(owned.c_str()) != FALSE;
    }

    void Logger::CountRecord(LogLevel level) noexcept
    {
        ++m_Stats.total;
        switch (level)
        {
        case LogLevel::Trace: ++m_Stats.trace; break;
        case LogLevel::Debug: ++m_Stats.debug; break;
        case LogLevel::Info: ++m_Stats.info; break;
        case LogLevel::Warning: ++m_Stats.warning; break;
        case LogLevel::Error: ++m_Stats.error; break;
        case LogLevel::Critical: ++m_Stats.critical; break;
        case LogLevel::Off: break;
        }
    }

    void Logger::ReleaseOwnedConsole() noexcept
    {
        m_ConsoleAvailable = false;

        if (m_OwnsConsole)
        {
            ::FreeConsole();
            m_OwnsConsole = false;
        }

        m_Stats.consoleAvailable = false;
        m_Stats.consoleOwned = false;
    }

    void Category::Trace(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Trace, m_Name, message, location); }
    void Category::Debug(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Debug, m_Name, message, location); }
    void Category::Info(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Info, m_Name, message, location); }
    void Category::Warning(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Warning, m_Name, message, location); }
    void Category::Error(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Error, m_Name, message, location); }
    void Category::Critical(std::string_view message, const std::source_location& location) const noexcept { Logger::Get().Write(LogLevel::Critical, m_Name, message, location); }
}
