#pragma once

#include "BackendTypes.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace Tutones::Backend
{
    class CapabilityRegistry final
    {
    public:
        void Refresh() noexcept;
        void Reset() noexcept;

        [[nodiscard]] bool Has(Capability capability) const noexcept;
        [[nodiscard]] bool HasAll(const std::vector<Capability>& capabilities) const noexcept;
        [[nodiscard]] std::vector<Capability> Missing(const std::vector<Capability>& capabilities) const;
        [[nodiscard]] CapabilitySnapshot Snapshot() const;

    private:
        void Set(Capability capability, bool available, std::string detail);

        mutable std::mutex m_Mutex;
        CapabilitySnapshot m_Snapshot{};
    };
}
