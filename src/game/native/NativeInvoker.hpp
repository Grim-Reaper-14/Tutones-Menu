#pragma once

#include "NativeRegistry.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace Tutones::Game::Native
{
    class NativeInvoker final
    {
    public:
        template<typename Return, typename... Args>
        [[nodiscard]] static std::optional<Return> Invoke(NativeId id, Args&&... args) noexcept
        {
            static_assert(!std::is_void_v<Return>);

            auto& registry = NativeRegistry::Get();
            if (!registry.IsReady() || !registry.CanInvokeOnCurrentThread())
                return std::nullopt;

            const auto handler = registry.Handler(id);
            if (!handler)
                return std::nullopt;

            CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return std::nullopt;

            handler(&context);
            context.FixVectors();
            return context.GetReturnValue<Return>();
        }

        template<typename... Args>
        static bool InvokeVoid(NativeId id, Args&&... args) noexcept
        {
            auto& registry = NativeRegistry::Get();
            if (!registry.IsReady() || !registry.CanInvokeOnCurrentThread())
                return false;

            const auto handler = registry.Handler(id);
            if (!handler)
                return false;

            CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return false;

            handler(&context);
            context.FixVectors();
            return true;
        }
    };
}
