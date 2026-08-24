#include "BackendHub.hpp"

#include "../core/logging/Logger.hpp"
#include "../features/network/NetworkRuntime.hpp"
#include "../features/player/OffRadarRuntime.hpp"
#include "../features/player/PlayerRuntime.hpp"
#include "../features/recovery/RecoveryRuntime.hpp"
#include "../features/vehicle/DlcVehicleRuntime.hpp"
#include "../features/vehicle/LscBypassRuntime.hpp"
#include "../features/vehicle/PersonalVehicleRuntime.hpp"
#include "../features/vehicle/VehicleModificationRuntime.hpp"
#include "../features/vehicle/VehiclePaintRuntime.hpp"
#include "../features/weapon/WeaponRuntime.hpp"
#include "../game/GameState.hpp"
#include "../game/tunables/TunableRegistry.hpp"
#include "../runtime/GameRuntime.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <utility>

namespace Tutones::Backend
{
    BackendHub& BackendHub::Get() noexcept
    {
        static BackendHub instance;
        return instance;
    }

    bool BackendHub::Initialize()
    {
        bool expected = false;
        if (!m_Initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        m_TickSequence.store(0, std::memory_order_release);
        m_ActiveTicks.store(0, std::memory_order_release);
        RegisterBuiltinFeatures();
        m_Capabilities.Refresh();
        RefreshContext();
        m_Features.StartEligible(m_Capabilities);

        if (!QueueNextTick())
        {
            TUTONES_LOG_ERROR("backend.hub", "Could not queue the first BackendHub heartbeat");
            m_Initialized.store(false, std::memory_order_release);
            m_Features.StopAll();
            m_Capabilities.Reset();
            return false;
        }

        const auto features = m_Features.Snapshot();
        std::size_t healthy{};
        std::size_t waiting{};
        std::size_t faulted{};
        for (const auto& feature : features)
        {
            switch (feature.state)
            {
            case RuntimeState::Healthy: ++healthy; break;
            case RuntimeState::WaitingForDependency: ++waiting; break;
            case RuntimeState::Faulted: ++faulted; break;
            default: break;
            }
        }

        TUTONES_LOG_INFO(
            "backend.hub",
            std::string("BackendHub initialized; features healthy=") + std::to_string(healthy)
                + ", waiting=" + std::to_string(waiting)
                + ", faulted=" + std::to_string(faulted));
        return true;
    }

    void BackendHub::Shutdown() noexcept
    {
        if (!m_Initialized.exchange(false, std::memory_order_acq_rel))
            return;

        TUTONES_LOG_INFO("backend.hub", "Stopping BackendHub feature orchestration");

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (m_ActiveTicks.load(std::memory_order_acquire) != 0
            && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (m_ActiveTicks.load(std::memory_order_acquire) != 0)
            TUTONES_LOG_WARN("backend.hub", "Timed out waiting for BackendHub heartbeat drain");

        m_Features.StopAll();
        m_Capabilities.Reset();
        {
            std::scoped_lock lock(m_ContextMutex);
            m_Context = {};
        }
        m_TickSequence.store(0, std::memory_order_release);
        m_ActiveTicks.store(0, std::memory_order_release);
        TUTONES_LOG_INFO("backend.hub", "BackendHub stopped");
    }

    void BackendHub::TickOnGameThread() noexcept
    {
        if (!IsInitialized())
            return;

        m_ActiveTicks.fetch_add(1, std::memory_order_acq_rel);
        if (!IsInitialized())
        {
            m_ActiveTicks.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }

        m_Capabilities.Refresh();
        RefreshContext();
        const auto context = Context();
        m_Features.Tick(context, m_Capabilities, MonotonicMs());
        m_TickSequence.fetch_add(1, std::memory_order_acq_rel);

        if (IsInitialized() && !QueueNextTick())
            TUTONES_LOG_ERROR("backend.hub", "BackendHub heartbeat could not requeue itself");

        m_ActiveTicks.fetch_sub(1, std::memory_order_acq_rel);
    }

    bool BackendHub::IsInitialized() const noexcept
    {
        return m_Initialized.load(std::memory_order_acquire);
    }

    bool BackendHub::RegisterFeature(FeatureDescriptor descriptor)
    {
        if (!m_Features.Register(std::move(descriptor)))
            return false;

        if (IsInitialized())
        {
            m_Capabilities.Refresh();
            m_Features.StartEligible(m_Capabilities);
        }
        return true;
    }

    bool BackendHub::RetryFeature(const std::string& id) noexcept
    {
        if (!m_Features.Retry(id))
            return false;
        if (IsInitialized())
        {
            m_Capabilities.Refresh();
            m_Features.StartEligible(m_Capabilities);
        }
        return true;
    }

    bool BackendHub::CanRun(const std::vector<Capability>& capabilities) const noexcept
    {
        return m_Capabilities.HasAll(capabilities);
    }

    BackendSnapshot BackendHub::Snapshot() const
    {
        BackendSnapshot snapshot;
        snapshot.initialized = IsInitialized();
        snapshot.tickSequence = m_TickSequence.load(std::memory_order_acquire);
        snapshot.capabilities = m_Capabilities.Snapshot();
        snapshot.context = Context();
        snapshot.features = m_Features.Snapshot();
        return snapshot;
    }

    GameContextSnapshot BackendHub::Context() const noexcept
    {
        std::scoped_lock lock(m_ContextMutex);
        return m_Context;
    }

    CapabilityRegistry& BackendHub::Capabilities() noexcept
    {
        return m_Capabilities;
    }

    const CapabilityRegistry& BackendHub::Capabilities() const noexcept
    {
        return m_Capabilities;
    }

    FeatureRegistry& BackendHub::Features() noexcept
    {
        return m_Features;
    }

    const FeatureRegistry& BackendHub::Features() const noexcept
    {
        return m_Features;
    }

    ScriptGateway& BackendHub::Scripts() noexcept
    {
        return m_Scripts;
    }

    const ScriptGateway& BackendHub::Scripts() const noexcept
    {
        return m_Scripts;
    }

    void BackendHub::RegisterBuiltinFeatures()
    {
        if (m_BuiltinsRegistered)
            return;
        m_BuiltinsRegistered = true;

        const std::vector<Capability> gameRuntime{Capability::GameRuntime};
        const auto registerRuntime = [this, &gameRuntime](
            const char* id,
            const char* name,
            const char* category,
            auto start,
            auto stop) {
            FeatureDescriptor descriptor;
            descriptor.id = id;
            descriptor.displayName = name;
            descriptor.category = category;
            descriptor.tickRate = TickRate::OnDemand;
            descriptor.requirements = gameRuntime;
            descriptor.start = std::move(start);
            descriptor.stop = std::move(stop);
            static_cast<void>(m_Features.Register(std::move(descriptor)));
        };

        registerRuntime(
            "game.tunables",
            "Tunable registry",
            "Game",
            [] { return Game::Tunables::TunableRegistry::Get().Start(); },
            [] { Game::Tunables::TunableRegistry::Get().Stop(); });

        {
            FeatureDescriptor descriptor;
            descriptor.id = "vehicle.dlc_websites";
            descriptor.displayName = "DLC vehicle websites";
            descriptor.category = "Vehicle";
            descriptor.tickRate = TickRate::OnDemand;
            descriptor.requirements = {Capability::ScriptRuntime, Capability::ScriptVm};
            descriptor.start = [] { return Game::VehicleFeatures::DlcVehicleRuntime::Get().Start(); };
            descriptor.stop = [] { Game::VehicleFeatures::DlcVehicleRuntime::Get().Stop(); };
            static_cast<void>(m_Features.Register(std::move(descriptor)));
        }

        registerRuntime(
            "vehicle.lsc",
            "LSC restriction bypass",
            "Vehicle",
            [] { return Game::Mods::LscBypassRuntime::Get().Start(); },
            [] { Game::Mods::LscBypassRuntime::Get().Stop(); });
        registerRuntime(
            "vehicle.paint",
            "Vehicle paint",
            "Vehicle",
            [] { return Game::Paint::VehiclePaintRuntime::Get().Start(); },
            [] { Game::Paint::VehiclePaintRuntime::Get().Stop(); });
        registerRuntime(
            "vehicle.modifications",
            "Vehicle modifications",
            "Vehicle",
            [] { return Game::Mods::VehicleModificationRuntime::Get().Start(); },
            [] { Game::Mods::VehicleModificationRuntime::Get().Stop(); });
        registerRuntime(
            "vehicle.personal",
            "Personal vehicle reader",
            "Vehicle",
            [] { return Game::PersonalVehicles::PersonalVehicleRuntime::Get().Start(); },
            [] { Game::PersonalVehicles::PersonalVehicleRuntime::Get().Stop(); });
        registerRuntime(
            "player.core",
            "Player runtime",
            "Player",
            [] { return Game::PlayerFeatures::PlayerRuntime::Get().Start(); },
            [] { Game::PlayerFeatures::PlayerRuntime::Get().Stop(); });
        registerRuntime(
            "player.off_radar",
            "Off Radar",
            "Player",
            [] { return Game::PlayerFeatures::OffRadarRuntime::Get().Start(); },
            [] { Game::PlayerFeatures::OffRadarRuntime::Get().Stop(); });
        registerRuntime(
            "weapons.core",
            "Weapon runtime",
            "Weapons",
            [] { return Game::WeaponFeatures::WeaponRuntime::Get().Start(); },
            [] { Game::WeaponFeatures::WeaponRuntime::Get().Stop(); });
        registerRuntime(
            "recovery.core",
            "Recovery runtime",
            "Recovery",
            [] { return Game::Recovery::RecoveryRuntime::Get().Start(); },
            [] { Game::Recovery::RecoveryRuntime::Get().Stop(); });
        registerRuntime(
            "network.core",
            "Network/QoL runtime",
            "Network",
            [] { return Game::NetworkFeatures::NetworkRuntime::Get().Start(); },
            [] { Game::NetworkFeatures::NetworkRuntime::Get().Stop(); });
    }

    void BackendHub::RefreshContext() noexcept
    {
        const auto capabilities = m_Capabilities.Snapshot();
        const auto game = Game::GameState::Get().Snapshot();

        GameContextSnapshot context;
        context.coreReady = capabilities.Has(Capability::CoreServices);
        context.gameRuntimeReady = capabilities.Has(Capability::GameRuntime);
        context.nativeRuntimeReady = capabilities.Has(Capability::NativeRuntime);
        context.scriptRuntimeReady = capabilities.Has(Capability::ScriptRuntime);
        context.scriptGlobalsReady = capabilities.Has(Capability::ScriptGlobals);
        context.scriptVmReady = capabilities.Has(Capability::ScriptVm);
        context.sessionStarted = capabilities.Has(Capability::OnlineSession);
        context.freemodeRunning = capabilities.Has(Capability::FreemodeScript);
        context.vehicleRewardRunning = capabilities.Has(Capability::VehicleRewardScript);
        context.playerPed = game.playerPed;
        context.inVehicle = game.inVehicle;
        context.vehicle = game.vehicle;
        context.vehicleModel = game.vehicleModel;
        context.gameStateSequence = game.sequence;
        context.hubSequence = m_TickSequence.load(std::memory_order_acquire) + 1;

        std::scoped_lock lock(m_ContextMutex);
        m_Context = context;
    }

    bool BackendHub::QueueNextTick() noexcept
    {
        if (!IsInitialized())
            return false;

        return Runtime::GameRuntime::Get().Enqueue([] {
            BackendHub::Get().TickOnGameThread();
        });
    }

    std::uint64_t BackendHub::MonotonicMs() noexcept
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}
