#include "core/Application.hpp"

#include <Windows.h>

namespace
{
    DWORD WINAPI TutonesV2Bootstrap(LPVOID parameter)
    {
        auto* module = static_cast<HMODULE>(parameter);
        TutonesV2::Core::Application::Get().Initialize(module);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ::DisableThreadLibraryCalls(module);

        if (HANDLE thread = ::CreateThread(nullptr, 0, TutonesV2Bootstrap, module, 0, nullptr))
            ::CloseHandle(thread);
    }

    return TRUE;
}
