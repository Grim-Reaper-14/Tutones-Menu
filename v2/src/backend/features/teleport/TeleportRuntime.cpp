#include "TeleportRuntime.hpp"
#include "core/logging/Logger.hpp"

namespace TutonesV2::Backend::Features::Teleport
{
    TeleportRuntime& TeleportRuntime::Get() noexcept
    {
        static TeleportRuntime instance;
        return instance;
    }

    bool TeleportRuntime::Initialize() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Pending.reset();
        m_Ready = true;
        Core::Logging::Logger::Get().Info("teleport", "Teleport backend registered as a main V2 feature");
        return true;
    }

    void TeleportRuntime::Shutdown() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Pending.reset();
        m_Ready = false;
    }

    bool TeleportRuntime::Submit(TeleportRequest request) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Ready)
            return false;

        m_Pending = request;
        return true;
    }

    std::optional<TeleportRequest> TeleportRuntime::ConsumePending() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        auto pending = m_Pending;
        m_Pending.reset();
        return pending;
    }

    bool TeleportRuntime::IsReady() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Ready;
    }
}
