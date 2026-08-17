#pragma once

#include <filesystem>

namespace Tutones::App
{
    class Application final
    {
    public:
        static Application& Get() noexcept;

        bool Initialize(const std::filesystem::path& moduleDirectory);
        void Shutdown() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;

    private:
        Application() = default;
        ~Application() = default;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        bool m_Running{};
    };
}
