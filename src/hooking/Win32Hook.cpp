#include "Win32Hook.hpp"

#include "../core/logging/Logger.hpp"

#include <string>

namespace Tutones::Hooking
{
    namespace
    {
        enum class WindowValidation
        {
            Valid,
            NullHandle,
            InvalidHandle,
            NotVisible,
            NotRootWindow,
            ForeignProcess,
            ClientRectFailed,
            EmptyClientArea,
        };

        WindowValidation ValidatePrimaryWindow(HWND window) noexcept
        {
            if (!window)
                return WindowValidation::NullHandle;
            if (!::IsWindow(window))
                return WindowValidation::InvalidHandle;
            if (!::IsWindowVisible(window))
                return WindowValidation::NotVisible;
            if (::GetAncestor(window, GA_ROOT) != window)
                return WindowValidation::NotRootWindow;

            DWORD processId{};
            ::GetWindowThreadProcessId(window, &processId);
            if (processId != ::GetCurrentProcessId())
                return WindowValidation::ForeignProcess;

            RECT client{};
            if (!::GetClientRect(window, &client))
                return WindowValidation::ClientRectFailed;
            if ((client.right - client.left) <= 0 || (client.bottom - client.top) <= 0)
                return WindowValidation::EmptyClientArea;

            return WindowValidation::Valid;
        }

        const char* ValidationReason(WindowValidation validation) noexcept
        {
            switch (validation)
            {
            case WindowValidation::Valid: return "valid";
            case WindowValidation::NullHandle: return "null HWND";
            case WindowValidation::InvalidHandle: return "invalid HWND";
            case WindowValidation::NotVisible: return "window is not visible";
            case WindowValidation::NotRootWindow: return "window is not a root window";
            case WindowValidation::ForeignProcess: return "window belongs to another process";
            case WindowValidation::ClientRectFailed: return "GetClientRect failed";
            case WindowValidation::EmptyClientArea: return "window has an empty client area";
            }
            return "unknown validation result";
        }
    }

    Win32Hook& Win32Hook::Get() noexcept
    {
        static Win32Hook instance;
        return instance;
    }

    bool Win32Hook::IsValidPrimaryWindow(HWND window) noexcept
    {
        return ValidatePrimaryWindow(window) == WindowValidation::Valid;
    }

    bool Win32Hook::Attach(HWND window) noexcept
    {
        const auto validation = ValidatePrimaryWindow(window);
        if (validation != WindowValidation::Valid)
        {
            if (m_LastRejectedWindow.exchange(window, std::memory_order_acq_rel) != window)
            {
                std::string message("Rejected render-window candidate: ");
                message += ValidationReason(validation);
                TUTONES_LOG_DEBUG("hook.win32", message);
            }
            return false;
        }

        std::scoped_lock lock(m_Mutex);

        const auto currentWindow = m_Window.load(std::memory_order_acquire);
        const auto currentOriginal = m_OriginalProc.load(std::memory_order_acquire);
        if (currentWindow == window && currentOriginal)
        {
            TUTONES_LOG_TRACE("hook.win32", "Primary WndProc hook is already attached");
            return true;
        }

        if (currentWindow && currentOriginal)
        {
            if (::IsWindow(currentWindow))
            {
                if (!m_LoggedSecondaryWindow.exchange(true, std::memory_order_acq_rel))
                    TUTONES_LOG_DEBUG("hook.win32", "Ignored secondary render window because the primary HWND is already pinned");
                return false;
            }

            TUTONES_LOG_WARN("hook.win32", "Previously pinned HWND is no longer valid; clearing stale WndProc state");
            m_Window.store(nullptr, std::memory_order_release);
            m_OriginalProc.store(nullptr, std::memory_order_release);
        }

        TUTONES_LOG_DEBUG("hook.win32", "Installing WndProc hook on validated primary render window");

        ::SetLastError(ERROR_SUCCESS);
        const auto previous = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
            window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&Win32Hook::Detour)));
        const auto lastError = ::GetLastError();

        if (!previous && lastError != ERROR_SUCCESS)
        {
            std::string message("Failed to install Win32 WndProc hook; GetLastError=");
            message += std::to_string(lastError);
            TUTONES_LOG_ERROR("hook.win32", message);
            return false;
        }

        RECT client{};
        if (::GetClientRect(window, &client))
        {
            const auto width = client.right - client.left;
            const auto height = client.bottom - client.top;
            std::string message("Primary render window pinned; client size=");
            message += std::to_string(width);
            message += 'x';
            message += std::to_string(height);
            TUTONES_LOG_INFO("hook.win32", message);
        }
        else
        {
            TUTONES_LOG_INFO("hook.win32", "Primary render window pinned and WndProc hook installed");
        }

        m_OriginalProc.store(previous, std::memory_order_release);
        m_Window.store(window, std::memory_order_release);
        m_LastRejectedWindow.store(nullptr, std::memory_order_release);
        return true;
    }

    void Win32Hook::Detach() noexcept
    {
        std::scoped_lock lock(m_Mutex);

        const auto window = m_Window.exchange(nullptr, std::memory_order_acq_rel);
        const auto original = m_OriginalProc.exchange(nullptr, std::memory_order_acq_rel);
        m_LastRejectedWindow.store(nullptr, std::memory_order_release);
        m_LoggedSecondaryWindow.store(false, std::memory_order_release);

        if (!window || !original)
        {
            TUTONES_LOG_TRACE("hook.win32", "WndProc detach requested with no active Win32 hook");
            return;
        }

        if (!::IsWindow(window))
        {
            TUTONES_LOG_WARN("hook.win32", "Primary render window vanished before WndProc could be restored");
            return;
        }

        TUTONES_LOG_DEBUG("hook.win32", "Restoring original WndProc on primary render window");
        ::SetLastError(ERROR_SUCCESS);
        const auto previous = ::SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        const auto lastError = ::GetLastError();
        if (!previous && lastError != ERROR_SUCCESS)
        {
            std::string message("Failed to restore original WndProc; GetLastError=");
            message += std::to_string(lastError);
            TUTONES_LOG_ERROR("hook.win32", message);
            return;
        }

        TUTONES_LOG_INFO("hook.win32", "Win32 WndProc hook removed and original procedure restored");
    }

    void Win32Hook::SetMessageHandler(MessageHandler handler) noexcept
    {
        const auto previous = m_Handler.exchange(handler, std::memory_order_acq_rel);
        if (previous == handler)
            return;

        if (handler)
            TUTONES_LOG_INFO("hook.win32", "Win32 message handler registered");
        else
            TUTONES_LOG_INFO("hook.win32", "Win32 message handler cleared");
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

        static std::atomic<bool> loggedMissingOriginal{false};
        if (!loggedMissingOriginal.exchange(true, std::memory_order_acq_rel))
            TUTONES_LOG_ERROR("hook.win32", "WndProc detour had no original procedure; falling back to DefWindowProcW");

        return ::DefWindowProcW(window, message, wParam, lParam);
    }
}
