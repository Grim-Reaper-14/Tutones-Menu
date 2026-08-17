#include "Win32Hook.hpp"

#include "../core/logging/Logger.hpp"

namespace Tutones::Hooking
{
    Win32Hook& Win32Hook::Get() noexcept
    {
        static Win32Hook instance;
        return instance;
    }

    bool Win32Hook::IsValidPrimaryWindow(HWND window) noexcept
    {
        if (!window || !::IsWindow(window) || !::IsWindowVisible(window))
            return false;

        if (::GetAncestor(window, GA_ROOT) != window)
            return false;

        DWORD processId{};
        ::GetWindowThreadProcessId(window, &processId);
        if (processId != ::GetCurrentProcessId())
            return false;

        RECT client{};
        if (!::GetClientRect(window, &client))
            return false;

        return (client.right - client.left) > 0 && (client.bottom - client.top) > 0;
    }

    bool Win32Hook::Attach(HWND window) noexcept
    {
        if (!IsValidPrimaryWindow(window))
            return false;

        std::scoped_lock lock(m_Mutex);

        const auto currentWindow = m_Window.load(std::memory_order_acquire);
        const auto currentOriginal = m_OriginalProc.load(std::memory_order_acquire);
        if (currentWindow == window && currentOriginal)
            return true;

        if (currentWindow && currentOriginal)
        {
            if (::IsWindow(currentWindow))
                return false;

            m_Window.store(nullptr, std::memory_order_release);
            m_OriginalProc.store(nullptr, std::memory_order_release);
        }

        ::SetLastError(ERROR_SUCCESS);
        const auto previous = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
            window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&Win32Hook::Detour)));

        if (!previous && ::GetLastError() != ERROR_SUCCESS)
        {
            TUTONES_LOG_ERROR("hook", "Failed to install Win32 WndProc hook");
            return false;
        }

        m_OriginalProc.store(previous, std::memory_order_release);
        m_Window.store(window, std::memory_order_release);
        TUTONES_LOG_INFO("hook", "Pinned Win32 hook to primary render window");
        return true;
    }

    void Win32Hook::Detach() noexcept
    {
        std::scoped_lock lock(m_Mutex);

        const auto window = m_Window.exchange(nullptr, std::memory_order_acq_rel);
        const auto original = m_OriginalProc.exchange(nullptr, std::memory_order_acq_rel);

        if (window && original && ::IsWindow(window))
        {
            ::SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
            TUTONES_LOG_INFO("hook", "Win32 WndProc hook removed");
        }
    }

    void Win32Hook::SetMessageHandler(MessageHandler handler) noexcept
    {
        m_Handler.store(handler, std::memory_order_release);
    }

    bool Win32Hook::IsAttached() const noexcept
    {
        return m_Window.load(std::memory_order_acquire) != nullptr &&
            m_OriginalProc.load(std::memory_order_acquire) != nullptr;
    }

    bool Win32Hook::IsPrimaryWindow(HWND window) const noexcept
    {
        return window && m_Window.load(std::memory_order_acquire) == window;
    }

    HWND Win32Hook::Window() const noexcept
    {
        return m_Window.load(std::memory_order_acquire);
    }

    WNDPROC Win32Hook::OriginalProc() const noexcept
    {
        return m_OriginalProc.load(std::memory_order_acquire);
    }

    LRESULT CALLBACK Win32Hook::Detour(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        auto& self = Get();

        if (const auto handler = self.m_Handler.load(std::memory_order_acquire))
        {
            if (handler(window, message, wParam, lParam))
                return 1;
        }

        if (const auto original = self.m_OriginalProc.load(std::memory_order_acquire))
            return ::CallWindowProcW(original, window, message, wParam, lParam);

        return ::DefWindowProcW(window, message, wParam, lParam);
    }
}
