#pragma once

#include "GamePointers.hpp"
#include "Natives.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Tutones::Game
{
    class EntityInspectorNatives final
    {
    public:
        struct ShapeResult final
        {
            int status{};
            bool hit{};
            Native::NativeVector3 endCoords{};
            Native::NativeVector3 surfaceNormal{};
            Hash materialHash{};
            Entity entity{};
        };

        [[nodiscard]] static std::optional<Native::NativeVector3> GetGameplayCamCoord() noexcept
        {
            return Invoke<Native::NativeVector3>(GetGameplayCamCoordIndex);
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetGameplayCamRot(int rotationOrder = 2) noexcept
        {
            return Invoke<Native::NativeVector3>(GetGameplayCamRotIndex, rotationOrder);
        }

        [[nodiscard]] static std::optional<int> StartShapeTestLosProbe(
            const Native::NativeVector3& start,
            const Native::NativeVector3& finish,
            int flags,
            Entity ignoredEntity,
            int optionFlags = 7) noexcept
        {
            return Invoke<int>(
                StartShapeTestLosProbeIndex,
                start.x, start.y, start.z,
                finish.x, finish.y, finish.z,
                flags,
                ignoredEntity,
                optionFlags);
        }

        [[nodiscard]] static std::optional<ShapeResult> GetShapeTestResultIncludingMaterial(int handle) noexcept
        {
            if (handle == 0 || !ResolveHandlers())
                return std::nullopt;

            std::int32_t hit{};
            Native::NativeVector3 endCoords{};
            Native::NativeVector3 surfaceNormal{};
            Hash materialHash{};
            Entity entity{};

            Native::CallContext context;
            if (!context.PushArg(handle)
                || !context.PushArg(&hit)
                || !context.PushArg(&endCoords)
                || !context.PushArg(&surfaceNormal)
                || !context.PushArg(&materialHash)
                || !context.PushArg(&entity))
            {
                return std::nullopt;
            }

            s_Handlers[GetShapeTestResultIncludingMaterialIndex](&context);
            context.FixVectors();

            ShapeResult result;
            result.status = context.GetReturnValue<int>();
            result.hit = hit != 0;
            result.endCoords = endCoords;
            result.surfaceNormal = surfaceNormal;
            result.materialHash = materialHash;
            result.entity = entity;
            return result;
        }

        [[nodiscard]] static std::optional<int> GetEntityType(Entity entity) noexcept
        {
            return entity != 0 ? Invoke<int>(GetEntityTypeIndex, entity) : std::nullopt;
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetEntityVelocity(Entity entity) noexcept
        {
            return entity != 0 ? Invoke<Native::NativeVector3>(GetEntityVelocityIndex, entity) : std::nullopt;
        }

        [[nodiscard]] static std::optional<float> GetEntitySpeed(Entity entity) noexcept
        {
            return entity != 0 ? Invoke<float>(GetEntitySpeedIndex, entity) : std::nullopt;
        }

        [[nodiscard]] static std::optional<float> GetVehicleEngineHealth(Vehicle vehicle) noexcept
        {
            return vehicle != 0 ? Invoke<float>(GetVehicleEngineHealthIndex, vehicle) : std::nullopt;
        }

        [[nodiscard]] static std::optional<float> GetVehicleBodyHealth(Vehicle vehicle) noexcept
        {
            return vehicle != 0 ? Invoke<float>(GetVehicleBodyHealthIndex, vehicle) : std::nullopt;
        }

        [[nodiscard]] static std::optional<int> GetPedType(Ped ped) noexcept
        {
            return ped != 0 ? Invoke<int>(GetPedTypeIndex, ped) : std::nullopt;
        }

        [[nodiscard]] static std::optional<bool> IsPedAPlayer(Ped ped) noexcept
        {
            if (ped == 0)
                return std::nullopt;
            const auto value = Invoke<std::int32_t>(IsPedAPlayerIndex, ped);
            return value ? std::optional<bool>(*value != 0) : std::nullopt;
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
            GetGameplayCamCoordIndex,
            GetGameplayCamRotIndex,
            StartShapeTestLosProbeIndex,
            GetShapeTestResultIncludingMaterialIndex,
            GetEntityTypeIndex,
            GetEntityVelocityIndex,
            GetEntitySpeedIndex,
            GetVehicleEngineHealthIndex,
            GetVehicleBodyHealthIndex,
            GetPedTypeIndex,
            IsPedAPlayerIndex,
            HandlerCount,
        };

        // Current GTA V Enhanced native hashes verified against the current crossmap.
        inline static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0xCF141FCD0940B0A3ull, // GET_GAMEPLAY_CAM_COORD
            0xD84A545408A3099Aull, // GET_GAMEPLAY_CAM_ROT
            0x120E577522852984ull, // START_SHAPE_TEST_LOS_PROBE
            0xEE92B4A78668B1CEull, // GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL
            0x75A2D1BBA9D95D0Eull, // GET_ENTITY_TYPE
            0xE5741C6B6539231Full, // GET_ENTITY_VELOCITY
            0xDF93B3CFAC96698Full, // GET_ENTITY_SPEED
            0x4C7724D572378B05ull, // GET_VEHICLE_ENGINE_HEALTH
            0x3B5692CB240DBC2Full, // GET_VEHICLE_BODY_HEALTH
            0x0DFE7358172FC006ull, // GET_PED_TYPE
            0x501EBB0523078750ull, // IS_PED_A_PLAYER
        }};

        static bool ResolveHandlers() noexcept
        {
            if (s_Ready.load(std::memory_order_acquire))
                return true;
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            std::scoped_lock lock(s_Mutex);
            if (s_Ready.load(std::memory_order_relaxed))
                return true;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = HandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            for (std::size_t i = 0; i < slots.size(); ++i)
                s_Handlers[i] = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[i]));

            for (const auto handler : s_Handlers)
            {
                if (!handler)
                {
                    s_Handlers.fill(nullptr);
                    return false;
                }
            }

            s_Ready.store(true, std::memory_order_release);
            return true;
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] static std::optional<Ret> Invoke(std::size_t index, Args... args) noexcept
        {
            if (index >= HandlerCount || !ResolveHandlers())
                return std::nullopt;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return std::nullopt;

            s_Handlers[index](&context);
            context.FixVectors();
            return context.GetReturnValue<Ret>();
        }

        inline static std::array<Native::NativeHandler, HandlerCount> s_Handlers{};
        inline static std::atomic<bool> s_Ready{false};
        inline static std::mutex s_Mutex{};
    };
}
