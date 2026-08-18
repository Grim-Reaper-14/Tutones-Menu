#include "ScriptFunction.hpp"

#include "ScriptRuntime.hpp"

#include <cstring>
#include <utility>

namespace Tutones::Game::Script
{
    ScriptFunction::ScriptFunction(std::uint32_t scriptHash, ScriptPointer pointer)
        : m_ScriptHash(scriptHash), m_Pointer(std::move(pointer))
    {
    }

    bool ScriptFunction::CallImpl(
        const std::vector<std::uint64_t>& args,
        void* returnValue,
        std::uint32_t returnSize)
    {
        auto& runtime = ScriptRuntime::Get();
        if (!runtime.IsReady())
            return false;

        auto* thread = runtime.FindThread(m_ScriptHash);
        auto* program = runtime.FindProgram(m_ScriptHash);
        auto** globals = runtime.Globals();
        auto scriptVm = runtime.ScriptVm();
        if (!thread || !program || !thread->stack || !globals || !scriptVm)
            return false;

        if (m_ProgramCounter == 0)
        {
            m_ProgramCounter = m_Pointer.Scan(program);
            if (m_ProgramCounter == 0)
                return false;
        }

        auto* tls = Types::TlsContext::Get();
        if (!tls)
            return false;

        auto* stack = static_cast<std::uint64_t*>(thread->stack);
        Types::ScriptThreadContext context = thread->context;
        const auto topStack = context.stackPointer;

        for (const auto arg : args)
            stack[context.stackPointer++] = arg;
        stack[context.stackPointer++] = 0;
        context.programCounter = m_ProgramCounter;
        context.state = Types::ScriptThreadState::Idle;

        auto* originalThread = tls->currentScriptThread;
        const bool originalActive = tls->scriptThreadActive;
        tls->currentScriptThread = thread;
        tls->scriptThreadActive = true;

        static_cast<void>(scriptVm(stack, globals, program, &context));

        tls->scriptThreadActive = originalActive;
        tls->currentScriptThread = originalThread;

        if (returnValue && returnSize > 0)
            std::memcpy(returnValue, stack + topStack, returnSize);
        return true;
    }
}
