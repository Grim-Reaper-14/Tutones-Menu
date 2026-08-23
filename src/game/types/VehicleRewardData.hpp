#pragma once

#include "../script/ScriptLocal.hpp"
#include "ScriptTypes.hpp"

#include <cstdint>

namespace Tutones::Game::Types
{
    // Mirrors YimMenuV2's VEHICLE_REWARD_DATA layout for AM_MP_VEHICLE_REWARD.
    // The structure begins at script local 148 and every SCR_INT occupies an
    // eight-byte GTA script stack slot even though the value itself is 32-bit.
    struct VehicleRewardData final
    {
        std::uint64_t pad0000[3]{};
        alignas(8) std::int32_t transactionStatus{}; // local 151
        alignas(8) std::int32_t garage{};            // local 152
        alignas(8) std::int32_t garageOffset{};      // local 153
        alignas(8) std::int32_t controlStatus{};     // local 154
        std::uint64_t pad0008[40]{};

        [[nodiscard]] static VehicleRewardData* Get(ScriptThread* thread) noexcept
        {
            if (!thread || !thread->stack)
                return nullptr;

            return Script::ScriptLocal(thread, 148).As<VehicleRewardData>();
        }
    };

    static_assert(sizeof(VehicleRewardData) == 47 * sizeof(std::uint64_t));
}
