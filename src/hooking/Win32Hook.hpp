#pragma once

#include <Windows.h>

#include <atomic>
#include <mutex>

namespace Tutones::Hooking
{
    using MessageHandler = bool(*)(HWND, UINT, WPARAM, LPARAM) noexcept;

    class Win32Hook final
    {
    public:
        static Win32Hook& Get() noexcept;

        bool Attach(HWND window) noexcept;
        void Detach() noexcept;

        void SetMessageHandler(MessageHandler handler) noexcept;

        [[nodiscard]] bool IsAttached() const noexcept;
        [[nodiscard]] HWND Window() const noexcept;
        [[nodiscard]] WNDPROC OriginalProc() const noexcept;

    private:
        Win32Hook() = default;
        ~Win32Hook() = default;
        Win32Hook(const Win32Hook&) = delete;
        Win32Hook& operator=(const Win32Hook&) = delete;

        static LRESULT CALLBACK Detour(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

        mutable std::mutex m_Mutex;
        std::atomic<HWND> m_Window{nullptr};
        std::atomic<WNDPROC> m_OriginalProc{nullptr};
        std::atomic<MessageHandler> m_Handler{nullptr};
    };
}
