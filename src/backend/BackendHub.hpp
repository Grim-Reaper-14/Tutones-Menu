#pragma once

#include "BackendTypes.hpp"
#include "CapabilityRegistry.hpp"
#include "FeatureRegistry.hpp"
#include "ScriptGateway.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Tutones::Backend
{
    class BackendHub final
    {
    public:
        static BackendHub& Get() noexcept;

        bool Initialize();
        void Shutdown() noexcept;
        void TickOnGameThread() noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] bool RegisterFeature(FeatureDescriptor descriptor);
        [[nodiscard]] bool RetryFeature(const std::string& id) noexcept;
        [[nodiscard]] bool CanRun(const std::vector<Capability>& capabilities) const noexcept;

        [[nodiscard]] BackendSnapshot Snapshot() const;
        [[nodiscard]] GameContextSnapshot Context() const noexcept;
        [[nodiscard]] CapabilityRegistry& Capabilities() noexcept;
        [[nodiscard]] const CapabilityRegistry& Capabilities() const noexcept;
        [[nodiscard]] FeatureRegistry& Features() noexcept;
        [[nodiscard]] const FeatureRegistry& Features() const noexcept;
        [[nodiscard]] ScriptGateway& Scripts() noexcept;
        [[nodiscard]] const ScriptGateway& Scripts() const noexcept;

    private:
        BackendHub() = default;
        ~BackendHub() = default;
        BackendHub(const BackendHub&) = delete;
        BackendHub& operator=(const BackendHub&) = delete;

        void RegisterBuiltinFeatures();
        void RefreshContext() noexcept;
        [[nodiscard]] bool QueueNextTick() noexcept;
        [[nodiscard]] static std::uint64_t MonotonicMs() noexcept;

        std::atomic<bool> m_Initialized{false};
        std::atomic<std::uint64_t> m_TickSequence{0};
        std::atomic<std::uint32_t> m_ActiveTicks{0};
        mutable std::mutex m_ContextMutex;
        GameContextSnapshot m_Context{};
        CapabilityRegistry m_Capabilities;
        FeatureRegistry m_Features;
        ScriptGateway m_Scripts;
        bool m_BuiltinsRegistered{};
    };
}
