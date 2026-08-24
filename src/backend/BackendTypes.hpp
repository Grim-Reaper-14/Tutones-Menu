#pragma once

#include "../game/Natives.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Tutones::Backend
{
    enum class Capability : std::size_t
    {
        CoreServices,
        GameRuntime,
        NativeRuntime,
        ScriptRuntime,
        ScriptGlobals,
        ScriptVm,
        OnlineSession,
        FreemodeScript,
        VehicleRewardScript,
        Count,
    };

    enum class RuntimeState : std::uint8_t
    {
        Offline,
        Starting,
        Healthy,
        WaitingForDependency,
        Degraded,
        Faulted,
        Stopping,
    };

    enum class TickRate : std::uint8_t
    {
        EveryFrame,
        Fast,
        Normal,
        Slow,
        Background,
        OnDemand,
    };

    [[nodiscard]] constexpr const char* CapabilityName(Capability capability) noexcept
    {
        switch (capability)
        {
        case Capability::CoreServices: return "Core Services";
        case Capability::GameRuntime: return "Game Runtime";
        case Capability::NativeRuntime: return "Native Runtime";
        case Capability::ScriptRuntime: return "Script Runtime";
        case Capability::ScriptGlobals: return "Script Globals";
        case Capability::ScriptVm: return "Script VM";
        case Capability::OnlineSession: return "Online Session";
        case Capability::FreemodeScript: return "Freemode Script";
        case Capability::VehicleRewardScript: return "Vehicle Reward Script";
        case Capability::Count: break;
        }
        return "Unknown";
    }

    [[nodiscard]] constexpr const char* RuntimeStateName(RuntimeState state) noexcept
    {
        switch (state)
        {
        case RuntimeState::Offline: return "Offline";
        case RuntimeState::Starting: return "Starting";
        case RuntimeState::Healthy: return "Healthy";
        case RuntimeState::WaitingForDependency: return "Waiting";
        case RuntimeState::Degraded: return "Degraded";
        case RuntimeState::Faulted: return "Faulted";
        case RuntimeState::Stopping: return "Stopping";
        }
        return "Unknown";
    }

    struct CapabilitySnapshot final
    {
        std::array<bool, static_cast<std::size_t>(Capability::Count)> available{};
        std::array<std::string, static_cast<std::size_t>(Capability::Count)> detail{};
        std::uint64_t revision{};

        [[nodiscard]] bool Has(Capability capability) const noexcept
        {
            const auto index = static_cast<std::size_t>(capability);
            return index < available.size() && available[index];
        }
    };

    struct GameContextSnapshot final
    {
        bool coreReady{};
        bool gameRuntimeReady{};
        bool nativeRuntimeReady{};
        bool scriptRuntimeReady{};
        bool scriptGlobalsReady{};
        bool scriptVmReady{};
        bool sessionStarted{};
        bool freemodeRunning{};
        bool vehicleRewardRunning{};

        Game::Ped playerPed{};
        bool inVehicle{};
        Game::Vehicle vehicle{};
        Game::Hash vehicleModel{};

        std::uint64_t gameStateSequence{};
        std::uint64_t hubSequence{};
    };

    struct FeatureHealthSnapshot final
    {
        std::string id;
        std::string displayName;
        std::string category;
        RuntimeState state{RuntimeState::Offline};
        TickRate tickRate{TickRate::OnDemand};
        std::vector<Capability> requirements;
        std::string detail{"Not started"};
        std::uint64_t startAttempts{};
    };

    struct BackendSnapshot final
    {
        bool initialized{};
        std::uint64_t tickSequence{};
        CapabilitySnapshot capabilities;
        GameContextSnapshot context;
        std::vector<FeatureHealthSnapshot> features;
    };
}
