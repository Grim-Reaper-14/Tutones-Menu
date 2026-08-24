#include "FeatureRegistry.hpp"

#include "../core/logging/Logger.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace Tutones::Backend
{
    bool FeatureRegistry::Register(FeatureDescriptor descriptor)
    {
        if (descriptor.id.empty() || descriptor.displayName.empty())
            return false;

        std::scoped_lock lock(m_Mutex);
        const auto duplicate = std::find_if(
            m_Features.begin(),
            m_Features.end(),
            [&descriptor](const FeatureRecord& record) {
                return record.descriptor.id == descriptor.id;
            });
        if (duplicate != m_Features.end())
        {
            TUTONES_LOG_WARN(
                "backend.features",
                std::string("Rejected duplicate feature registration: ") + descriptor.id);
            return false;
        }

        FeatureRecord record;
        record.health.id = descriptor.id;
        record.health.displayName = descriptor.displayName;
        record.health.category = descriptor.category;
        record.health.tickRate = descriptor.tickRate;
        record.health.requirements = descriptor.requirements;
        record.health.state = RuntimeState::Offline;
        record.health.detail = "Registered";
        record.descriptor = std::move(descriptor);
        m_Features.emplace_back(std::move(record));
        return true;
    }

    void FeatureRegistry::StartEligible(const CapabilityRegistry& capabilities) noexcept
    {
        std::size_t count{};
        {
            std::scoped_lock lock(m_Mutex);
            count = m_Features.size();
        }

        for (std::size_t index = 0; index < count; ++index)
            TryStart(index, capabilities);
    }

    void FeatureRegistry::Tick(
        const GameContextSnapshot& context,
        const CapabilityRegistry& capabilities,
        std::uint64_t nowMs) noexcept
    {
        std::size_t count{};
        {
            std::scoped_lock lock(m_Mutex);
            count = m_Features.size();
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            FeatureDescriptor descriptor;
            RuntimeState state{};
            std::uint64_t lastTick{};
            {
                std::scoped_lock lock(m_Mutex);
                if (index >= m_Features.size())
                    break;
                descriptor = m_Features[index].descriptor;
                state = m_Features[index].health.state;
                lastTick = m_Features[index].lastTickMs;
            }

            const auto missing = capabilities.Missing(descriptor.requirements);
            if (!missing.empty())
            {
                if (state == RuntimeState::Healthy || state == RuntimeState::Starting)
                    SuspendForMissingDependencies(index, missing);
                else if (state == RuntimeState::Offline || state == RuntimeState::WaitingForDependency)
                {
                    std::scoped_lock lock(m_Mutex);
                    if (index < m_Features.size())
                    {
                        m_Features[index].health.state = RuntimeState::WaitingForDependency;
                        m_Features[index].health.detail = MissingDetail(missing);
                    }
                }
                continue;
            }

            if (state == RuntimeState::Offline || state == RuntimeState::WaitingForDependency)
            {
                TryStart(index, capabilities);
                std::scoped_lock lock(m_Mutex);
                if (index >= m_Features.size() || m_Features[index].health.state != RuntimeState::Healthy)
                    continue;
                descriptor = m_Features[index].descriptor;
                lastTick = m_Features[index].lastTickMs;
            }
            else if (state != RuntimeState::Healthy)
            {
                continue;
            }

            if (!descriptor.tick || descriptor.tickRate == TickRate::OnDemand)
                continue;

            const auto interval = IntervalMs(descriptor.tickRate);
            if (interval != 0 && nowMs - lastTick < interval)
                continue;

            try
            {
                descriptor.tick(context);
                std::scoped_lock lock(m_Mutex);
                if (index < m_Features.size())
                    m_Features[index].lastTickMs = nowMs;
            }
            catch (const std::exception& exception)
            {
                std::scoped_lock lock(m_Mutex);
                if (index < m_Features.size())
                {
                    m_Features[index].health.state = RuntimeState::Faulted;
                    m_Features[index].health.detail = std::string("Tick exception: ") + exception.what();
                }
                TUTONES_LOG_ERROR(
                    "backend.features",
                    descriptor.displayName + " tick failed: " + exception.what());
            }
            catch (...)
            {
                std::scoped_lock lock(m_Mutex);
                if (index < m_Features.size())
                {
                    m_Features[index].health.state = RuntimeState::Faulted;
                    m_Features[index].health.detail = "Tick threw an unknown exception";
                }
                TUTONES_LOG_ERROR(
                    "backend.features",
                    descriptor.displayName + " tick failed with unknown exception");
            }
        }
    }

    void FeatureRegistry::StopAll() noexcept
    {
        std::vector<std::function<void()>> stops;
        std::vector<std::string> names;
        {
            std::scoped_lock lock(m_Mutex);
            stops.reserve(m_Features.size());
            names.reserve(m_Features.size());
            for (auto it = m_Features.rbegin(); it != m_Features.rend(); ++it)
            {
                if (it->health.state == RuntimeState::Healthy
                    || it->health.state == RuntimeState::Degraded
                    || it->health.state == RuntimeState::Faulted
                    || it->health.state == RuntimeState::WaitingForDependency)
                {
                    it->health.state = RuntimeState::Stopping;
                    it->health.detail = "Stopping";
                    stops.push_back(it->descriptor.stop);
                    names.push_back(it->descriptor.displayName);
                }
            }
        }

        for (std::size_t index = 0; index < stops.size(); ++index)
        {
            if (!stops[index])
                continue;
            try
            {
                stops[index]();
            }
            catch (...)
            {
                TUTONES_LOG_ERROR(
                    "backend.features",
                    names[index] + " threw during shutdown");
            }
        }

        std::scoped_lock lock(m_Mutex);
        for (auto& record : m_Features)
        {
            record.health.state = RuntimeState::Offline;
            record.health.detail = "Stopped";
            record.lastTickMs = 0;
        }
    }

    void FeatureRegistry::Reset() noexcept
    {
        StopAll();
        std::scoped_lock lock(m_Mutex);
        m_Features.clear();
    }

    bool FeatureRegistry::Retry(const std::string& id) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        const auto found = std::find_if(
            m_Features.begin(),
            m_Features.end(),
            [&id](const FeatureRecord& record) { return record.descriptor.id == id; });
        if (found == m_Features.end())
            return false;
        if (found->health.state != RuntimeState::Faulted && found->health.state != RuntimeState::Degraded)
            return false;
        found->health.state = RuntimeState::Offline;
        found->health.detail = "Manual retry requested";
        return true;
    }

    std::vector<FeatureHealthSnapshot> FeatureRegistry::Snapshot() const
    {
        std::vector<FeatureHealthSnapshot> snapshot;
        std::scoped_lock lock(m_Mutex);
        snapshot.reserve(m_Features.size());
        for (const auto& record : m_Features)
            snapshot.push_back(record.health);
        return snapshot;
    }

    std::size_t FeatureRegistry::Count() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Features.size();
    }

    std::uint64_t FeatureRegistry::IntervalMs(TickRate rate) noexcept
    {
        switch (rate)
        {
        case TickRate::EveryFrame: return 0;
        case TickRate::Fast: return 50;
        case TickRate::Normal: return 100;
        case TickRate::Slow: return 500;
        case TickRate::Background: return 1000;
        case TickRate::OnDemand: return 0;
        }
        return 0;
    }

    void FeatureRegistry::TryStart(std::size_t index, const CapabilityRegistry& capabilities) noexcept
    {
        FeatureDescriptor descriptor;
        {
            std::scoped_lock lock(m_Mutex);
            if (index >= m_Features.size())
                return;
            auto& record = m_Features[index];
            if (record.health.state == RuntimeState::Healthy
                || record.health.state == RuntimeState::Starting
                || record.health.state == RuntimeState::Faulted
                || record.health.state == RuntimeState::Stopping)
            {
                return;
            }
            descriptor = record.descriptor;
        }

        const auto missing = capabilities.Missing(descriptor.requirements);
        if (!missing.empty())
        {
            std::scoped_lock lock(m_Mutex);
            if (index < m_Features.size())
            {
                m_Features[index].health.state = RuntimeState::WaitingForDependency;
                m_Features[index].health.detail = MissingDetail(missing);
            }
            return;
        }

        {
            std::scoped_lock lock(m_Mutex);
            if (index >= m_Features.size())
                return;
            m_Features[index].health.state = RuntimeState::Starting;
            m_Features[index].health.detail = "Starting";
            ++m_Features[index].health.startAttempts;
        }

        bool started = true;
        try
        {
            if (descriptor.start)
                started = descriptor.start();
        }
        catch (const std::exception& exception)
        {
            std::scoped_lock lock(m_Mutex);
            if (index < m_Features.size())
            {
                m_Features[index].health.state = RuntimeState::Faulted;
                m_Features[index].health.detail = std::string("Startup exception: ") + exception.what();
            }
            TUTONES_LOG_ERROR(
                "backend.features",
                descriptor.displayName + " startup failed: " + exception.what());
            return;
        }
        catch (...)
        {
            std::scoped_lock lock(m_Mutex);
            if (index < m_Features.size())
            {
                m_Features[index].health.state = RuntimeState::Faulted;
                m_Features[index].health.detail = "Startup threw an unknown exception";
            }
            TUTONES_LOG_ERROR(
                "backend.features",
                descriptor.displayName + " startup failed with unknown exception");
            return;
        }

        if (!started)
        {
            try
            {
                if (descriptor.stop)
                    descriptor.stop();
            }
            catch (...)
            {
                TUTONES_LOG_ERROR(
                    "backend.features",
                    descriptor.displayName + " cleanup failed after rejected startup");
            }

            std::scoped_lock lock(m_Mutex);
            if (index < m_Features.size())
            {
                m_Features[index].health.state = RuntimeState::Faulted;
                m_Features[index].health.detail = "Start() returned false";
            }
            TUTONES_LOG_WARN(
                "backend.features",
                descriptor.displayName + " faulted; unrelated features remain active");
            return;
        }

        {
            std::scoped_lock lock(m_Mutex);
            if (index < m_Features.size())
            {
                m_Features[index].health.state = RuntimeState::Healthy;
                m_Features[index].health.detail = "Running";
            }
        }
        TUTONES_LOG_INFO("backend.features", descriptor.displayName + " is healthy");
    }

    void FeatureRegistry::SuspendForMissingDependencies(
        std::size_t index,
        const std::vector<Capability>& missing) noexcept
    {
        std::function<void()> stop;
        std::string name;
        {
            std::scoped_lock lock(m_Mutex);
            if (index >= m_Features.size())
                return;
            auto& record = m_Features[index];
            record.health.state = RuntimeState::WaitingForDependency;
            record.health.detail = MissingDetail(missing);
            stop = record.descriptor.stop;
            name = record.descriptor.displayName;
        }

        try
        {
            if (stop)
                stop();
        }
        catch (...)
        {
            TUTONES_LOG_ERROR(
                "backend.features",
                name + " threw while suspending for missing dependencies");
        }

        TUTONES_LOG_WARN(
            "backend.features",
            name + " suspended: " + MissingDetail(missing));
    }

    std::string FeatureRegistry::MissingDetail(const std::vector<Capability>& missing)
    {
        if (missing.empty())
            return "Dependencies ready";

        std::string detail = "Waiting for ";
        for (std::size_t index = 0; index < missing.size(); ++index)
        {
            if (index != 0)
                detail += ", ";
            detail += CapabilityName(missing[index]);
        }
        return detail;
    }
}
