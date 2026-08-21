#pragma once

#include "GamePointers.hpp"
#include "Natives.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace Tutones::Game::NetworkPlayerNatives
{
    namespace Detail
    {
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
            IsPlayerActive,
            GetPlayerName,
            GetPlayerPedScriptIndex,
            GetAverageLatency,
            GetAveragePacketLoss,
            HandlerCount,
        };

        inline std::array<Native::NativeHandler, HandlerCount>& Handlers() noexcept
        {
            static std::array<Native::NativeHandler, HandlerCount> handlers{};
            return handlers;
        }

        inline bool ResolveHandlers() noexcept
        {
            auto& handlers = Handlers();
            if (handlers[IsPlayerActive]
                && handlers[GetPlayerName]
                && handlers[GetPlayerPedScriptIndex]
                && handlers[GetAverageLatency]
                && handlers[GetAveragePacketLoss])
            {
                return true;
            }

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto initNativeTables = GamePointers::Get().InitNativeTables();
            if (!initNativeTables)
                return false;

            // Current GTA V Enhanced mappings used by the session player inspector.
            std::array<std::uint64_t, HandlerCount> slots{
                0x762604C40829DB72ull, // NETWORK_IS_PLAYER_ACTIVE
                0xBD6CA019F46AB947ull, // GET_PLAYER_NAME
                0xE8466DBC1A7E794Full, // GET_PLAYER_PED_SCRIPT_INDEX
                0xD29CB5E83871293Bull, // NETWORK_GET_AVERAGE_LATENCY
                0xA26711392EBF5371ull, // NETWORK_GET_AVERAGE_PACKET_LOSS
            };

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            initNativeTables(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                handlers[index] = reinterpret_cast<Native::NativeHandler>(
                    static_cast<std::uintptr_t>(slots[index]));
            }

            return handlers[IsPlayerActive]
                && handlers[GetPlayerName]
                && handlers[GetPlayerPedScriptIndex]
                && handlers[GetAverageLatency]
                && handlers[GetAveragePacketLoss];
        }
    }

    [[nodiscard]] inline bool Ready() noexcept
    {
        return Detail::ResolveHandlers();
    }

    [[nodiscard]] inline std::optional<bool> IsPlayerActive(Player player) noexcept
    {
        if (player < 0 || player >= 32 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(player))
            return std::nullopt;
        Detail::Handlers()[Detail::IsPlayerActive](&context);
        return context.GetReturnValue<std::int32_t>() != 0;
    }

    [[nodiscard]] inline std::optional<std::string> GetPlayerName(Player player)
    {
        if (player < 0 || player >= 32 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(player))
            return std::nullopt;
        Detail::Handlers()[Detail::GetPlayerName](&context);
        const char* name = context.GetReturnValue<const char*>();
        if (!name)
            return std::nullopt;
        return std::string(name);
    }

    [[nodiscard]] inline std::optional<Ped> GetPlayerPedScriptIndex(Player player) noexcept
    {
        if (player < 0 || player >= 32 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(player))
            return std::nullopt;
        Detail::Handlers()[Detail::GetPlayerPedScriptIndex](&context);
        return context.GetReturnValue<Ped>();
    }

    [[nodiscard]] inline std::optional<float> GetAverageLatency(Player player) noexcept
    {
        if (player < 0 || player >= 32 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(player))
            return std::nullopt;
        Detail::Handlers()[Detail::GetAverageLatency](&context);
        return context.GetReturnValue<float>();
    }

    [[nodiscard]] inline std::optional<float> GetAveragePacketLoss(Player player) noexcept
    {
        if (player < 0 || player >= 32 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(player))
            return std::nullopt;
        Detail::Handlers()[Detail::GetAveragePacketLoss](&context);
        return context.GetReturnValue<float>();
    }
}
