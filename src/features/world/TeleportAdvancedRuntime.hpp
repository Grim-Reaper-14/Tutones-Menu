#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/PlayerNatives.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::World
{
    struct AdvancedTeleportSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class TeleportAdvancedRuntime final
    {
    public:
        static TeleportAdvancedRuntime& Get() noexcept
        {
            static TeleportAdvancedRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueObjective()
        {
            if (!CanQueue())
                return false;
            SetPending("Objective teleport queued");
            return QueueOrFail([this] { TeleportObjective(); });
        }

        [[nodiscard]] bool QueueDirectional(float right, float forward, float up)
        {
            if (!std::isfinite(right) || !std::isfinite(forward) || !std::isfinite(up) || !CanQueue())
                return false;
            SetPending("Directional teleport queued");
            return QueueOrFail([this, right, forward, up] { TeleportDirectional(right, forward, up); });
        }

        [[nodiscard]] AdvancedTeleportSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.pending = m_Pending.load(std::memory_order_acquire);
            return out;
        }

    private:
        enum HandlerIndex : std::size_t
        {
            GetClosestBlip,
            DoesBlipExist,
            GetBlipCoords,
            GetOffsetFromEntity,
            SetEntityCoords,
            RequestCollision,
            GetGroundZ,
            GetWaterHeight,
            GetApproxHeight,
            HandlerCount,
        };

        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xB981254932E1095Eull, // GET_CLOSEST_BLIP_INFO_ID
            0xC450B06E5AAA0985ull, // DOES_BLIP_EXIST
            0x7DFE6973AE84B6EDull, // GET_BLIP_COORDS
            0x0D1381B6E0F3987Dull, // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
            0xEA2D52183C7EA9CFull, // REQUEST_COLLISION_AT_COORD
            0xB1EAADCB692D69CEull, // GET_GROUND_Z_FOR_3D_COORD
            0xF85C2BE613AD7903ull, // GET_WATER_HEIGHT
            0x54D01A0F98391D5Bull, // GET_APPROX_HEIGHT_FOR_POINT
        };

        static constexpr std::array<int, 17> ObjectiveSprites{
            1, 0, 2,
            143, 144, 145, 146,
            478,
            535, 536, 537, 538, 539, 540, 541, 542,
            549,
        };

        static constexpr std::size_t MaxGroundAttempts = 20;

        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        struct TeleportTarget final
        {
            Entity entity{};
            Vehicle vehicle{};
            bool inVehicle{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        TeleportAdvancedRuntime() = default;

        [[nodiscard]] bool CanQueue()
        {
            return Native::NativeRegistry::Get().IsReady()
                && !m_Pending.exchange(true, std::memory_order_acq_rel);
        }

        template <typename Task>
        [[nodiscard]] bool QueueOrFail(Task&& task)
        {
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Task>(task)))
                return true;
            Finish(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] static bool IsExecutable(std::uintptr_t address) noexcept
        {
            if (!address)
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

        [[nodiscard]] bool ResolveHandlers()
        {
            if (m_Handlers[0])
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
                if (!IsExecutable(address))
                {
                    m_Handlers.fill(nullptr);
                    TUTONES_LOG_ERROR("world.teleport", "Enhanced advanced teleport native resolution failed");
                    Core::Logging::Logger::Get().Flush();
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
            }

            TUTONES_LOG_INFO("world.teleport", "Enhanced objective/directional safe teleport natives resolved");
            Core::Logging::Logger::Get().Flush();
            return true;
        }

        template <typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args... args) const
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;
            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        template <typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args... args) const
        {
            if (index >= m_Handlers.size() || !m_Handlers[index])
                return false;
            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_Handlers[index](&context);
            context.FixVectors();
            return true;
        }

        [[nodiscard]] TeleportTarget ResolveLocalTeleportTarget() const noexcept
        {
            TeleportTarget target{};
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return target;

            // Explicitly check vehicle state before the teleport. This keeps the ped in
            // the car and teleports the whole vehicle when the player is driving/riding.
            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, true);
            if (inVehicle && *inVehicle)
            {
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, true);
                if (vehicle && *vehicle != 0)
                {
                    const auto exists = Natives::DoesEntityExist(*vehicle);
                    if (exists && *exists)
                    {
                        target.entity = *vehicle;
                        target.vehicle = *vehicle;
                        target.inVehicle = true;
                        return target;
                    }
                }
            }

            target.entity = *ped;
            return target;
        }

        [[nodiscard]] bool MoveEntity(Entity entity, const Native::NativeVector3& coords) const
        {
            if (entity == 0 || !std::isfinite(coords.x) || !std::isfinite(coords.y) || !std::isfinite(coords.z))
                return false;

            if (!CallVoid(
                    SetEntityCoords,
                    entity,
                    coords.x,
                    coords.y,
                    coords.z,
                    std::int32_t{1},
                    std::int32_t{1},
                    std::int32_t{1}))
            {
                return false;
            }

            const auto actual = VehicleNatives::GetEntityCoords(entity, false);
            if (!actual)
                return false;

            const float dx = actual->x - coords.x;
            const float dy = actual->y - coords.y;
            const float dz = actual->z - coords.z;
            return (dx * dx + dy * dy) <= 36.0f && std::fabs(dz) <= 15.0f;
        }

        void StreamCollision(const Native::NativeVector3& coords) const
        {
            static_cast<void>(CallVoid(RequestCollision, coords.x, coords.y, coords.z));
        }

        [[nodiscard]] bool ProbeGround(float x, float y, float& out) const
        {
            if (!m_Handlers[GetGroundZ])
                return false;
            Native::CallContext context;
            if (!context.PushArg(x)
                || !context.PushArg(y)
                || !context.PushArg(1000.0f)
                || !context.PushArg(&out)
                || !context.PushArg(std::int32_t{0})
                || !context.PushArg(std::int32_t{0}))
            {
                return false;
            }
            m_Handlers[GetGroundZ](&context);
            context.FixVectors();
            return context.GetReturnValue<std::int32_t>() != 0 && std::isfinite(out);
        }

        [[nodiscard]] bool ProbeWater(const Native::NativeVector3& coords, float& out) const
        {
            if (!m_Handlers[GetWaterHeight])
                return false;
            Native::CallContext context;
            if (!context.PushArg(coords.x)
                || !context.PushArg(coords.y)
                || !context.PushArg(coords.z)
                || !context.PushArg(&out))
            {
                return false;
            }
            m_Handlers[GetWaterHeight](&context);
            context.FixVectors();
            return context.GetReturnValue<std::int32_t>() != 0 && std::isfinite(out);
        }

        [[nodiscard]] float ApproxHeight(float x, float y) const
        {
            float result{};
            if (!Call(GetApproxHeight, result, x, y) || !std::isfinite(result))
                return 50.0f;
            return result;
        }

        void BeginSafeGroundTeleport(const Native::NativeVector3& coords, std::string label)
        {
            const TeleportTarget target = ResolveLocalTeleportTarget();
            if (target.entity == 0)
                return Finish(false, "Local player/vehicle entity is unavailable");

            m_Target = target;
            m_Destination = coords;
            m_GroundAttempt = 0;
            m_PendingLabel = std::move(label);

            StreamCollision(m_Destination);
            SetPending(m_PendingLabel + (m_Target.inVehicle
                ? ": resolving safe vehicle landing"
                : ": resolving ground"));

            if (!Runtime::GameRuntime::Get().Enqueue([this] { GroundProbe(); }))
                Finish(false, "Ground-probe queue unavailable");
        }

        void GroundProbe()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;
            if (!ResolveHandlers())
                return Finish(false, "Teleport natives became unavailable");

            StreamCollision(m_Destination);

            float ground{};
            if (ProbeGround(m_Destination.x, m_Destination.y, ground))
            {
                auto destination = m_Destination;
                destination.z = ground + 1.0f;
                return FinishSafeMove(destination, m_PendingLabel + " complete", true);
            }

            if (++m_GroundAttempt < MaxGroundAttempts)
            {
                if (Runtime::GameRuntime::Get().Enqueue([this] { GroundProbe(); }))
                    return;
                return Finish(false, "Ground-probe queue unavailable");
            }

            float water{};
            if (ProbeWater(m_Destination, water))
            {
                auto destination = m_Destination;
                destination.z = water + 1.0f;
                return FinishSafeMove(destination, m_PendingLabel + " complete (water height)", false);
            }

            auto destination = m_Destination;
            destination.z = ApproxHeight(destination.x, destination.y) + 1.0f;
            FinishSafeMove(destination, m_PendingLabel + " complete (approximate height)", true);
        }

        void FinishSafeMove(const Native::NativeVector3& coords, std::string label, bool settleVehicle)
        {
            const TeleportTarget current = ResolveLocalTeleportTarget();
            if (current.entity == 0
                || current.entity != m_Target.entity
                || current.inVehicle != m_Target.inVehicle
                || (m_Target.inVehicle && current.vehicle != m_Target.vehicle))
            {
                return Finish(false, "Teleport canceled because the player/vehicle changed while resolving the destination");
            }

            bool moved = MoveEntity(m_Target.entity, coords);
            if (moved && m_Target.inVehicle && settleVehicle)
            {
                const auto grounded = Natives::SetVehicleOnGroundProperly(m_Target.vehicle, 5.0f);
                if (grounded && !*grounded)
                    moved = false;
            }

            Finish(moved, moved ? std::move(label) : "Teleport ran, but safe destination placement failed");
        }

        void TeleportObjective()
        {
            if (!ResolveHandlers())
                return Finish(false, "Objective teleport natives unavailable");

            for (const int sprite : ObjectiveSprites)
            {
                std::int32_t blip{};
                if (!Call(GetClosestBlip, blip, sprite) || blip == 0)
                    continue;

                std::int32_t exists{};
                if (!Call(DoesBlipExist, exists, blip) || exists == 0)
                    continue;

                Native::NativeVector3 coords{};
                if (!Call(GetBlipCoords, coords, blip))
                    continue;

                return BeginSafeGroundTeleport(coords, "Teleport to objective");
            }

            Finish(false, "No supported objective blip is active");
        }

        void TeleportDirectional(float right, float forward, float up)
        {
            if (!ResolveHandlers())
                return Finish(false, "Directional teleport natives unavailable");

            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return Finish(false, "Player ped unavailable");

            Native::NativeVector3 coords{};
            if (!Call(GetOffsetFromEntity, coords, *ped, right, forward, up))
                return Finish(false, "Failed to calculate directional offset");

            // Directional teleport intentionally preserves the requested vertical offset,
            // but it still checks whether the player is in a vehicle before moving.
            const TeleportTarget target = ResolveLocalTeleportTarget();
            if (target.entity == 0)
                return Finish(false, "Local player/vehicle entity is unavailable");

            const TeleportTarget current = ResolveLocalTeleportTarget();
            if (current.entity != target.entity
                || current.inVehicle != target.inVehicle
                || (target.inVehicle && current.vehicle != target.vehicle))
            {
                return Finish(false, "Teleport canceled because the player/vehicle changed");
            }

            Finish(MoveEntity(target.entity, coords), target.inVehicle
                ? "Directional vehicle teleport"
                : "Directional teleport");
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.haveResult = false;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.haveResult = true;
                m_Snapshot.lastSucceeded = success;
                m_Snapshot.message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        AdvancedTeleportSnapshot m_Snapshot{};

        TeleportTarget m_Target{};
        Native::NativeVector3 m_Destination{};
        std::size_t m_GroundAttempt{};
        std::string m_PendingLabel{"Teleport"};
    };
}
