#include "game/script/ScriptLocal.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using Tutones::Game::Script::ScriptLocal;
using Tutones::Game::Types::ScriptThread;
using Tutones::Game::Types::ScriptThreadState;

namespace
{
    struct TwoSlots final
    {
        std::uint64_t first{};
        std::uint64_t second{};
    };
}

int main()
{
    std::array<std::uint64_t, 4> stack{};
    ScriptThread thread{};
    thread.context.threadId = 1;
    thread.context.state = ScriptThreadState::Running;
    thread.context.stackSize = static_cast<std::uint32_t>(stack.size());
    thread.stack = stack.data();

    assert(ScriptLocal(&thread, 0).As<std::uint64_t>() == &stack[0]);
    assert(ScriptLocal(&thread, 3).As<std::uint64_t>() == &stack[3]);
    assert(ScriptLocal(&thread, 4).As<std::uint64_t>() == nullptr);
    assert(ScriptLocal(&thread, 3).As<TwoSlots>() == nullptr);
    assert(ScriptLocal(&thread, 2).As<TwoSlots>() != nullptr);

    assert(ScriptLocal(&thread, 1).At(-1).As<std::uint64_t>() == &stack[0]);
    assert(ScriptLocal(&thread, 0).At(-1).As<std::uint64_t>() == nullptr);

    thread.context.state = ScriptThreadState::Killed;
    assert(ScriptLocal(&thread, 0).As<std::uint64_t>() == nullptr);

    thread.context.state = ScriptThreadState::Running;
    thread.context.threadId = 0;
    assert(ScriptLocal(&thread, 0).As<std::uint64_t>() == nullptr);

    thread.context.threadId = 1;
    thread.stack = nullptr;
    assert(ScriptLocal(&thread, 0).As<std::uint64_t>() == nullptr);
    return 0;
}
