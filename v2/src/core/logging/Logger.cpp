#include "Logger.hpp"

#include <Windows.h>
#include <cstdio>
#include <string>

namespace TutonesV2::Core::Logging
{
    Logger& Logger::Get() noexcept
    {
        static Logger instance;
        return instance;
    }

    bool Logger::Initialize() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Initialized = true;
        return true;
    }

    void Logger::Shutdown() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Initialized = false;
    }

    void Logger::Info(std::string_view channel, std::string_view message) noexcept
    {
        Write("INFO", channel, message);
    }

    void Logger::Error(std::string_view channel, std::string_view message) noexcept
    {
        Write("ERROR", channel, message);
    }

    void Logger::Write(std::string_view level, std::string_view channel, std::string_view message) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Initialized)
            return;

        std::string line;
        line.reserve(level.size() + channel.size() + message.size() + 16);
        line += "[TutonesV2][";
        line.append(level);
        line += "][";
        line.append(channel);
        line += "] ";
        line.append(message);
        line += '\n';

        ::OutputDebugStringA(line.c_str());
        std::fputs(line.c_str(), stdout);
    }
}
