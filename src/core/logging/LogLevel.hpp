#pragma once

#include <cstdint>
#include <string_view>

namespace Tutones::Core::Logging
{
    enum class LogLevel : std::uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Critical,
        Off,
    };

    [[nodiscard]] constexpr std::string_view ToString(LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off: return "OFF";
        }
        return "UNKNOWN";
    }
}
