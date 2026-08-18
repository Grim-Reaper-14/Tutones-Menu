#include "ScriptRuntime.hpp"

namespace Tutones::Game::Script
{
    ScriptRuntime& ScriptRuntime::Get() noexcept
    {
        static ScriptRuntime instance;
        return instance;
    }

    void ScriptRuntime::Configure(
        Types::AtArray<Types::ScriptThread*>* threads,
        Types::ScriptProgram** programs,
        std::int64_t** globals,
        ScriptVmFn scriptVm) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Threads = threads;
        m_Programs = programs;
        m_Globals = globals;
        m_ScriptVm = scriptVm;
    }

    void ScriptRuntime::Reset() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Threads = nullptr;
        m_Programs = nullptr;
        m_Globals = nullptr;
        m_ScriptVm = nullptr;
    }

    bool ScriptRuntime::IsReady() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Threads && m_Programs && m_Globals && m_ScriptVm;
    }

    Types::ScriptThread* ScriptRuntime::FindThread(std::uint32_t scriptHash) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Threads || !m_Threads->data || m_Threads->size > m_Threads->capacity)
            return nullptr;

        for (std::uint16_t index = 0; index < m_Threads->size; ++index)
        {
            auto* thread = m_Threads->data[index];
            if (thread && thread->context.threadId != 0 && thread->scriptHash == scriptHash)
                return thread;
        }

        return nullptr;
    }

    Types::ScriptProgram* ScriptRuntime::FindProgram(std::uint32_t scriptHash) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Programs)
            return nullptr;

        for (std::size_t index = 0; index < ScriptProgramCount; ++index)
        {
            auto* program = m_Programs[index];
            if (program && (program->hash == scriptHash || program->nameHash == scriptHash))
                return program;
        }

        return nullptr;
    }

    std::int64_t** ScriptRuntime::Globals() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Globals;
    }

    ScriptVmFn ScriptRuntime::ScriptVm() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_ScriptVm;
    }
}
