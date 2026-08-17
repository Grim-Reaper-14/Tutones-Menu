#pragma once

#include "LogLevel.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Tutones::Core::Logging
{
    struct LogRecord final
    {
        LogLevel level{LogLevel::Info};
        std::uint64_t sequence{};
        std::uint64_t threadId{};
        std::uint64_t unixMilliseconds{};
        std::string category;
        std::string message;
        std::string file;
        std::string function;
        std::uint_least32_t line{};

        [[nodiscard]] bool IsError() const noexcept
        {
            return level == LogLevel::Error || level == LogLevel::Critical;
        }
    };
}
