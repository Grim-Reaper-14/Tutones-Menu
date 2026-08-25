#pragma once

#include "../../game/native/NativeInvoker.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <atomic>
#include <cstdint>

namespace Tutones::Game::NativeTools
{
    class NetworkControlRuntime final
    {
    public:
        static NetworkControlRuntime& Get() noexcept
        {
            static NetworkControlRuntime instance;
            return instance;
        }

        [[nodiscard]] bool QueueRequestControl(Entity entity, int maxAttempts = 15)
        {
            if (entity == 0)
                return false;
            maxAttempts = maxAttempts < 1 ? 1 : (maxAttempts > 60 ? 60 : maxAttempts);
            return Runtime::GameRuntime::Get().Enqueue([entity, maxAttempts] {
                for (int i = 0; i < maxAttempts; ++i)
                {
                    const auto have = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::NetworkHasControlOfEntity, entity);
                    if (have && *have != 0)
                        return;
                    static_cast<void>(Native::NativeInvoker::InvokeVoid(Native::NativeId::NetworkRequestControlOfEntity, entity));
                }
            });
        }

    private:
        NetworkControlRuntime() = default;
    };
}
