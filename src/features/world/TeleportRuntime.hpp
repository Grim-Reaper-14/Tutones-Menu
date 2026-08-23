#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
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
#include <vector>

namespace Tutones::Game::World
{
    struct TeleportBlipGroup final
    {
        const char* label{};
        int sprite{};
        bool cycle{};
    };

    namespace TeleportData
    {
        inline constexpr std::array<TeleportBlipGroup, 6> Groups{{
            {"LS Car Meet", 777, false},
            {"Los Santos Customs", 72, true},
            {"Clothing Stores", 73, true},
            {"Ammu-Nation", 110, true},
            {"Barber Shops", 71, true},
            {"Tattoo Parlors", 75, true},
        }};
    }

    struct TeleportSnapshot final
    {
        bool nativeReady{};
        bool actionPending{};
        bool autoWaypointEnabled{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class TeleportRuntime final
    {
    public:
        static TeleportRuntime& Get() noexcept
        {
            static TeleportRuntime instance;
            return instance;
        }

        bool QueueWaypoint()
        {
            if (!CanQueue())
                return false;
            SetPending("Waypoint teleport queued");
            return QueueOrFail([this] { BeginWaypoint(false); });
        }

        bool QueueGroup(std::size_t index)
        {
            if (index >= TeleportData::Groups.size() || !CanQueue())
                return false;
            SetPending(std::string("Teleport queued: ") + TeleportData::Groups[index].label);
            return QueueOrFail([this, index] { TeleportGroup(index); });
        }

        void SetAutoWaypoint(bool enabled)
        {
            m_AutoWaypoint.store(enabled, std::memory_order_release);
            if (enabled)
                EnsureAutoLoop();
        }

        [[nodiscard]] TeleportSnapshot Snapshot() const
        {
            TeleportSnapshot out;
            out.nativeReady = Native::NativeRegistry::Get().IsReady();
            out.actionPending = m_Pending.load(std::memory_order_acquire);
            out.autoWaypointEnabled = m_AutoWaypoint.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            out.haveResult = m_HaveResult;
            out.lastSucceeded = m_LastSucceeded;
            out.message = m_Message;
            return out;
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

        struct TeleportTarget final
        {
            Entity entity{};
            bool inVehicle{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum HandlerIndex : std::size_t
        {
            GetFirstBlip,
            GetNextBlip,
            DoesBlipExist,
            GetBlipCoords,
            SetEntityCoords,
            RequestCollision,
            GetGroundZ,
            IsWaypointActive,
            GetWaypointBlipEnum,
            GetClosestBlip,
            SetWaypointOff,
            GetWaterHeight,
            GetApproxHeight,
            HandlerCount,
        };

        // Current GTA V Enhanced mappings cross-checked against YimMenuV2 enhanced.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xD56419CB9E15983Full, // GET_FIRST_BLIP_INFO_ID
            0xA3F6143A8F610118ull, // GET_NEXT_BLIP_INFO_ID
            0xC450B06E5AAA0985ull, // DOES_BLIP_EXIST
            0x7DFE6973AE84B6EDull, // GET_BLIP_COORDS
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
            0xEA2D52183C7EA9CFull, // REQUEST_COLLISION_AT_COORD
            0xB1EAADCB692D69CEull, // GET_GROUND_Z_FOR_3D_COORD
            0x02213DC34A224533ull, // IS_WAYPOINT_ACTIVE
            0x2A3612A4B836469Eull, // GET_WAYPOINT_BLIP_ENUM_ID
            0xB981254932E1095Eull, // GET_CLOSEST_BLIP_INFO_ID
            0xA4C1E1845880C098ull, // SET_WAYPOINT_OFF
            0xF85C2BE613AD7903ull, // GET_WATER_HEIGHT
            0x54D01A0F98391D5Bull, // GET_APPROX_HEIGHT_FOR_POINT
        };

        static constexpr std::size_t MaxBlips = 64;
        static constexpr int MaxGroundAttempts = 20;

        TeleportRuntime() = default;

        bool CanQueue()
        {
            return Native::NativeRegistry::Get().IsReady()
                && !m_Pending.exchange(true, std::memory_order_acq_rel);
        }

        template<typename Task>
        bool QueueOrFail(Task&& task)
        {
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Task>(task)))
                return true;
            m_Pending.store(false, std::memory_order_release);
            SetResult(false, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] static bool IsExecutable(std::uintptr_t address) noexcept
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

        bool ResolveHandlers()
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
                if (!IsExecutable(address))
                {
                    m_Handlers.fill(nullptr);
                    return false;
                }
                m_Handlers[i] = reinterpret_cast<Native::NativeHandler>(address);
            }
            return true;
        }

        template<typename Ret, typename... Args>
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

        template<typename... Args>
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

        [[nodiscard]] int BlipIterator(std::size_t handlerIndex, int sprite) const
        {
            int result{};
            return Call(handlerIndex, result, sprite) ? result : 0;
        }

        [[nodiscard]] bool BlipExists(int blip) const
        {
            std::int32_t result{};
            return blip != 0 && Call(DoesBlipExist, result, blip) && result != 0;
        }

        [[nodiscard]] bool BlipCoords(int blip, Native::NativeVector3& out) const
        {
            if (!BlipExists(blip) || !Call(GetBlipCoords, out, blip))
                return false;
            return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
        }

        [[nodiscard]] bool WaypointCoords(Native::NativeVector3& out) const
        {
            std::int32_t active{};
            if (!Call(IsWaypointActive, active) || active == 0)
                return false;

            int waypointEnum{};
            if (!Call(GetWaypointBlipEnum, waypointEnum) || waypointEnum == 0)
                return false;

            int blip{};
            if (!Call(GetClosestBlip, blip, waypointEnum) || blip == 0)
                return false;

            return BlipCoords(blip, out);
        }

        [[nodiscard]] std::vector<int> CollectBlips(int sprite) const
        {
            std::vector<int> blips;
            blips.reserve(16);
            int blip = BlipIterator(GetFirstBlip, sprite);
            while (blip != 0 && blips.size() < MaxBlips)
            {
                if (BlipExists(blip))
                    blips.push_back(blip);
                blip = BlipIterator(GetNextBlip, sprite);
            }
            return blips;
        }

        [[nodiscard]] TeleportTarget ResolveLocalTeleportTarget() const noexcept
        {
            TeleportTarget target{};
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return target;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (inVehicle && *inVehicle)
            {
                const auto vehicle = Natives::GetVehiclePedIsIn(*ped, false);
                if (vehicle && *vehicle != 0)
                {
                    const auto exists = Natives::DoesEntityExist(*vehicle);
                    if (exists && *exists)
                    {
                        target.entity = *vehicle;
                        target.inVehicle = true;
                        return target;
                    }
                }
            }

            target.entity = *ped;
            return target;
        }

        [[nodiscard]] bool MoveEntity(Entity entity, float x, float y, float z) const
        {
            if (entity == 0)
                return false;
            const auto exists = Natives::DoesEntityExist(entity);
            if (!exists || !*exists)
                return false;

            // Mirrors YimMenuV2 Entity::SetPosition: the current vehicle is selected
            // by ResolveLocalTeleportTarget(), then SET_ENTITY_COORDS_NO_OFFSET is used.
            return CallVoid(
                SetEntityCoords,
                entity,
                x,
                y,
                z,
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{1});
        }

        void StreamCollision(float x, float y, float z) const
        {
            static_cast<void>(CallVoid(RequestCollision, x, y, z));
        }

        [[nodiscard]] bool ProbeGround(float x, float y, float& out) const
        {
            Native::CallContext context;
            if (!m_Handlers[GetGroundZ]
                || !context.PushArg(x)
                || !context.PushArg(y)
                || !context.PushArg(1000.0f)
                || !context.PushArg(&out)
                || !context.PushArg(std::int32_t{0})
                || !context.PushArg(std::int32_t{0}))
                return false;

            m_Handlers[GetGroundZ](&context);
            context.FixVectors();
            return context.GetReturnValue<std::int32_t>() != 0 && std::isfinite(out);
        }

        [[nodiscard]] bool ProbeWater(float x, float y, float z, float& out) const
        {
            Native::CallContext context;
            if (!m_Handlers[GetWaterHeight]
                || !context.PushArg(x)
                || !context.PushArg(y)
                || !context.PushArg(z)
                || !context.PushArg(&out))
                return false;

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

        void BeginWaypoint(bool automatic)
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread() || !ResolveHandlers())
                return Fail("Teleport natives are unavailable");

            Native::NativeVector3 coords{};
            if (!WaypointCoords(coords))
                return Fail(automatic ? "Auto waypoint disappeared" : "Set a waypoint first");

            if (automatic)
                static_cast<void>(CallVoid(SetWaypointOff));

            BeginResolvedTeleport(coords, automatic ? "Auto waypoint teleport" : "Waypoint teleport");
        }

