#pragma once

#include <Windows.h>
#include <atomic>

namespace TutonesV2::Core
{
    class Application final
    {
    public:
        static Application& Get() noexcept;

        bool Initialize(HMODULE module) noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] HMODULE Module() const noexcept;

    private:
        Application() = default;

        std::atomic_bool m_Running{false};
        HMODULE m_Module{};
    };
}
