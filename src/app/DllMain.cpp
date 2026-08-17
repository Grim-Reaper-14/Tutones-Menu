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

        if (moduleDirectory.empty() || !Tutones::App::Application::Get().Initialize(moduleDirectory))
        {
            ::FreeLibraryAndExitThread(module, 1);
            return 1;
        }

        TUTONES_LOG_INFO("app", "Tutones Menu bootstrap thread running");
        TUTONES_LOG_INFO("app", "Press END to unload Tutones Menu");

        while (Tutones::App::Application::Get().IsRunning())
        {
            if ((::GetAsyncKeyState(VK_END) & 1) != 0)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        Tutones::App::Application::Get().Shutdown();
        ::FreeLibraryAndExitThread(module, 0);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ::DisableThreadLibraryCalls(module);

        const auto thread = ::CreateThread(
            nullptr,
            0,
            BootstrapThread,
            module,
            0,
            nullptr);

        if (thread)
            ::CloseHandle(thread);
    }

    return TRUE;
}
