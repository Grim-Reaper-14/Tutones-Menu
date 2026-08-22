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
    class WeaponLaserNatives final
    {
    public:
        [[nodiscard]] static std::optional<Entity> GetCurrentPedWeaponEntityIndex(Ped ped) noexcept
        {
            if (ped == 0)
                return std::nullopt;
            return Invoke<Entity>(GetCurrentPedWeaponEntityIndexIndex, ped, std::int32_t{0});
        }

        [[nodiscard]] static std::optional<bool> IsPlayerFreeAiming(Player player) noexcept
        {
            const auto value = Invoke<std::int32_t>(IsPlayerFreeAimingIndex, player);
            return value ? std::optional<bool>(*value != 0) : std::nullopt;
        }

        [[nodiscard]] static std::optional<int> GetEntityBoneIndexByName(Entity entity, const char* boneName) noexcept
        {
            if (entity == 0 || !boneName || !*boneName)
                return std::nullopt;
            return Invoke<int>(GetEntityBoneIndexByNameIndex, entity, boneName);
        }

        [[nodiscard]] static std::optional<Native::NativeVector3> GetWorldPositionOfEntityBone(
            Entity entity,
            int boneIndex) noexcept
        {
            if (entity == 0 || boneIndex < 0)
                return std::nullopt;
            return Invoke<Native::NativeVector3>(GetWorldPositionOfEntityBoneIndex, entity, boneIndex);
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
            GetCurrentPedWeaponEntityIndexIndex,
            IsPlayerFreeAimingIndex,
            GetEntityBoneIndexByNameIndex,
            GetWorldPositionOfEntityBoneIndex,
            DrawLineIndex,
            HandlerCount,
        };

        // Current GTA V Enhanced hashes from the active crossmap.
        inline static constexpr std::array<std::uint64_t, HandlerCount> HandlerHashes{{
            0x484426882F80CACEull, // GET_CURRENT_PED_WEAPON_ENTITY_INDEX
            0x1C751EF63BF4D501ull, // IS_PLAYER_FREE_AIMING
            0x365DC1E8054AF31Aull, // GET_ENTITY_BONE_INDEX_BY_NAME
            0x75DF72FC74EED046ull, // GET_WORLD_POSITION_OF_ENTITY_BONE
            0xC9A38C22BE8013F2ull, // DRAW_LINE
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
    };
}
