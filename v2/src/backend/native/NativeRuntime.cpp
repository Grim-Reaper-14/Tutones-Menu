#include "NativeRuntime.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Backend::Native
{
    NativeRuntime& NativeRuntime::Get() noexcept
    {
        static NativeRuntime instance;
        return instance;
    }

    bool NativeRuntime::Initialize() noexcept
    {
        m_Ready = true;
        Core::Logging::Logger::Get().Info("native", "Native runtime shell ready; resolver wiring is the next milestone");
        return true;
    }

    void NativeRuntime::Shutdown() noexcept
    {
        m_Ready = false;
    }

    bool NativeRuntime::IsReady() const noexcept
    {
        return m_Ready;
    }
}
