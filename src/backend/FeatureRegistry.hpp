#pragma once

#include "BackendTypes.hpp"
#include "CapabilityRegistry.hpp"

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Tutones::Backend
{
    struct FeatureDescriptor final
    {
        std::string id;
        std::string displayName;
        std::string category;
        TickRate tickRate{TickRate::OnDemand};
        std::vector<Capability> requirements;
        std::function<bool()> start;
        std::function<void()> stop;
        std::function<void(const GameContextSnapshot&)> tick;
    };

    class FeatureRegistry final
    {
    public:
        bool Register(FeatureDescriptor descriptor);
        void StartEligible(const CapabilityRegistry& capabilities) noexcept;
        void Tick(const GameContextSnapshot& context, const CapabilityRegistry& capabilities, std::uint64_t nowMs) noexcept;
        void StopAll() noexcept;
        void Reset() noexcept;

        [[nodiscard]] bool Retry(const std::string& id) noexcept;
        [[nodiscard]] std::vector<FeatureHealthSnapshot> Snapshot() const;
        [[nodiscard]] std::size_t Count() const noexcept;

    private:
        struct FeatureRecord final
        {
            FeatureDescriptor descriptor;
            FeatureHealthSnapshot health;
            std::uint64_t lastTickMs{};
        };

        [[nodiscard]] static std::uint64_t IntervalMs(TickRate rate) noexcept;
        void TryStart(std::size_t index, const CapabilityRegistry& capabilities) noexcept;
        void SuspendForMissingDependencies(std::size_t index, const std::vector<Capability>& missing) noexcept;
        [[nodiscard]] static std::string MissingDetail(const std::vector<Capability>& missing);

        mutable std::mutex m_Mutex;
        std::vector<FeatureRecord> m_Features;
    };
}
