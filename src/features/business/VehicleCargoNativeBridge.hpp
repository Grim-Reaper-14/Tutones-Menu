#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeHandlerValidation.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/types/ScriptTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Business
{
    // One checked native bridge shared by the source and delivery runtimes.
    // Handler resolution is performed only from Tutones' preferred GTA script
    // TLS scope and every invocation re-validates that execution context.
    class VehicleCargoNativeBridge final
    {
    public:
        static VehicleCargoNativeBridge& Get() noexcept
        {
            static VehicleCargoNativeBridge instance;
            return instance;
        }

        [[nodiscard]] bool NetworkHasControl(Entity entity, bool& out) noexcept
        {
            std::int32_t value{};
            if (!Call(NetworkHasControlOfEntity, value, entity))
                return false;
            out = value != 0;
            return true;
        }

        [[nodiscard]] bool NetworkRequestControl(Entity entity) noexcept
        {
            std::int32_t value{};
            return Call(NetworkRequestControlOfEntity, value, entity);
        }

        [[nodiscard]] bool GetBlipIterator(std::int32_t& out) noexcept
        {
            return Call(GetBlipInfoIdIterator, out);
        }

        [[nodiscard]] bool GetFirstBlip(std::int32_t iterator, std::int32_t& out) noexcept
        {
            return Call(GetFirstBlipInfoId, out, iterator);
        }

        [[nodiscard]] bool GetNextBlip(std::int32_t iterator, std::int32_t& out) noexcept
        {
            return Call(GetNextBlipInfoId, out, iterator);
        }

        [[nodiscard]] bool BlipExists(std::int32_t blip, bool& out) noexcept
        {
            std::int32_t value{};
            if (!Call(DoesBlipExist, value, blip))
                return false;
            out = value != 0;
            return true;
        }

        [[nodiscard]] bool GetBlipEntity(std::int32_t blip, Entity& out) noexcept
        {
            std::int32_t value{};
            if (!Call(GetBlipInfoIdEntityIndex, value, blip))
                return false;
            out = static_cast<Entity>(value);
            return true;
        }

        [[nodiscard]] bool RequestCollisionAt(float x, float y, float z) noexcept
        {
            return CallVoid(RequestCollision, x, y, z);
        }

        [[nodiscard]] bool SetCoordsNoOffset(Entity entity, float x, float y, float z) noexcept
        {
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

        [[nodiscard]] bool SetHeading(Entity entity, float heading) noexcept
        {
            return CallVoid(SetEntityHeading, entity, heading);
        }

        [[nodiscard]] bool SetVelocity(Entity entity, float x, float y, float z) noexcept
        {
            return CallVoid(SetEntityVelocity, entity, x, y, z);
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
            NetworkRequestControlOfEntity,
            NetworkHasControlOfEntity,
            GetBlipInfoIdIterator,
            GetFirstBlipInfoId,
            GetNextBlipInfoId,
            DoesBlipExist,
            GetBlipInfoIdEntityIndex,
            RequestCollision,
            SetEntityCoords,
            SetEntityHeading,
            SetEntityVelocity,
            HandlerCount,
        };

        static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0xF093E270C0B6B318ull, // NETWORK_REQUEST_CONTROL_OF_ENTITY
            0x1B1A446EFA398EB5ull, // NETWORK_HAS_CONTROL_OF_ENTITY
            0x2A3612A4B836469Eull, // _GET_BLIP_INFO_ID_ITERATOR
            0xD56419CB9E15983Full, // GET_FIRST_BLIP_INFO_ID
            0xA3F6143A8F610118ull, // GET_NEXT_BLIP_INFO_ID
            0xB5DA0E63D08D983Dull, // DOES_BLIP_EXIST
            0xA143F68B0CD079F4ull, // GET_BLIP_INFO_ID_ENTITY_INDEX
            0xEA2D52183C7EA9CFull, // REQUEST_COLLISION_AT_COORD
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
            0x5C96CEA06531AB03ull, // SET_ENTITY_HEADING
            0x1AB7223AC0702871ull, // SET_ENTITY_VELOCITY
        }};

        VehicleCargoNativeBridge() = default;

        [[nodiscard]] static bool ContextReady() noexcept
        {
            auto& registry = Native::NativeRegistry::Get();
            if (!registry.IsReady() || !registry.CanInvokeOnCurrentThread())
                return false;

            auto* tls = Types::TlsContext::Get();
            return tls && tls->scriptThreadActive && tls->currentScriptThread;
        }

        [[nodiscard]] bool ResolveHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_Handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return ContextReady();

            if (!ContextReady())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            if (!Native::AssignValidatedHandlers(slots, m_Handlers))
            {
                TUTONES_LOG_ERROR("business.vehicle_cargo.native", "Vehicle Cargo native bridge validation failed");
                return false;
            }

            TUTONES_LOG_INFO("business.vehicle_cargo.native", "Vehicle Cargo native bridge resolved under GTA script TLS");
            return true;
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] bool Call(std::size_t index, Ret& out, Args... args) noexcept
        {
            if (index >= m_Handlers.size() || !ResolveHandlers() || !ContextReady())
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;

            const auto handler = m_Handlers[index];
            if (!handler || !Native::IsExecutableHandlerAddress(reinterpret_cast<std::uintptr_t>(handler)))
                return false;

            handler(&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        template<typename... Args>
        [[nodiscard]] bool CallVoid(std::size_t index, Args... args) noexcept
        {
            if (index >= m_Handlers.size() || !ResolveHandlers() || !ContextReady())
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;

            const auto handler = m_Handlers[index];
            if (!handler || !Native::IsExecutableHandlerAddress(reinterpret_cast<std::uintptr_t>(handler)))
                return false;

            handler(&context);
            context.FixVectors();
            return true;
        }

        std::array<Native::NativeHandler, HandlerCount> m_Handlers{};
    };
}
