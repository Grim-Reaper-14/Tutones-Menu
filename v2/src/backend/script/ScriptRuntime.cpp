#include "ScriptRuntime.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Backend::Script
{
    ScriptRuntime& ScriptRuntime::Get() noexcept
    {
        static ScriptRuntime instance;
        return instance;
    }

    bool ScriptRuntime::Initialize() noexcept
    {
        m_Ready = true;
        Core::Logging::Logger::Get().Info("script", "Script runtime shell ready");
        return true;
    }

    void ScriptRuntime::Shutdown() noexcept
    {
        m_Ready = false;
    }

    bool ScriptRuntime::IsReady() const noexcept
    {
        return m_Ready;
    }
}
