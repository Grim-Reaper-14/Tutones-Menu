#pragma once

#include <mutex>
#include <string_view>

namespace TutonesV2::Core::Logging
{
    class Logger final
    {
    public:
        static Logger& Get() noexcept;

        bool Initialize() noexcept;
        void Shutdown() noexcept;

        void Info(std::string_view channel, std::string_view message) noexcept;
        void Error(std::string_view channel, std::string_view message) noexcept;

    private:
        Logger() = default;
        void Write(std::string_view level, std::string_view channel, std::string_view message) noexcept;

        std::mutex m_Mutex;
        bool m_Initialized{};
    };
}
