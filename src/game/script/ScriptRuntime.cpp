#include "ScriptRuntime.hpp"

#include <algorithm>

namespace Tutones::Game::Script
{
    namespace
    {
        [[nodiscard]] std::string BoundedScriptName(const char* text, std::size_t capacity)
        {
            if (!text || capacity == 0)
                return {};

            std::size_t length{};
            while (length < capacity && text[length] != '\0')
                ++length;
            return std::string(text, length);
        }
    }

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

    std::vector<ScriptThreadSnapshot> ScriptRuntime::ThreadsSnapshot() const
    {
        std::vector<ScriptThreadSnapshot> result;
        std::scoped_lock lock(m_Mutex);
        if (!m_Threads || !m_Threads->data || m_Threads->size > m_Threads->capacity)
            return result;

        result.reserve(m_Threads->size);
        for (std::uint16_t index = 0; index < m_Threads->size; ++index)
        {
            auto* thread = m_Threads->data[index];
            if (!thread || thread->context.threadId == 0)
                continue;

            ScriptThreadSnapshot snapshot{};
            snapshot.threadId = thread->context.threadId;
            snapshot.scriptHash = thread->scriptHash;
            snapshot.scriptName = BoundedScriptName(thread->scriptName, sizeof(thread->scriptName));
            snapshot.state = thread->context.state;
            snapshot.programCounter = thread->context.programCounter;
            snapshot.framePointer = thread->context.framePointer;
            snapshot.stackPointer = thread->context.stackPointer;
            snapshot.stackSize = thread->context.stackSize;
            snapshot.stackReady = thread->stack != nullptr;

            if (m_Programs)
            {
                for (std::size_t programIndex = 0; programIndex < ScriptProgramCount; ++programIndex)
                {
                    auto* program = m_Programs[programIndex];
                    if (!program || (program->hash != snapshot.scriptHash && program->nameHash != snapshot.scriptHash))
                        continue;

                    snapshot.programLoaded = true;
                    snapshot.codeSize = program->codeSize;
                    snapshot.localCount = program->localCount;
                    snapshot.globalCount = program->globalCount;
                    snapshot.nativeCount = program->nativeCount;
                    break;
                }
            }

            result.push_back(std::move(snapshot));
        }

        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            if (left.scriptName == right.scriptName)
                return left.threadId < right.threadId;
            return left.scriptName < right.scriptName;
        });
        return result;
    }

    std::optional<std::uint64_t> ScriptRuntime::ReadLocalRaw(std::uint32_t scriptHash, std::size_t index) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Threads || !m_Threads->data || m_Threads->size > m_Threads->capacity)
            return std::nullopt;

        for (std::uint16_t threadIndex = 0; threadIndex < m_Threads->size; ++threadIndex)
        {
            auto* thread = m_Threads->data[threadIndex];
            if (!thread || thread->context.threadId == 0 || thread->scriptHash != scriptHash || !thread->stack)
                continue;

            // Enhanced m_StackSize is a count of 64-bit script slots, not a byte count.
            if (index >= static_cast<std::size_t>(thread->context.stackSize))
                return std::nullopt;

            return static_cast<const std::uint64_t*>(thread->stack)[index];
        }

        return std::nullopt;
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
