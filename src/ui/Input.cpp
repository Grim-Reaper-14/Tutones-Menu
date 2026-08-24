#include "Input.hpp"

#include "../core/logging/Logger.hpp"
#include "../hooking/Win32Hook.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace Tutones::UI
{
    namespace
    {
        bool IsMouseMessage(UINT message) noexcept
        {
            switch (message)
            {
            case WM_MOUSEMOVE:
            case WM_MOUSELEAVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_SETCURSOR:
                return true;
            default:
                return false;
            }
        }

        bool IsKeyboardMessage(UINT message) noexcept
        {
            switch (message)
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
            case WM_DEADCHAR:
            case WM_SYSCHAR:
            case WM_SYSDEADCHAR:
                return true;
            default:
                return false;
            }
        }

        bool IsKeyDownMessage(UINT message) noexcept
        {
            return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        }

        bool IsKeyUpMessage(UINT message) noexcept
        {
            return message == WM_KEYUP || message == WM_SYSKEYUP;
        }

        void CenterCursorInGameWindow() noexcept
        {
            const auto window = Hooking::Win32Hook::Get().Window();
            if (!window || !::IsWindow(window))
                return;

            RECT client{};
            if (!::GetClientRect(window, &client))
                return;

            POINT center{
                (client.left + client.right) / 2,
                (client.top + client.bottom) / 2,
            };

            if (!::ClientToScreen(window, &center))
                return;

            ::SetCursorPos(center.x, center.y);
            TUTONES_LOG_DEBUG("input.mouse", "Cursor centered on the Tutones menu area");
        }
    }

    Input& Input::Get() noexcept
    {
        static Input instance;
        return instance;
    }

    bool Input::Initialize() noexcept
    {
        bool expected = false;
        if (!m_Initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            TUTONES_LOG_TRACE("input", "Input initialize requested while already initialized");
            return true;
        }

        m_MenuOpen.store(false, std::memory_order_release);
        m_F4FallbackDown.store(false, std::memory_order_release);
        m_PendingActions.store(0, std::memory_order_release);
        Hooking::Win32Hook::Get().SetMessageHandler(&Input::HandleWindowMessage);

        TUTONES_LOG_INFO("input", "Win32 menu input routing initialized");
        TUTONES_LOG_INFO("input", "Controls: F4 toggle, Arrow keys navigate, Enter select, Escape back");
        TUTONES_LOG_DEBUG("input", "Numpad menu bindings are disabled");
        TUTONES_LOG_DEBUG("input", "Backspace is not bound to menu Back");
        TUTONES_LOG_DEBUG("input", "Tutones owns mouse, keyboard, and raw input while the menu is open");
        TUTONES_LOG_INFO("input", "Technical diagnostics are console-only; the menu UI remains feature-focused");
        return true;
    }

    void Input::Shutdown() noexcept
    {
        if (!m_Initialized.exchange(false, std::memory_order_acq_rel))
        {
            TUTONES_LOG_TRACE("input", "Input shutdown requested while not initialized");
            return;
        }

        Hooking::Win32Hook::Get().SetMessageHandler(nullptr);
        m_MenuOpen.store(false, std::memory_order_release);
        m_F4FallbackDown.store(false, std::memory_order_release);
        m_PendingActions.store(0, std::memory_order_release);

        if (ImGui::GetCurrentContext())
            ImGui::GetIO().MouseDrawCursor = false;

        TUTONES_LOG_INFO("input", "Win32 menu input routing stopped");
    }

    bool Input::IsInitialized() const noexcept
    {
        return m_Initialized.load(std::memory_order_acquire);
    }

    bool Input::IsMenuOpen() const noexcept
    {
        return m_MenuOpen.load(std::memory_order_acquire);
    }

    void Input::SetMenuOpen(bool open) noexcept
    {
        const auto previous = m_MenuOpen.exchange(open, std::memory_order_acq_rel);
        if (previous == open)
            return;

        m_PendingActions.store(0, std::memory_order_release);

        if (ImGui::GetCurrentContext())
        {
            auto& io = ImGui::GetIO();
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            io.MouseDrawCursor = open;
        }

        if (open)
        {
            ::ReleaseCapture();
            CenterCursorInGameWindow();
            TUTONES_LOG_INFO("input", "Tutones menu opened; game mouse/keyboard/raw input capture suspended");
        }
        else
        {
            TUTONES_LOG_INFO("input", "Tutones menu closed; game input routing restored");
        }
    }

    void Input::ToggleMenu() noexcept
    {
        SetMenuOpen(!IsMenuOpen());
    }

    void Input::PollFallbackHotkeys() noexcept
    {
        if (!IsInitialized())
            return;

        const bool down = (::GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        const bool wasDown = m_F4FallbackDown.exchange(down, std::memory_order_acq_rel);
        if (down && !wasDown)
        {
            TUTONES_LOG_DEBUG("input", "F4 detected by render-frame fallback");
            ToggleMenu();
        }
    }

    std::uint32_t Input::ConsumePendingActions() noexcept
    {
        return m_PendingActions.exchange(0, std::memory_order_acq_rel);
    }

    bool Input::Consume(InputAction action) noexcept
    {
        const auto mask = ToMask(action);
        auto current = m_PendingActions.load(std::memory_order_acquire);

        while ((current & mask) != 0)
        {
            const auto desired = current & ~mask;
            if (m_PendingActions.compare_exchange_weak(
                    current,
                    desired,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return true;
            }
        }

        return false;
    }

    bool Input::HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        auto& input = Get();
        if (!input.IsInitialized())
            return false;

        // Tutones command keys are handled before ImGui sees them so a single
        // key press can only update one menu navigation path.
        if (IsKeyDownMessage(message))
        {
            if (wParam == VK_F4)
            {
                input.m_F4FallbackDown.store(true, std::memory_order_release);
                if (!IsRepeat(lParam))
                    input.ToggleMenu();
                return true;
            }

            if (input.IsMenuOpen() && IsMenuKey(wParam))
            {
                if (!IsRepeat(lParam))
                {
                    switch (wParam)
                    {
                    case VK_UP:     input.Queue(InputAction::Up, "Up"); break;
                    case VK_DOWN:   input.Queue(InputAction::Down, "Down"); break;
                    case VK_LEFT:   input.Queue(InputAction::Left, "Left"); break;
                    case VK_RIGHT:  input.Queue(InputAction::Right, "Right"); break;
                    case VK_RETURN: input.Queue(InputAction::Select, "Select"); break;
                    case VK_ESCAPE: input.Queue(InputAction::Back, "Back"); break;
                    default: break;
                    }
                }
                return true;
            }

            if (wParam == VK_BACK && input.IsMenuOpen())
            {
                TUTONES_LOG_TRACE("input", "Backspace ignored as menu Back; Escape is the Back binding");
                return true;
            }
        }

        if (IsKeyUpMessage(message))
        {
            if (wParam == VK_F4)
            {
                input.m_F4FallbackDown.store(false, std::memory_order_release);
                return true;
            }
            if (input.IsMenuOpen() && IsMenuKey(wParam))
                return true;
        }

        if (!input.IsMenuOpen())
            return false;

        // GTA uses raw input in addition to ordinary Win32 keyboard/mouse
        // messages. Let DefWindowProc perform WM_INPUT cleanup, but do not
        // forward the raw packet to the game's original WndProc while open.
        if (message == WM_INPUT)
        {
            static std::atomic<bool> loggedRawCapture{false};
            if (!loggedRawCapture.exchange(true, std::memory_order_acq_rel))
                TUTONES_LOG_DEBUG("input.raw", "Raw input suppression active while Tutones menu is open");

            static_cast<void>(::DefWindowProcW(window, message, wParam, lParam));
            return true;
        }

        bool imguiHandled = false;
        if (ImGui::GetCurrentContext())
            imguiHandled = ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;

        // The overlay owns pointer and keyboard input while open. Do not rely
        // on WantCapture* here: GTA must not receive clicks, wheel events, or
        // menu navigation underneath the Tutones window.
        if (IsMouseMessage(message))
            return true;
        if (IsKeyboardMessage(message))
            return true;

        return imguiHandled;
    }

    bool Input::IsRepeat(LPARAM lParam) noexcept
    {
        return (static_cast<std::uint64_t>(lParam) & (1ull << 30)) != 0;
    }

    bool Input::IsMenuKey(WPARAM key) noexcept
    {
        switch (key)
        {
        case VK_F4:
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_RETURN:
        case VK_ESCAPE:
            return true;
        default:
            return false;
        }
    }

    void Input::Queue(InputAction action, const char* actionName) noexcept
    {
        m_PendingActions.fetch_or(ToMask(action), std::memory_order_acq_rel);
        TUTONES_LOG_DEBUG("input", actionName);
    }
}
