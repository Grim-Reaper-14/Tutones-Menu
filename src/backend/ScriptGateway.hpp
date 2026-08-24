#pragma once

#include "../game/script/ScriptRuntime.hpp"

#include <cstdint>

namespace Tutones::Backend
{
    // Central read-only gateway to the shared GTA script runtime. Feature code should
    // prefer this facade instead of independently discovering thread/program/global/VM
    // pointers. Invocation helpers can build on this without duplicating ownership.
    class ScriptGateway final
    {
    public:
        [[nodiscard]] bool IsReady() const noexcept
        {
            return Game::Script::ScriptRuntime::Get().IsReady();
        }

        [[nodiscard]] Game::Types::ScriptThread* Thread(std::uint32_t scriptHash) const noexcept
        {
            return Game::Script::ScriptRuntime::Get().FindThread(scriptHash);
        }

        [[nodiscard]] Game::Types::ScriptProgram* Program(std::uint32_t scriptHash) const noexcept
        {
            return Game::Script::ScriptRuntime::Get().FindProgram(scriptHash);
        }

        [[nodiscard]] std::int64_t** Globals() const noexcept
        {
            return Game::Script::ScriptRuntime::Get().Globals();
        }

        [[nodiscard]] Game::Script::ScriptVmFn Vm() const noexcept
        {
            return Game::Script::ScriptRuntime::Get().ScriptVm();
        }
    };
}
