#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>

namespace Tutones::UI
{
    enum class InputAction : std::uint32_t
    {
        None   = 0,
        Up     = 1u << 0,
        Down   = 1u << 1,
        Left   = 1u << 2,
        Right  = 1u << 3,
        Select = 1u << 4,
        Back   = 1u << 5,
    };

    class Input final
    {
    public:
        static Input& Get() noexcept;

        bool Initialize() noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] bool IsMenuOpen() const noexcept;
        void SetMenuOpen(bool open) noexcept;
        void ToggleMenu() noexcept;

        [[nodiscard]] std::uint32_t ConsumePendingActions() noexcept;
        [[nodiscard]] bool Consume(InputAction action) noexcept;

        static bool HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

    private:
        Input() = default;
        ~Input() = default;
        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;

        static bool IsRepeat(LPARAM lParam) noexcept;
        static bool IsMenuKey(WPARAM key) noexcept;
        void Queue(InputAction action, const char* actionName) noexcept;

        std::atomic<bool> m_Initialized{false};
        std::atomic<bool> m_MenuOpen{false};
        std::atomic<std::uint32_t> m_PendingActions{0};
    };

    [[nodiscard]] constexpr std::uint32_t ToMask(InputAction action) noexcept
    {
        return static_cast<std::uint32_t>(action);
    }
}
