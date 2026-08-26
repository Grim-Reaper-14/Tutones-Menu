#pragma once

#include "GamePointers.hpp"
#include "Natives.hpp"
#include "native/NativeCallContext.hpp"
#include "native/NativeHandlerValidation.hpp"
#include "native/NativeRegistry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Tutones::Game::NetshoppingNatives
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
            UseServerTransactions,
            CatalogItemKeyIsValid,
            GetPrice,
            BeginService,
            CheckoutStart,
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
            bool ready = true;
            for (const auto handler : handlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto initNativeTables = GamePointers::Get().InitNativeTables();
            if (!initNativeTables)
                return false;

            // Current GTA V Enhanced mappings verified against YimMenuV2's Enhanced crossmap.
            std::array<std::uint64_t, HandlerCount> slots{
                0xC18CB5D7A27A2E00ull, // NET_GAMESERVER_USE_SERVER_TRANSACTIONS
                0x206AC354EB77B7FDull, // NET_GAMESERVER_CATALOG_ITEM_KEY_IS_VALID
                0xD2ACF01ED6E6D7C6ull, // NET_GAMESERVER_GET_PRICE
                0x54BC5E0B6A29AE8Aull, // NET_GAMESERVER_BEGIN_SERVICE
                0x5F7C6361179DFFC4ull, // NET_GAMESERVER_CHECKOUT_START
            };

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            initNativeTables(&program);

            return Native::AssignValidatedHandlers(slots, handlers);
        }
    }

    [[nodiscard]] inline std::optional<bool> UseServerTransactions() noexcept
    {
        if (!Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        Detail::Handlers()[Detail::UseServerTransactions](&context);
        return context.GetReturnValue<std::int32_t>() != 0;
    }

    [[nodiscard]] inline std::optional<bool> CatalogItemKeyIsValid(Hash hash) noexcept
    {
        if (hash == 0 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(hash))
            return std::nullopt;
        Detail::Handlers()[Detail::CatalogItemKeyIsValid](&context);
        return context.GetReturnValue<std::int32_t>() != 0;
    }

    [[nodiscard]] inline std::optional<int> GetPrice(
        Hash itemHash,
        Hash categoryHash,
        bool useCurrentPrice = true) noexcept
    {
        if (itemHash == 0 || categoryHash == 0 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(itemHash)
            || !context.PushArg(categoryHash)
            || !context.PushArg(static_cast<std::int32_t>(useCurrentPrice)))
        {
            return std::nullopt;
        }

        Detail::Handlers()[Detail::GetPrice](&context);
        return context.GetReturnValue<int>();
    }

    [[nodiscard]] inline std::optional<bool> BeginService(
        int* transactionId,
        Hash categoryHash,
        Hash itemHash,
        Hash actionHash,
        int value,
        int flags = 4) noexcept
    {
        if (!transactionId || categoryHash == 0 || itemHash == 0 || actionHash == 0 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(transactionId)
            || !context.PushArg(categoryHash)
            || !context.PushArg(itemHash)
            || !context.PushArg(actionHash)
            || !context.PushArg(value)
            || !context.PushArg(flags))
        {
            return std::nullopt;
        }

        Detail::Handlers()[Detail::BeginService](&context);
        context.FixVectors();
        return context.GetReturnValue<std::int32_t>() != 0;
    }

    [[nodiscard]] inline std::optional<bool> CheckoutStart(int transactionId) noexcept
    {
        if (transactionId < 0 || !Detail::ResolveHandlers())
            return std::nullopt;

        Native::CallContext context;
        if (!context.PushArg(transactionId))
            return std::nullopt;

        Detail::Handlers()[Detail::CheckoutStart](&context);
        return context.GetReturnValue<std::int32_t>() != 0;
    }
}
