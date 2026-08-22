#pragma once

#include "NativeRegistry.hpp"
#include "../WeaponLaserNatives.hpp"
#include "../types/ScriptTypes.hpp"

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

            auto* tls = Types::TlsContext::Get();
            if (!tls || !tls->scriptThreadActive || !tls->currentScriptThread)
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

            auto* tls = Types::TlsContext::Get();
            if (!tls || !tls->scriptThreadActive || !tls->currentScriptThread)
                return false;

            const auto handler = registry.Handler(id);
            if (!handler)
                return false;

            bool laserEnabled = false;
            if (id == NativeId::EnableLaserSightRendering)
            {
                if constexpr (sizeof...(Args) == 1)
                {
                    const auto captureLaserState = [&laserEnabled](auto&& value) noexcept
                    {
                        using Value = std::decay_t<decltype(value)>;
                        if constexpr (std::is_arithmetic_v<Value>)
                            laserEnabled = value != 0;
                    };
                    (captureLaserState(args), ...);
                }
            }

            CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return false;

            handler(&context);
            context.FixVectors();

            if (id == NativeId::EnableLaserSightRendering)
                static_cast<void>(Tutones::Game::WeaponLaserNatives::RenderAimbotLaser(laserEnabled));

            return true;
        }
    };
}
