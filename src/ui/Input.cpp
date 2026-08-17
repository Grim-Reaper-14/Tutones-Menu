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
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
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
                return true;
            default:
                return false;
            }
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
        m_PendingActions.store(0, std::memory_order_release);
        Hooking::Win32Hook::Get().SetMessageHandler(&Input::HandleWindowMessage);

        TUTONES_LOG_INFO("input", "Win32 menu input routing initialized");
        TUTONES_LOG_INFO("input", "Controls: F4 toggle, NUM8 up, NUM2 down, NUM4 left, NUM6 right, NUM5 select, NUM0 back");
        TUTONES_LOG_DEBUG("input", "Backspace is intentionally not bound to menu Back");
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
        m_PendingActions.store(0, std::memory_order_release);
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

        if (!open)
            m_PendingActions.store(0, std::memory_order_release);

        TUTONES_LOG_INFO("input", open ? "Tutones menu opened" : "Tutones menu closed");
    }

    void Input::ToggleMenu() noexcept
    {
        SetMenuOpen(!IsMenuOpen());
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

        bool imguiHandled = false;
        if (ImGui::GetCurrentContext())
            imguiHandled = ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0;

        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        {
            if (IsRepeat(lParam))
                return IsMenuKey(wParam) && input.IsMenuOpen();

            switch (wParam)
            {
            case VK_F4:
                input.ToggleMenu();
                return true;

            case VK_NUMPAD8:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Up, "Up"); return true; }
                break;
            case VK_NUMPAD2:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Down, "Down"); return true; }
                break;
            case VK_NUMPAD4:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Left, "Left"); return true; }
                break;
            case VK_NUMPAD6:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Right, "Right"); return true; }
                break;
            case VK_NUMPAD5:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Select, "Select"); return true; }
                break;
            case VK_NUMPAD0:
                if (input.IsMenuOpen()) { input.Queue(InputAction::Back, "Back"); return true; }
                break;
            case VK_BACK:
                if (input.IsMenuOpen())
                    TUTONES_LOG_TRACE("input", "Backspace ignored as menu Back; NUM0 remains the only Back binding");
                break;
            default:
                break;
            }
        }

        if (!input.IsMenuOpen() || !ImGui::GetCurrentContext())
            return false;

        const auto& io = ImGui::GetIO();
        if (IsMouseMessage(message))
            return imguiHandled || io.WantCaptureMouse;
        if (IsKeyboardMessage(message))
            return imguiHandled || io.WantCaptureKeyboard;

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
        case VK_NUMPAD8:
        case VK_NUMPAD2:
        case VK_NUMPAD4:
        case VK_NUMPAD6:
        case VK_NUMPAD5:
        case VK_NUMPAD0:
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
