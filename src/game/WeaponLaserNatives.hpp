#pragma once

#include "GamePointers.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Tutones::Game
{
    class WeaponLaserNatives final
    {
    public:
        static bool RenderAimbotLaser(bool enabled) noexcept
        {
            if (!enabled)
            {
                ResetTraceState();
                return true;
            }

            const auto ped = InvokeRegistered<std::int32_t>(Native::NativeId::PlayerPedId);
            const auto player = InvokeRegistered<std::int32_t>(Native::NativeId::PlayerId);
            if (!ped || *ped == 0 || !player)
            {
                ResetTraceState();
                return false;
            }

            const auto armed = InvokeRegistered<std::int32_t>(
                Native::NativeId::IsPedArmed,
                *ped,
                std::int32_t{4});
            if (!armed || *armed == 0)
            {
                ResetTraceState();
                return true;
            }

            const auto freeAiming = IsPlayerFreeAiming(*player);
            const auto targeting = IsPlayerTargettingAnything(*player);
            if ((!freeAiming || !*freeAiming) && (!targeting || !*targeting))
            {
                ResetTraceState();
                return true;
            }

            const auto weaponEntity = GetCurrentPedWeaponEntityIndex(*ped);
            if (!weaponEntity || *weaponEntity == 0)
            {
                ResetTraceState();
                return true;
            }

            std::optional<Native::NativeVector3> muzzle;
            if (const auto muzzleBone = GetEntityBoneIndexByName(*weaponEntity, "gun_muzzle"); muzzleBone && *muzzleBone >= 0)
                muzzle = GetWorldPositionOfEntityBone(*weaponEntity, *muzzleBone);
            if (!muzzle)
                muzzle = InvokeRegistered<Native::NativeVector3>(Native::NativeId::GetEntityCoords, *weaponEntity, std::int32_t{1});
            if (!muzzle)
                return true;

            const auto camera = GetGameplayCamCoord();
            const auto rotation = GetGameplayCamRot(2);
            if (!camera || !rotation)
                return true;

            const auto direction = RotationToDirection(*rotation);
            const Native::NativeVector3 finish{
                camera->x + direction.x * MaxLaserDistance,
                camera->y + direction.y * MaxLaserDistance,
                camera->z + direction.z * MaxLaserDistance};

            // Poll the previous frame's asynchronous ray before launching the next one.
            // This avoids the expensive synchronous LOS probe in the weapon tick.
            if (s_PendingShapeTest != 0)
            {
                if (const auto ray = GetShapeTestResult(s_PendingShapeTest))
                {
                    if (ray->status == 2)
                    {
                        s_LastBeamEnd = ray->hit ? ray->endCoords : s_PendingFallbackEnd;
                        s_PendingShapeTest = 0;
                    }
                    else if (ray->status == 0)
                    {
                        s_PendingShapeTest = 0;
                    }
                }
            }

            if (s_PendingShapeTest == 0)
            {
                if (const auto handle = StartShapeTestLosProbe(*camera, finish, -1, *ped, 7); handle && *handle != 0)
                {
                    s_PendingShapeTest = *handle;
                    s_PendingFallbackEnd = finish;
                }
            }

            const Native::NativeVector3 beamEnd = s_LastBeamEnd.value_or(finish);

            // Bright core plus a subtle outer glow gives the beam more presence
            // than GTA's one-pixel line without introducing a particle lifecycle.
            const bool core = DrawLine(*muzzle, beamEnd, 255, 20, 20, 255);

            auto upperStart = *muzzle;
            auto upperEnd = beamEnd;
            upperStart.z += 0.0015f;
            upperEnd.z += 0.0015f;
            static_cast<void>(DrawLine(upperStart, upperEnd, 180, 0, 0, 150));

            auto lowerStart = *muzzle;
            auto lowerEnd = beamEnd;
            lowerStart.z -= 0.0015f;
            lowerEnd.z -= 0.0015f;
            static_cast<void>(DrawLine(lowerStart, lowerEnd, 180, 0, 0, 150));

            return core;
        }

    private:
        struct ShapeResult final
        {
            int status{};
            bool hit{};
            Native::NativeVector3 endCoords{};
            Native::NativeVector3 surfaceNormal{};
            std::int32_t entity{};
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

        static constexpr float MaxLaserDistance = 1000.0f;
        static constexpr float DegToRad = 0.01745329251994329577f;

        enum HandlerIndex : std::size_t
        {
            GetCurrentPedWeaponEntityIndexIndex,
            IsPlayerFreeAimingIndex,
            IsPlayerTargettingAnythingIndex,
            GetEntityBoneIndexByNameIndex,
            GetWorldPositionOfEntityBoneIndex,
            GetGameplayCamCoordIndex,
            GetGameplayCamRotIndex,
            StartShapeTestLosProbeIndex,
            GetShapeTestResultIndex,
            DrawLineIndex,
            HandlerCount,
        };

        // Current GTA V Enhanced hashes verified against the active crossmap.
        inline static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0x484426882F80CACEull, // GET_CURRENT_PED_WEAPON_ENTITY_INDEX
            0x1C751EF63BF4D501ull, // IS_PLAYER_FREE_AIMING
            0x4F035D45FC2856F8ull, // IS_PLAYER_TARGETTING_ANYTHING
            0x365DC1E8054AF31Aull, // GET_ENTITY_BONE_INDEX_BY_NAME
            0x75DF72FC74EED046ull, // GET_WORLD_POSITION_OF_ENTITY_BONE
            0xCF141FCD0940B0A3ull, // GET_GAMEPLAY_CAM_COORD
            0xD84A545408A3099Aull, // GET_GAMEPLAY_CAM_ROT
            0x120E577522852984ull, // START_SHAPE_TEST_LOS_PROBE
            0x0E7DD1EBCA8D2DE3ull, // GET_SHAPE_TEST_RESULT
            0xC9A38C22BE8013F2ull, // DRAW_LINE
        }};

        [[nodiscard]] static Native::NativeVector3 RotationToDirection(const Native::NativeVector3& rotation) noexcept
        {
            const float pitch = rotation.x * DegToRad;
            const float yaw = rotation.z * DegToRad;
            const float horizontal = std::abs(std::cos(pitch));
            return Native::NativeVector3{
                -std::sin(yaw) * horizontal,
                std::cos(yaw) * horizontal,
                std::sin(pitch)};
        }

        static void ResetTraceState() noexcept
        {
            s_PendingShapeTest = 0;
            s_LastBeamEnd.reset();
            s_PendingFallbackEnd = {};
        }

        [[nodiscard]] static std::optional<std::int32_t> GetCurrentPedWeaponEntityIndex(std::int32_t ped) noexcept
        {
            return ped != 0
                ? Invoke<std::int32_t>(GetCurrentPedWeaponEntityIndexIndex, ped, std::int32_t{0})
                : std::nullopt;
        }

        [[nodiscard]] static std::optional<bool> IsPlayerFreeAiming(std::int32_t player) noexcept
        {
            const auto value = Invoke<std::int32_t>(IsPlayerFreeAimingIndex, player);
            return value ? std::optional<bool>(*value != 0) : std::nullopt;
        }

        [[nodiscard]] static std::optional<bool> IsPlayerTargettingAnything(std::int32_t player) noexcept
        {
            const auto value = Invoke<std::int32_t>(IsPlayerTargettingAnythingIndex, player);
            return value ? std::optional<bool>(*value != 0) : std::nullopt;
        }

        [[nodiscard]] static std::optional<int> GetEntityBoneIndexByName(
            std::int32_t entity,
            const char* boneName) noexcept
        {
            if (entity == 0 || !boneName || !*boneName)
                return std::nullopt;
            return Invoke<int>(GetEntityBoneIndexByNameIndex, entity, boneName);
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetWorldPositionOfEntityBone(
            std::int32_t entity,
            int boneIndex) noexcept
        {
            if (entity == 0 || boneIndex < 0)
                return std::nullopt;
            return Invoke<Native::NativeVector3>(GetWorldPositionOfEntityBoneIndex, entity, boneIndex);
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetGameplayCamCoord() noexcept
        {
            return Invoke<Native::NativeVector3>(GetGameplayCamCoordIndex);
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetGameplayCamRot(int rotationOrder) noexcept
        {
            return Invoke<Native::NativeVector3>(GetGameplayCamRotIndex, rotationOrder);
        }

        [[nodiscard]] static std::optional<int> StartShapeTestLosProbe(
            const Native::NativeVector3& start,
            const Native::NativeVector3& finish,
            int flags,
            std::int32_t ignoredEntity,
            int optionFlags) noexcept
        {
            return Invoke<int>(
                StartShapeTestLosProbeIndex,
                start.x, start.y, start.z,
                finish.x, finish.y, finish.z,
                flags,
                ignoredEntity,
                optionFlags);
        }

        [[nodiscard]] static std::optional<ShapeResult> GetShapeTestResult(int handle) noexcept
        {
            if (handle == 0 || !ResolveHandlers())
                return std::nullopt;

            std::int32_t hit{};
            Native::NativeVector3 endCoords{};
            Native::NativeVector3 surfaceNormal{};
            std::int32_t entity{};

            Native::CallContext context;
            if (!context.PushArg(handle)
                || !context.PushArg(&hit)
                || !context.PushArg(&endCoords)
                || !context.PushArg(&surfaceNormal)
                || !context.PushArg(&entity))
            {
                return std::nullopt;
            }

            s_Handlers[GetShapeTestResultIndex](&context);
            context.FixVectors();

            ShapeResult result;
            result.status = context.GetReturnValue<int>();
            result.hit = hit != 0;
            result.endCoords = endCoords;
            result.surfaceNormal = surfaceNormal;
            result.entity = entity;
            return result;
        }

        static bool DrawLine(
            const Native::NativeVector3& start,
            const Native::NativeVector3& finish,
            int red,
            int green,
            int blue,
            int alpha) noexcept
        {
            return InvokeVoid(
                DrawLineIndex,
                start.x, start.y, start.z,
                finish.x, finish.y, finish.z,
                red, green, blue, alpha);
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] static std::optional<Ret> InvokeRegistered(Native::NativeId id, Args... args) noexcept
        {
            auto& registry = Native::NativeRegistry::Get();
            if (!registry.IsReady() || !registry.CanInvokeOnCurrentThread())
                return std::nullopt;

            const auto handler = registry.Handler(id);
            if (!handler)
                return std::nullopt;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return std::nullopt;

            handler(&context);
            context.FixVectors();
            return context.GetReturnValue<Ret>();
        }

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

        template<typename... Args>
        static bool InvokeVoid(std::size_t index, Args... args) noexcept
        {
            if (index >= HandlerCount || !ResolveHandlers())
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;

            s_Handlers[index](&context);
            context.FixVectors();
            return true;
        }

        inline static std::array<Native::NativeHandler, HandlerCount> s_Handlers{};
        inline static std::atomic<bool> s_Ready{false};
        inline static std::mutex s_Mutex{};
        inline static int s_PendingShapeTest{};
        inline static std::optional<Native::NativeVector3> s_LastBeamEnd{};
        inline static Native::NativeVector3 s_PendingFallbackEnd{};
    };
}
