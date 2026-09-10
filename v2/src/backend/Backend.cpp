#include "Backend.hpp"

#include "hooking/HookManager.hpp"
#include "native/NativeRuntime.hpp"
#include "script/ScriptRuntime.hpp"
#include "features/teleport/TeleportRuntime.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Backend
{
    Backend& Backend::Get() noexcept
    {
        static Backend instance;
        return instance;
    }

    bool Backend::Initialize() noexcept
    {
        if (m_Ready)
            return true;

        Core::Logging::Logger::Get().Info("backend", "Initializing V2 backend services");

        if (!Native::NativeRuntime::Get().Initialize())
            return false;
        if (!Script::ScriptRuntime::Get().Initialize())
            return false;
        if (!Hooking::HookManager::Get().Initialize())
            return false;
        if (!Features::Teleport::TeleportRuntime::Get().Initialize())
            return false;

        m_Ready = true;
        Core::Logging::Logger::Get().Info("backend", "V2 backend services ready");
        return true;
    }

    void Backend::Shutdown() noexcept
    {
        if (!m_Ready)
            return;

        Features::Teleport::TeleportRuntime::Get().Shutdown();
        Hooking::HookManager::Get().Shutdown();
        Script::ScriptRuntime::Get().Shutdown();
        Native::NativeRuntime::Get().Shutdown();
        m_Ready = false;
    }

    bool Backend::IsReady() const noexcept
    {
        return m_Ready;
    }
}