        void BeginWaypoint(const Native::NativeVector3& coords, bool automatic)
        {
            if (!ResolveHandlers())
                return Fail("Teleport natives are unavailable");
            if (automatic)
                static_cast<void>(CallVoid(SetWaypointOff));
            BeginResolvedTeleport(coords, automatic ? "Auto waypoint teleport" : "Waypoint teleport");
        }

        void BeginResolvedTeleport(const Native::NativeVector3& coords, std::string label)
        {
            if (!ResolveHandlers())
                return Fail("Teleport natives are unavailable");

            const TeleportTarget target = ResolveLocalTeleportTarget();
            if (target.entity == 0)
                return Fail("Local player/vehicle entity is unavailable");

            m_GroundX = coords.x;
            m_GroundY = coords.y;
            m_GroundOriginalZ = coords.z;
            m_GroundProbeZ = coords.z;
            m_GroundResolvedZ = coords.z;
            m_GroundAttempt = 0;
            m_GroundLabel = std::move(label);

            StreamCollision(coords.x, coords.y, coords.z);
            SetPending(m_GroundLabel + ": resolving destination");
            if (!Runtime::GameRuntime::Get().Enqueue([this] { GroundProbe(); }))
                Fail("Ground-probe queue unavailable");
        }

        void GroundProbe()
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread() || !ResolveHandlers())
                return Fail("Teleport natives became unavailable");

