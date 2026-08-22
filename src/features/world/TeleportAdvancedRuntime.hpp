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
            HandlerCount,
        };

        // Current GTA V Enhanced hashes cross-checked against YimMenuV2 enhanced.
        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{
            0xB981254932E1095Eull, // GET_CLOSEST_BLIP_INFO_ID
            0xC450B06E5AAA0985ull, // DOES_BLIP_EXIST
            0x7DFE6973AE84B6EDull, // GET_BLIP_COORDS
            0x0D1381B6E0F3987Dull, // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
        };

        // Same objective sprite order used by YimMenuV2's TpToObjective.
        static constexpr std::array<int, 17> ObjectiveSprites{
            1, 0, 2,
            143, 144, 145, 146,
            478,
            535, 536, 537, 538, 539, 540, 541, 542,
            549,
        };

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

            TUTONES_LOG_INFO("world.teleport", "Enhanced objective and directional teleport natives resolved");
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

        [[nodiscard]] Entity LocalTeleportEntity() const
        {
            const auto ped = PlayerNatives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;
            const auto vehicle = VehicleNatives::GetVehiclePedIsUsing(*ped);
            return vehicle && *vehicle != 0 ? *vehicle : *ped;
        }

        [[nodiscard]] bool MoveEntity(Entity entity, const Native::NativeVector3& coords) const
        {
            if (entity == 0 || !std::isfinite(coords.x) || !std::isfinite(coords.y) || !std::isfinite(coords.z))
                return false;
            return CallVoid(
                SetEntityCoords,
                entity,
                coords.x,
                coords.y,
                coords.z,
                std::int32_t{0},
                std::int32_t{0},
                std::int32_t{0});
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
                coords.z += 1.0f;

                const Entity target = LocalTeleportEntity();
                return Finish(MoveEntity(target, coords), "Teleport to objective");
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

            const Entity target = LocalTeleportEntity();
            Finish(MoveEntity(target, coords), "Directional teleport");
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
    };
}
