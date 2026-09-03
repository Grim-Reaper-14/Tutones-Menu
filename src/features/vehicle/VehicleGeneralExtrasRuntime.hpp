#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace Tutones::Game::Mods
{
    class VehicleGeneralExtrasRuntime final
    {
    public:
        struct EnterLastVehicleSnapshot final
        {
            bool pending{};
            bool haveResult{};
            bool succeeded{};
            Vehicle vehicle{};
            std::string message{};
        };

        static VehicleGeneralExtrasRuntime& Get() noexcept
        {
            static VehicleGeneralExtrasRuntime instance;
            return instance;
        }

        [[nodiscard]] bool KeepVehicleFixed() const noexcept
        {
            return m_KeepVehicleFixed.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool Seatbelt() const noexcept
        {
            return m_Seatbelt.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool KeepEngineRunning() const noexcept
        {
            return m_KeepEngineRunning.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool KeepHeadlightsOn() const noexcept
        {
            return m_KeepHeadlightsOn.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool HighBeams() const noexcept
        {
            return m_HighBeams.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool AllowHatsInVehicles() const noexcept
        {
            return m_AllowHatsInVehicles.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool SpeedReadout() const noexcept
        {
            return m_SpeedReadout.load(std::memory_order_acquire);
        }

        [[nodiscard]] float SpeedMetersPerSecond() const noexcept
        {
            return m_SpeedMetersPerSecond.load(std::memory_order_acquire);
        }

        [[nodiscard]] float SpeedKph() const noexcept
        {
            return SpeedMetersPerSecond() * 3.6f;
        }

        [[nodiscard]] float SpeedMph() const noexcept
        {
            return SpeedMetersPerSecond() * 2.23693629f;
        }

        [[nodiscard]] EnterLastVehicleSnapshot EnterLastVehicleStatus() const
        {
            std::scoped_lock lock(m_EnterLastVehicleMutex);
            return m_EnterLastVehicleSnapshot;
        }

        void SetKeepVehicleFixed(bool enabled) noexcept
        {
            m_KeepVehicleFixed.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetSeatbelt(bool enabled) noexcept
        {
            m_Seatbelt.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }
            QueueCleanup([this] { RestoreSeatbelt(); });
        }

        void SetKeepEngineRunning(bool enabled) noexcept
        {
            m_KeepEngineRunning.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }
            QueueCleanup([this] { RestoreEnginePedFlag(); });
        }

        void SetKeepHeadlightsOn(bool enabled) noexcept
        {
            m_KeepHeadlightsOn.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }
            if (!m_HighBeams.load(std::memory_order_acquire))
                QueueCleanup([this] { RestoreLights(); });
        }

        void SetHighBeams(bool enabled) noexcept
        {
            m_HighBeams.store(enabled, std::memory_order_release);
            if (enabled)
            {
                EnsureTicking();
                return;
            }
            if (!m_KeepHeadlightsOn.load(std::memory_order_acquire))
                QueueCleanup([this] { RestoreLights(); });
        }

        void SetAllowHatsInVehicles(bool enabled) noexcept
        {
            m_AllowHatsInVehicles.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
        }

        void SetSpeedReadout(bool enabled) noexcept
        {
            m_SpeedReadout.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureTicking();
            else
                m_SpeedMetersPerSecond.store(0.0f, std::memory_order_release);
        }

        bool QueueEnterLastVehicle() noexcept
        {
            bool expected = false;
            if (!m_EnterLastVehiclePending.compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel))
            {
                return false;
            }

            PublishEnterLastVehiclePending("Resolving the last occupied vehicle...");
            if (QueueAction([this] { BeginEnterLastVehicle(); }))
                return true;

            FinishEnterLastVehicle(false, 0, "GTA script-thread queue is unavailable");
            return false;
        }

        bool QueueSetEngine(bool enabled) noexcept
        {
            return QueueAction([this, enabled] {
                const Vehicle vehicle = CurrentVehicle();
                if (vehicle != 0)
                    static_cast<void>(SetVehicleEngineOn(vehicle, enabled, true, !enabled));
            });
        }

        bool QueueSetDoorsLocked(bool locked) noexcept
        {
            return QueueAction([this, locked] {
                const Vehicle vehicle = CurrentVehicle();
                if (vehicle != 0)
                    static_cast<void>(SetVehicleDoorsLocked(vehicle, locked ? 2 : 1));
            });
        }

        void Shutdown() noexcept
        {
            m_KeepVehicleFixed.store(false, std::memory_order_release);
            m_Seatbelt.store(false, std::memory_order_release);
            m_KeepEngineRunning.store(false, std::memory_order_release);
            m_KeepHeadlightsOn.store(false, std::memory_order_release);
            m_HighBeams.store(false, std::memory_order_release);
            m_AllowHatsInVehicles.store(false, std::memory_order_release);
            m_SpeedReadout.store(false, std::memory_order_release);
            m_SpeedMetersPerSecond.store(0.0f, std::memory_order_release);
            m_EnterLastVehiclePending.store(false, std::memory_order_release);

            {
                std::scoped_lock lock(m_EnterLastVehicleMutex);
                m_EnterLastVehicleSnapshot = {};
            }

            const auto cleanup = [this] {
                RestoreSeatbelt();
                RestoreEnginePedFlag();
                RestoreLights();
            };

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                cleanup();
            }
            else if (runtime.IsInitialized())
            {
                const auto cleaned = std::make_shared<std::atomic<bool>>(false);
                if (runtime.Enqueue([cleanup, cleaned] {
                        cleanup();
                        cleaned->store(true, std::memory_order_release);
                    }))
                {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                    while (!cleaned->load(std::memory_order_acquire)
                        && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            }

            m_Ticking.store(false, std::memory_order_release);
        }

    private:
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            SetPedConfigFlag,
            SetPedCanBeKnockedOffVehicle,
            SetPedResetFlag,
            SetVehicleEngineOnHandler,
            SetVehicleLightsHandler,
            SetVehicleFullbeamHandler,
            SetVehicleDoorsLockedHandler,
            GetEntitySpeedHandler,
            GetDamageDecalsHandler,
            RemoveDecalsHandler,
            SetEngineHealthHandler,
            SetPetrolTankHealthHandler,
            ForceEntityUpdateHandler,
            HandlerCount,
        };

        // Current GTA V Enhanced hashes verified against YimMenuV2's enhanced crossmap.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0x0428AFDCAA63B06Eull, // SET_PED_CONFIG_FLAG
            0x68F395D64BC35E68ull, // SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE
            0x0FB8E752BCC547A9ull, // SET_PED_RESET_FLAG
            0xC229299217554C78ull, // SET_VEHICLE_ENGINE_ON
            0xBA3C1A9AA7FD9616ull, // SET_VEHICLE_LIGHTS
            0x2F12C305B28C6C59ull, // SET_VEHICLE_FULLBEAM
            0x0B74F181ADFC39BFull, // SET_VEHICLE_DOORS_LOCKED
            0xDF93B3CFAC96698Full, // GET_ENTITY_SPEED
            0xB69AE16F62A14003ull, // GET_DOES_VEHICLE_HAVE_DAMAGE_DECALS
            0xFEC8EAE457274AD3ull, // REMOVE_DECALS_FROM_VEHICLE
            0x2AEBE39F6BF7D6BCull, // SET_VEHICLE_ENGINE_HEALTH
            0xDF9DC0584881B7AFull, // SET_VEHICLE_PETROL_TANK_HEALTH
            0x2B2ECB6F6371E59Eull, // FORCE_ENTITY_AI_AND_ANIMATION_UPDATE
        };

        static constexpr int WillFlyThroughWindscreenFlag = 32;
        static constexpr int LeaveEngineOnWhenExitingVehiclesFlag = 241;
        static constexpr int KeepHatInVehicleResetFlag = 337;
        static constexpr int KnockOffNever = 1;
        static constexpr int KnockOffDefault = 0;
        static constexpr int LightsNormal = 0;
        static constexpr int LightsForcedOn = 2;

        VehicleGeneralExtrasRuntime() = default;
        ~VehicleGeneralExtrasRuntime() = default;
        VehicleGeneralExtrasRuntime(const VehicleGeneralExtrasRuntime&) = delete;
        VehicleGeneralExtrasRuntime& operator=(const VehicleGeneralExtrasRuntime&) = delete;

        [[nodiscard]] bool AnyEnabled() const noexcept
        {
            return m_KeepVehicleFixed.load(std::memory_order_acquire)
                || m_Seatbelt.load(std::memory_order_acquire)
                || m_KeepEngineRunning.load(std::memory_order_acquire)
                || m_KeepHeadlightsOn.load(std::memory_order_acquire)
                || m_HighBeams.load(std::memory_order_acquire)
                || m_AllowHatsInVehicles.load(std::memory_order_acquire)
                || m_SpeedReadout.load(std::memory_order_acquire);
        }

        [[nodiscard]] static bool IsExecutableAddress(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                const auto address = static_cast<std::uintptr_t>(slots[i]);
                if (!IsExecutableAddress(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
            }
            return true;
        }

        [[nodiscard]] Ped CurrentPed() const noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return 0;
            const auto ped = Natives::PlayerPedId();
            return ped ? *ped : 0;
        }

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept
        {
            const Ped ped = CurrentPed();
            if (ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = Natives::GetVehiclePedIsIn(ped, false);
            if (!vehicle || *vehicle == 0)
                return 0;

            const auto exists = Natives::DoesEntityExist(*vehicle);
            return exists && *exists ? *vehicle : 0;
        }

        [[nodiscard]] bool SetPedConfig(Ped ped, int flag, bool value) noexcept
        {
            if (ped == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(ped) || !context.PushArg(flag) || !context.PushArg(static_cast<std::int32_t>(value)))
                return false;
            m_Handlers[SetPedConfigFlag](&context);
            return true;
        }

        [[nodiscard]] bool SetPedKnockOff(Ped ped, int state) noexcept
        {
            if (ped == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(ped) || !context.PushArg(state))
                return false;
            m_Handlers[SetPedCanBeKnockedOffVehicle](&context);
            return true;
        }

        [[nodiscard]] bool SetPedReset(Ped ped, int flag, bool value) noexcept
        {
            if (ped == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(ped) || !context.PushArg(flag) || !context.PushArg(static_cast<std::int32_t>(value)))
                return false;
            m_Handlers[SetPedResetFlag](&context);
            return true;
        }

        [[nodiscard]] bool SetVehicleEngineOn(Vehicle vehicle, bool enabled, bool instantly, bool disableAutoStart) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle)
                || !context.PushArg(static_cast<std::int32_t>(enabled))
                || !context.PushArg(static_cast<std::int32_t>(instantly))
                || !context.PushArg(static_cast<std::int32_t>(disableAutoStart)))
                return false;
            m_Handlers[SetVehicleEngineOnHandler](&context);
            return true;
        }

        [[nodiscard]] bool SetVehicleLights(Vehicle vehicle, int state) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(state))
                return false;
            m_Handlers[SetVehicleLightsHandler](&context);
            return true;
        }

        [[nodiscard]] bool SetVehicleFullbeam(Vehicle vehicle, bool enabled) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(static_cast<std::int32_t>(enabled)))
                return false;
            m_Handlers[SetVehicleFullbeamHandler](&context);
            return true;
        }

        [[nodiscard]] bool SetVehicleDoorsLocked(Vehicle vehicle, int state) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(state))
                return false;
            m_Handlers[SetVehicleDoorsLockedHandler](&context);
            return true;
        }

        [[nodiscard]] std::optional<float> GetEntitySpeed(Entity entity) noexcept
        {
            if (entity == 0 || !ResolveHandlers())
                return std::nullopt;
            Native::CallContext context;
            if (!context.PushArg(entity))
                return std::nullopt;
            m_Handlers[GetEntitySpeedHandler](&context);
            return context.GetReturnValue<float>();
        }

        [[nodiscard]] std::optional<bool> HasDamageDecals(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return std::nullopt;
            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return std::nullopt;
            m_Handlers[GetDamageDecalsHandler](&context);
            return context.GetReturnValue<std::int32_t>() != 0;
        }

        [[nodiscard]] bool RemoveDecals(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle))
                return false;
            m_Handlers[RemoveDecalsHandler](&context);
            return true;
        }

        [[nodiscard]] bool SetEngineHealth(Vehicle vehicle, float health) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(health))
                return false;
            m_Handlers[SetEngineHealthHandler](&context);
            return true;
        }

        [[nodiscard]] bool SetPetrolTankHealth(Vehicle vehicle, float health) noexcept
        {
            if (vehicle == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(vehicle) || !context.PushArg(health))
                return false;
            m_Handlers[SetPetrolTankHealthHandler](&context);
            return true;
        }

        [[nodiscard]] bool ForceEntityUpdate(Entity entity) noexcept
        {
            if (entity == 0 || !ResolveHandlers())
                return false;
            Native::CallContext context;
            if (!context.PushArg(entity))
                return false;
            m_Handlers[ForceEntityUpdateHandler](&context);
            return true;
        }

        void PublishEnterLastVehiclePending(std::string message) noexcept
        {
            std::scoped_lock lock(m_EnterLastVehicleMutex);
            m_EnterLastVehicleSnapshot.pending = true;
            m_EnterLastVehicleSnapshot.haveResult = false;
            m_EnterLastVehicleSnapshot.succeeded = false;
            m_EnterLastVehicleSnapshot.vehicle = m_PendingEnterLastVehicle;
            m_EnterLastVehicleSnapshot.message = std::move(message);
        }

        void FinishEnterLastVehicle(bool success, Vehicle vehicle, std::string message) noexcept
        {
            m_PendingEnterLastPed = 0;
            m_PendingEnterLastVehicle = 0;
            m_EnterLastVehicleDeadline = {};
            m_NextEnterLastVehicleAttempt = {};
            m_EnterLastVehiclePending.store(false, std::memory_order_release);

            {
                std::scoped_lock lock(m_EnterLastVehicleMutex);
                m_EnterLastVehicleSnapshot.pending = false;
                m_EnterLastVehicleSnapshot.haveResult = true;
                m_EnterLastVehicleSnapshot.succeeded = success;
                m_EnterLastVehicleSnapshot.vehicle = vehicle;
                m_EnterLastVehicleSnapshot.message = message;
            }

            const std::string logMessage = std::string(success ? "Succeeded: " : "Failed: ") + message;
            if (success)
                TUTONES_LOG_INFO("vehicle.enter_last", logMessage);
            else
                TUTONES_LOG_WARN("vehicle.enter_last", logMessage);
        }

        void BeginEnterLastVehicle() noexcept
        {
            if (!m_EnterLastVehiclePending.load(std::memory_order_acquire))
                return;

            const Ped ped = CurrentPed();
            if (ped == 0)
            {
                FinishEnterLastVehicle(false, 0, "The local player ped is unavailable");
                return;
            }

            const auto inVehicle = Natives::IsPedInAnyVehicle(ped, false);
            if (!inVehicle)
            {
                FinishEnterLastVehicle(false, 0, "Could not read the local player's vehicle state");
                return;
            }
            if (*inVehicle)
            {
                FinishEnterLastVehicle(false, 0, "Exit the current vehicle before using Enter Last Vehicle");
                return;
            }

            // GET_VEHICLE_PED_IS_IN(ped, true) returns the ped's last vehicle.
            const auto vehicle = Natives::GetVehiclePedIsIn(ped, true);
            if (!vehicle || *vehicle == 0)
            {
                FinishEnterLastVehicle(false, 0, "GTA does not have a last occupied vehicle for the local player");
                return;
            }

            const auto exists = Natives::DoesEntityExist(*vehicle);
            if (!exists || !*exists)
            {
                FinishEnterLastVehicle(false, *vehicle, "The last occupied vehicle no longer exists");
                return;
            }

            const auto driver = Native::NativeInvoker::Invoke<Ped>(
                Native::NativeId::GetPedInVehicleSeat,
                *vehicle,
                std::int32_t{-1},
                std::int32_t{0});
            if (!driver)
            {
                FinishEnterLastVehicle(false, *vehicle, "Could not read the last vehicle's driver seat");
                return;
            }
            if (*driver != 0 && *driver != ped)
            {
                FinishEnterLastVehicle(false, *vehicle, "The last vehicle's driver seat is occupied");
                return;
            }

            m_PendingEnterLastPed = ped;
            m_PendingEnterLastVehicle = *vehicle;
            m_EnterLastVehicleDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            m_NextEnterLastVehicleAttempt = {};
            PublishEnterLastVehiclePending("Entering the last vehicle and verifying the driver seat...");
            ContinueEnterLastVehicle();
        }

        void ContinueEnterLastVehicle() noexcept
        {
            if (!m_EnterLastVehiclePending.load(std::memory_order_acquire))
                return;

            const Ped ped = m_PendingEnterLastPed;
            const Vehicle vehicle = m_PendingEnterLastVehicle;
            if (ped == 0 || vehicle == 0 || CurrentPed() != ped)
            {
                FinishEnterLastVehicle(false, vehicle, "The local player changed while entering the last vehicle");
                return;
            }

            const auto exists = Natives::DoesEntityExist(vehicle);
            if (!exists || !*exists)
            {
                FinishEnterLastVehicle(false, vehicle, "The last occupied vehicle disappeared before entry completed");
                return;
            }

            const auto driver = Native::NativeInvoker::Invoke<Ped>(
                Native::NativeId::GetPedInVehicleSeat,
                vehicle,
                std::int32_t{-1},
                std::int32_t{0});
            if (!driver)
            {
                FinishEnterLastVehicle(false, vehicle, "Could not verify the last vehicle's driver seat");
                return;
            }

            const auto currentVehicle = Natives::GetVehiclePedIsIn(ped, false);
            if (currentVehicle && *currentVehicle == vehicle && *driver == ped)
            {
                FinishEnterLastVehicle(true, vehicle, "Entered the last vehicle and verified the driver seat");
                return;
            }
            if (*driver != 0 && *driver != ped)
            {
                FinishEnterLastVehicle(false, vehicle, "The last vehicle's driver seat became occupied");
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now >= m_EnterLastVehicleDeadline)
            {
                FinishEnterLastVehicle(false, vehicle, "GTA did not confirm entry into the last vehicle within two seconds");
                return;
            }

            if (now >= m_NextEnterLastVehicleAttempt)
            {
                static_cast<void>(VehicleNatives::SetPedIntoVehicle(ped, vehicle, -1));
                m_NextEnterLastVehicleAttempt = now + std::chrono::milliseconds(100);
            }

            if (!QueueAction([this] { ContinueEnterLastVehicle(); }))
                FinishEnterLastVehicle(false, vehicle, "GTA script-thread queue stopped before entry could be verified");
        }

        void ApplyKeepFixed(Vehicle vehicle) noexcept
        {
            if (vehicle == 0)
                return;

            const auto damaged = HasDamageDecals(vehicle);
            if (damaged && !*damaged)
                return;

            static_cast<void>(Natives::SetVehicleFixed(vehicle));
            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityHealth,
                vehicle,
                1000,
                std::int32_t{0},
                std::uint32_t{0}));
            static_cast<void>(SetEngineHealth(vehicle, 1000.0f));
            static_cast<void>(SetPetrolTankHealth(vehicle, 1000.0f));
            static_cast<void>(Natives::SetVehicleDirtLevel(vehicle, 0.0f));
            static_cast<void>(RemoveDecals(vehicle));
            static_cast<void>(ForceEntityUpdate(vehicle));
        }

        void ApplySeatbelt(Ped ped, bool enabled) noexcept
        {
            if (ped == 0)
                return;
            static_cast<void>(SetPedConfig(ped, WillFlyThroughWindscreenFlag, !enabled));
            static_cast<void>(SetPedKnockOff(ped, enabled ? KnockOffNever : KnockOffDefault));
        }

        void RestoreSeatbelt() noexcept
        {
            if (m_LastSeatbeltPed != 0)
            {
                static_cast<void>(ApplySeatbelt(m_LastSeatbeltPed, false));
                m_LastSeatbeltPed = 0;
            }
        }

        void RestoreEnginePedFlag() noexcept
        {
            if (m_LastEnginePed != 0)
            {
                static_cast<void>(SetPedConfig(m_LastEnginePed, LeaveEngineOnWhenExitingVehiclesFlag, false));
                m_LastEnginePed = 0;
            }
        }

        void RestoreLights() noexcept
        {
            if (m_LastLightsVehicle != 0)
            {
                const auto exists = Natives::DoesEntityExist(m_LastLightsVehicle);
                if (exists && *exists)
                {
                    static_cast<void>(SetVehicleFullbeam(m_LastLightsVehicle, false));
                    static_cast<void>(SetVehicleLights(m_LastLightsVehicle, LightsNormal));
                }
                m_LastLightsVehicle = 0;
            }
        }

        template<typename Task>
        bool QueueAction(Task&& task) noexcept
        {
            auto& runtime = Runtime::GameRuntime::Get();
            if (!runtime.IsInitialized())
                return false;
            return runtime.Enqueue(std::forward<Task>(task));
        }

        template<typename Task>
        void QueueCleanup(Task&& task) noexcept
        {
            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                task();
                return;
            }
            if (runtime.IsInitialized())
                static_cast<void>(runtime.Enqueue(std::forward<Task>(task)));
        }

        void EnsureTicking() noexcept
        {
            if (!AnyEnabled())
                return;

            bool expected = false;
            if (!m_Ticking.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            if (!Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                m_Ticking.store(false, std::memory_order_release);
        }

        void TickOnGameThread() noexcept
        {
            if (!AnyEnabled())
            {
                m_SpeedMetersPerSecond.store(0.0f, std::memory_order_release);
                m_Ticking.store(false, std::memory_order_release);
                return;
            }

            const Ped ped = CurrentPed();
            const Vehicle vehicle = CurrentVehicle();

            if (m_KeepVehicleFixed.load(std::memory_order_acquire) && vehicle != 0)
                ApplyKeepFixed(vehicle);

            if (m_Seatbelt.load(std::memory_order_acquire) && ped != 0)
            {
                if (m_LastSeatbeltPed != 0 && m_LastSeatbeltPed != ped)
                    RestoreSeatbelt();
                ApplySeatbelt(ped, true);
                m_LastSeatbeltPed = ped;
            }

            if (m_KeepEngineRunning.load(std::memory_order_acquire) && ped != 0)
            {
                if (m_LastEnginePed != 0 && m_LastEnginePed != ped)
                    RestoreEnginePedFlag();
                static_cast<void>(SetPedConfig(ped, LeaveEngineOnWhenExitingVehiclesFlag, true));
                m_LastEnginePed = ped;
                if (vehicle != 0)
                    static_cast<void>(SetVehicleEngineOn(vehicle, true, true, false));
            }

            if (m_AllowHatsInVehicles.load(std::memory_order_acquire) && ped != 0)
                static_cast<void>(SetPedReset(ped, KeepHatInVehicleResetFlag, true));

            const bool forceLights = m_KeepHeadlightsOn.load(std::memory_order_acquire);
            const bool highBeams = m_HighBeams.load(std::memory_order_acquire);
            if ((forceLights || highBeams) && vehicle != 0)
            {
                if (m_LastLightsVehicle != 0 && m_LastLightsVehicle != vehicle)
                    RestoreLights();
                if (forceLights)
                    static_cast<void>(SetVehicleLights(vehicle, LightsForcedOn));
                static_cast<void>(SetVehicleFullbeam(vehicle, highBeams));
                m_LastLightsVehicle = vehicle;
            }

            if (m_SpeedReadout.load(std::memory_order_acquire) && vehicle != 0)
                m_SpeedMetersPerSecond.store(GetEntitySpeed(vehicle).value_or(0.0f), std::memory_order_release);
            else
                m_SpeedMetersPerSecond.store(0.0f, std::memory_order_release);

            if (!Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                m_Ticking.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_KeepVehicleFixed{false};
        std::atomic<bool> m_Seatbelt{false};
        std::atomic<bool> m_KeepEngineRunning{false};
        std::atomic<bool> m_KeepHeadlightsOn{false};
        std::atomic<bool> m_HighBeams{false};
        std::atomic<bool> m_AllowHatsInVehicles{false};
        std::atomic<bool> m_SpeedReadout{false};
        std::atomic<bool> m_Ticking{false};
        std::atomic<float> m_SpeedMetersPerSecond{0.0f};
        std::atomic<bool> m_EnterLastVehiclePending{false};
        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        mutable std::mutex m_EnterLastVehicleMutex;
        EnterLastVehicleSnapshot m_EnterLastVehicleSnapshot{};
        Ped m_PendingEnterLastPed{};
        Vehicle m_PendingEnterLastVehicle{};
        std::chrono::steady_clock::time_point m_EnterLastVehicleDeadline{};
        std::chrono::steady_clock::time_point m_NextEnterLastVehicleAttempt{};
        Ped m_LastSeatbeltPed{};
        Ped m_LastEnginePed{};
        Vehicle m_LastLightsVehicle{};
    };
}