            // YimMenuV2 requests collision and yields between attempts rather than
            // repeatedly moving the entity while the destination is still streaming.
            StreamCollision(m_GroundX, m_GroundY, m_GroundOriginalZ);

            float ground = m_GroundProbeZ;
            if (ProbeGround(m_GroundX, m_GroundY, ground))
            {
                m_GroundResolvedZ = ground + 1.0f;
                return ResolveWaterAndFinish(true);
            }

            if ((m_GroundAttempt % 3) == 0)
                m_GroundProbeZ += 25.0f;

            ++m_GroundAttempt;
            if (m_GroundAttempt < MaxGroundAttempts)
            {
                if (Runtime::GameRuntime::Get().Enqueue([this] { GroundProbe(); }))
                    return;
                return Fail("Ground-probe queue unavailable");
            }

            m_GroundResolvedZ = m_GroundOriginalZ;
            ResolveWaterAndFinish(false);
        }

        void ResolveWaterAndFinish(bool foundGround)
        {
            float finalZ = m_GroundResolvedZ;
            float water{};
            if (ProbeWater(m_GroundX, m_GroundY, finalZ, water))
            {
                // Match YimMenuV2: water height wins even when a ground probe succeeded.
                finalZ = water;
            }
            else if (!foundGround)
            {
                finalZ = ApproxHeight(m_GroundX, m_GroundY);
            }

            FinishMove(finalZ, m_GroundLabel + " complete");
        }

        void FinishMove(float z, std::string successMessage)
        {
            // Resolve the current vehicle at the final step, matching YimMenuV2's
            // Ped::TeleportTo behavior after its yielded ground-resolution loop.
            const TeleportTarget target = ResolveLocalTeleportTarget();
            const bool moved = target.entity != 0 && MoveEntity(target.entity, m_GroundX, m_GroundY, z);
            m_Pending.store(false, std::memory_order_release);
            SetResult(moved, moved ? std::move(successMessage) : "Teleport destination placement failed");
        }

        void TeleportGroup(std::size_t index)
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread()
                || !ResolveHandlers()
                || index >= TeleportData::Groups.size())
                return Fail("Teleport natives are unavailable");

            const auto& group = TeleportData::Groups[index];
            const auto blips = CollectBlips(group.sprite);
            if (blips.empty())
                return Fail(std::string(group.label) + " blip is not active in this session");

            std::size_t ordinal{};
            if (group.cycle)
            {
                ordinal = m_GroupCursor[index] % blips.size();
                m_GroupCursor[index] = (ordinal + 1) % blips.size();
            }

            Native::NativeVector3 coords{};
            if (!BlipCoords(blips[ordinal], coords))
                return Fail(std::string(group.label) + " coordinates are unavailable");

            std::string label = group.label;
            if (group.cycle)
                label += " " + std::to_string(ordinal + 1) + "/" + std::to_string(blips.size());
            BeginResolvedTeleport(coords, std::move(label));
        }

        void EnsureAutoLoop()
        {
            bool expected = false;
            if (!m_AutoLoopScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;
            if (!Runtime::GameRuntime::Get().Enqueue([this] { AutoTick(); }))
                m_AutoLoopScheduled.store(false, std::memory_order_release);
        }

        void AutoTick()
        {
            if (!m_AutoWaypoint.load(std::memory_order_acquire))
            {
                m_AutoLoopScheduled.store(false, std::memory_order_release);
                return;
            }

            if (!m_Pending.load(std::memory_order_acquire)
                && Native::NativeRegistry::Get().CanInvokeOnCurrentThread()
                && ResolveHandlers())
            {
                Native::NativeVector3 coords{};
                if (WaypointCoords(coords)
                    && !m_Pending.exchange(true, std::memory_order_acq_rel))
                {
                    // YimMenuV2 consumes the waypoint immediately after capturing its
                    // coordinates. This prevents the loop from repeatedly teleporting to
                    // the same marker and lets the user place a fresh waypoint afterward.
                    static_cast<void>(CallVoid(SetWaypointOff));
                    SetPending("Auto waypoint detected");
                    BeginResolvedTeleport(coords, "Auto waypoint teleport");
                }
            }

            if (m_AutoWaypoint.load(std::memory_order_acquire)
                && Runtime::GameRuntime::Get().Enqueue([this] { AutoTick(); }))
                return;
            m_AutoLoopScheduled.store(false, std::memory_order_release);
        }

        void Fail(std::string message)
        {
            m_Pending.store(false, std::memory_order_release);
            SetResult(false, std::move(message));
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void SetResult(bool success, std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = true;
            m_LastSucceeded = success;
            m_Message = std::move(message);
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
        std::array<std::size_t, TeleportData::Groups.size()> m_GroupCursor{};

        std::atomic<bool> m_Pending{false};
        std::atomic<bool> m_AutoWaypoint{false};
        std::atomic<bool> m_AutoLoopScheduled{false};

        float m_GroundX{};
        float m_GroundY{};
        float m_GroundOriginalZ{};
        float m_GroundProbeZ{};
        float m_GroundResolvedZ{};
        int m_GroundAttempt{};
        std::string m_GroundLabel{};

        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
