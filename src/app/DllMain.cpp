#include "Application.hpp"

#include "../core/logging/Logger.hpp"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    std::filesystem::path ModuleDirectory(HMODULE module)
    {
        wchar_t buffer[MAX_PATH]{};
        const auto length = ::GetModuleFileNameW(module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return {};

        return std::filesystem::path(buffer).parent_path();
    }

    DWORD WINAPI BootstrapThread(void* parameter)
    {
        const auto module = static_cast<HMODULE>(parameter);
        const auto moduleDirectory = ModuleDirectory(module);

        if (moduleDirectory.empty())
        {
            ::OutputDebugStringA("[Tutones] Failed to resolve module directory before logger initialization.\n");
            ::FreeLibraryAndExitThread(module, 1);
            return 1;
        }

        if (!Tutones::App::Application::Get().Initialize(moduleDirectory))
        {
            ::OutputDebugStringA("[Tutones] Application initialization failed. Check Tutones logs if logger initialization completed.\n");
            ::FreeLibraryAndExitThread(module, 1);
            return 1;
        }

        TUTONES_LOG_INFO("bootstrap", "Tutones Menu bootstrap thread running");
        TUTONES_LOG_DEBUG("bootstrap", "Module directory resolved and application startup completed");
        TUTONES_LOG_INFO("bootstrap", "Press END to unload Tutones Menu");

        while (Tutones::App::Application::Get().IsRunning())
        {
            if ((::GetAsyncKeyState(VK_END) & 1) != 0)
            {
                TUTONES_LOG_INFO("bootstrap", "END key unload request received");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        TUTONES_LOG_DEBUG("bootstrap", "Bootstrap loop exiting; beginning application shutdown");
        Tutones::App::Application::Get().Shutdown();

        ::OutputDebugStringA("[Tutones] Bootstrap shutdown complete; unloading module.\n");
        ::FreeLibraryAndExitThread(module, 0);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ::DisableThreadLibraryCalls(module);

        const auto thread = ::CreateThread(nullptr, 0, BootstrapThread, module, 0, nullptr);
        if (thread)
        {
            ::CloseHandle(thread);
        }
        else
        {
            // The full logger cannot safely be initialized from DllMain. The
            // debugger fallback still records this earliest bootstrap failure.
            ::OutputDebugStringA("[Tutones] CreateThread failed during DLL_PROCESS_ATTACH.\n");
        }
    }

    return TRUE;
}
