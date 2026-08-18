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
        Types::AtArray<Types::ScriptProgram*>* programs,
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
        if (!m_Threads || !m_Threads->data)
            return nullptr;

        for (std::uint16_t index = 0; index < m_Threads->size; ++index)
        {
            auto* thread = m_Threads->data[index];
            if (thread && thread->scriptHash == scriptHash)
                return thread;
        }

        return nullptr;
    }

    Types::ScriptProgram* ScriptRuntime::FindProgram(std::uint32_t scriptHash) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Programs || !m_Programs->data)
            return nullptr;

        for (std::uint16_t index = 0; index < m_Programs->size; ++index)
        {
            auto* program = m_Programs->data[index];
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
