#include "ScriptPatchRuntime.hpp"

#include "../../core/logging/Logger.hpp"

#include <MinHook.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

namespace Tutones::Game::Script
{
    namespace
    {
        constexpr std::uint32_t ScriptPageSize = 0x4000;

        std::string MinHookMessage(const char* prefix, MH_STATUS status)
        {
            std::string message(prefix);
            message += ": ";
            message += MH_StatusToString(status);
            return message;
        }
    }

    ScriptPatchRuntime& ScriptPatchRuntime::Get() noexcept
    {
        static ScriptPatchRuntime instance;
        return instance;
    }

    bool ScriptPatchRuntime::Start() noexcept
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return true;

        m_ShuttingDown.store(false, std::memory_order_release);
        m_HookActive.store(false, std::memory_order_release);
        m_ActiveCallbacks.store(0, std::memory_order_release);

        const auto scriptVm = ScriptRuntime::Get().ScriptVm();
        if (!scriptVm)
        {
            TUTONES_LOG_WARN("script.patch", "ScriptVM pointer is unavailable; script bytecode patches will report unsupported");
            return true;
        }

        m_ScriptVmTarget = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(scriptVm));
        const auto detour = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&ScriptPatchRuntime::ScriptVmDetour));
        auto status = MH_CreateHook(m_ScriptVmTarget, detour, reinterpret_cast<void**>(&m_OriginalScriptVm));
        if (status != MH_OK)
        {
            TUTONES_LOG_WARN("script.patch", MinHookMessage("Failed to create ScriptVM shadow-patch hook", status));
            m_OriginalScriptVm = nullptr;
            m_ScriptVmTarget = nullptr;
            return true;
        }

        status = MH_EnableHook(m_ScriptVmTarget);
        if (status != MH_OK)
        {
            TUTONES_LOG_WARN("script.patch", MinHookMessage("Failed to enable ScriptVM shadow-patch hook", status));
            MH_RemoveHook(m_ScriptVmTarget);
            m_OriginalScriptVm = nullptr;
            m_ScriptVmTarget = nullptr;
            return true;
        }

        m_HookActive.store(true, std::memory_order_release);
        TUTONES_LOG_INFO("script.patch", "ScriptVM shadow-bytecode patch runtime installed");
        return true;
    }

    void ScriptPatchRuntime::Stop() noexcept
    {
        if (!m_Running.exchange(false, std::memory_order_acq_rel))
            return;

        m_ShuttingDown.store(true, std::memory_order_release);
        if (m_ScriptVmTarget)
        {
            const auto status = MH_DisableHook(m_ScriptVmTarget);
            if (status != MH_OK && status != MH_ERROR_DISABLED)
                TUTONES_LOG_WARN("script.patch", MinHookMessage("ScriptVM shadow-patch hook disable returned", status));
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (m_ActiveCallbacks.load(std::memory_order_acquire) != 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (m_ActiveCallbacks.load(std::memory_order_acquire) != 0)
            TUTONES_LOG_WARN("script.patch", "Timed out waiting for ScriptVM shadow-patch callbacks to drain");

        if (m_ScriptVmTarget)
        {
            const auto status = MH_RemoveHook(m_ScriptVmTarget);
            if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
                TUTONES_LOG_WARN("script.patch", MinHookMessage("ScriptVM shadow-patch hook removal returned", status));
        }

        {
            std::scoped_lock lock(m_Mutex);
            m_Patches.clear();
            m_Shadows.clear();
            m_NextHandle = 1;
        }

        m_OriginalScriptVm = nullptr;
        m_ScriptVmTarget = nullptr;
        m_HookActive.store(false, std::memory_order_release);
        m_ShuttingDown.store(false, std::memory_order_release);
        TUTONES_LOG_INFO("script.patch", "ScriptVM shadow-bytecode patch runtime stopped");
    }

    bool ScriptPatchRuntime::IsRunning() const noexcept
    {
        return m_Running.load(std::memory_order_acquire);
    }

    bool ScriptPatchRuntime::HookActive() const noexcept
    {
        return m_HookActive.load(std::memory_order_acquire);
    }

    ScriptPatchHandle ScriptPatchRuntime::AddPatch(
        std::uint32_t scriptHash,
        ScriptPointer pointer,
        std::vector<std::uint8_t> replacement)
    {
        if (!IsRunning() || scriptHash == 0 || replacement.empty())
            return 0;

        std::scoped_lock lock(m_Mutex);
        const ScriptPatchHandle handle = m_NextHandle++;
        Patch patch{};
        patch.handle = handle;
        patch.scriptHash = scriptHash;
        patch.pointer = std::move(pointer);
        patch.replacement = std::move(replacement);
        m_Patches.emplace_back(std::move(patch));

        if (auto it = m_Shadows.find(scriptHash); it != m_Shadows.end())
            it->second.dirty = true;
        return handle;
    }

    void ScriptPatchRuntime::RemovePatch(ScriptPatchHandle handle) noexcept
    {
        if (handle == 0)
            return;

        std::scoped_lock lock(m_Mutex);
        const auto found = std::find_if(m_Patches.begin(), m_Patches.end(), [handle](const Patch& patch) {
            return patch.handle == handle;
        });
        if (found == m_Patches.end())
            return;

        if (auto it = m_Shadows.find(found->scriptHash); it != m_Shadows.end())
            it->second.dirty = true;
        m_Patches.erase(found);
    }

    bool ScriptPatchRuntime::SetPatchEnabled(ScriptPatchHandle handle, bool enabled) noexcept
    {
        if (handle == 0)
            return false;

        std::scoped_lock lock(m_Mutex);
        const auto found = std::find_if(m_Patches.begin(), m_Patches.end(), [handle](const Patch& patch) {
            return patch.handle == handle;
        });
        if (found == m_Patches.end())
            return false;
        if (found->enabled == enabled)
            return true;

        found->enabled = enabled;
        if (auto it = m_Shadows.find(found->scriptHash); it != m_Shadows.end())
            it->second.dirty = true;
        return true;
    }

    ScriptPatchStatus ScriptPatchRuntime::Status(ScriptPatchHandle handle) const noexcept
    {
        ScriptPatchStatus status{};
        if (handle == 0)
            return status;

        std::scoped_lock lock(m_Mutex);
        const auto found = std::find_if(m_Patches.begin(), m_Patches.end(), [handle](const Patch& patch) {
            return patch.handle == handle;
        });
        if (found == m_Patches.end())
            return status;

        status.registered = true;
        status.enabled = found->enabled;
        status.supported = found->supported;
        if (const auto shadow = m_Shadows.find(found->scriptHash); shadow != m_Shadows.end())
            status.active = found->enabled && found->supported && shadow->second.active && !shadow->second.dirty;
        return status;
    }

    bool ScriptPatchRuntime::PatchMatchesProgram(const Patch& patch, const Types::ScriptProgram* program) const noexcept
    {
        return program && (program->hash == patch.scriptHash || program->nameHash == patch.scriptHash);
    }

    bool ScriptPatchRuntime::CopyOriginal(ProgramShadow& shadow) noexcept
    {
        if (!shadow.originalBlocks || shadow.codeSize == 0 || shadow.pages.empty())
            return false;

        std::uint32_t remaining = shadow.codeSize;
        for (std::size_t page = 0; page < shadow.pages.size(); ++page)
        {
            if (!shadow.originalBlocks[page])
                return false;
            const auto bytes = static_cast<std::size_t>(std::min<std::uint32_t>(remaining, ScriptPageSize));
            if (shadow.pages[page].size() != bytes)
                shadow.pages[page].resize(bytes);
            std::memcpy(shadow.pages[page].data(), shadow.originalBlocks[page], bytes);
            remaining -= static_cast<std::uint32_t>(bytes);
        }
        return remaining == 0;
    }

    bool ScriptPatchRuntime::WriteShadow(
        ProgramShadow& shadow,
        std::uint32_t pc,
        const std::vector<std::uint8_t>& bytes) noexcept
    {
        if (bytes.empty() || pc >= shadow.codeSize || bytes.size() > shadow.codeSize - pc)
            return false;

        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            const std::uint32_t address = pc + static_cast<std::uint32_t>(index);
            const std::size_t page = address >> 14;
            const std::size_t offset = address & 0x3FFF;
            if (page >= shadow.pages.size() || offset >= shadow.pages[page].size())
                return false;
            shadow.pages[page][offset] = bytes[index];
        }
        return true;
    }

    bool ScriptPatchRuntime::RebuildShadow(
        ProgramShadow& shadow,
        Types::ScriptProgram* program,
        std::uint32_t scopeHash) noexcept
    {
        if (!program || !program->codeBlocks || program->codeSize == 0 || shadow.activeUsers != 0)
            return false;

        shadow.program = program;
        shadow.originalBlocks = program->codeBlocks;
        shadow.codeSize = program->codeSize;
        shadow.active = false;

        const std::size_t pageCount = (static_cast<std::size_t>(shadow.codeSize) + ScriptPageSize - 1) / ScriptPageSize;
        shadow.pages.clear();
        shadow.blockPointers.clear();
        shadow.pages.resize(pageCount);
        shadow.blockPointers.resize(pageCount);

        std::uint32_t remaining = shadow.codeSize;
        for (std::size_t page = 0; page < pageCount; ++page)
        {
            if (!shadow.originalBlocks[page])
                return false;
            const auto bytes = static_cast<std::size_t>(std::min<std::uint32_t>(remaining, ScriptPageSize));
            shadow.pages[page].resize(bytes);
            shadow.blockPointers[page] = shadow.pages[page].data();
            remaining -= static_cast<std::uint32_t>(bytes);
        }
        if (remaining != 0 || !CopyOriginal(shadow))
            return false;

        bool anyEnabled = false;
        bool enabledUnsupported = false;
        for (auto& patch : m_Patches)
        {
            if (patch.scriptHash != scopeHash)
                continue;

            patch.resolvedProgram = program;
            patch.pc = patch.pointer.Scan(program);
            patch.supported = patch.pc != 0
                && patch.pc < program->codeSize
                && patch.replacement.size() <= program->codeSize - patch.pc;
            if (patch.enabled)
            {
                anyEnabled = true;
                if (!patch.supported)
                    enabledUnsupported = true;
            }
        }

        if (enabledUnsupported)
        {
            shadow.dirty = false;
            return true;
        }

        for (const auto& patch : m_Patches)
        {
            if (patch.scriptHash != scopeHash || !patch.enabled)
                continue;
            if (!WriteShadow(shadow, patch.pc, patch.replacement))
                return false;
        }

        shadow.active = anyEnabled;
        shadow.dirty = false;
        return true;
    }

    std::uint8_t** ScriptPatchRuntime::PrepareProgram(Types::ScriptProgram* program) noexcept
    {
        if (!program || !program->codeBlocks || program->codeSize == 0 || !HookActive())
            return nullptr;

        try
        {
            std::scoped_lock lock(m_Mutex);

            std::uint32_t scopeHash{};
            for (const auto& patch : m_Patches)
            {
                if (PatchMatchesProgram(patch, program))
                {
                    scopeHash = patch.scriptHash;
                    break;
                }
            }
            if (scopeHash == 0)
                return nullptr;

            auto& shadow = m_Shadows[scopeHash];
            if (shadow.program == program && !shadow.blockPointers.empty() && program->codeBlocks == shadow.blockPointers.data())
            {
                if (!shadow.active)
                    return nullptr;
                ++shadow.activeUsers;
                return shadow.blockPointers.data();
            }

            const bool storageChanged = shadow.program != program
                || shadow.originalBlocks != program->codeBlocks
                || shadow.codeSize != program->codeSize;
            if (storageChanged)
            {
                if (shadow.activeUsers != 0)
                    return nullptr;
                shadow = ProgramShadow{};
                shadow.program = program;
                shadow.originalBlocks = program->codeBlocks;
                shadow.codeSize = program->codeSize;
                shadow.dirty = true;
                for (auto& patch : m_Patches)
                {
                    if (patch.scriptHash == scopeHash)
                    {
                        patch.resolvedProgram = nullptr;
                        patch.pc = 0;
                        patch.supported = false;
                    }
                }
            }

            if (shadow.dirty)
            {
                if (shadow.activeUsers != 0)
                {
                    if (!shadow.active)
                        return nullptr;
                    ++shadow.activeUsers;
                    return shadow.blockPointers.data();
                }
                if (!RebuildShadow(shadow, program, scopeHash))
                    return nullptr;
            }

            if (!shadow.active || shadow.blockPointers.empty())
                return nullptr;

            ++shadow.activeUsers;
            return shadow.blockPointers.data();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void ScriptPatchRuntime::ReleaseProgram(Types::ScriptProgram* program) noexcept
    {
        if (!program)
            return;

        std::scoped_lock lock(m_Mutex);
        for (auto& [hash, shadow] : m_Shadows)
        {
            static_cast<void>(hash);
            if (shadow.program == program && shadow.activeUsers != 0)
            {
                --shadow.activeUsers;
                return;
            }
        }
    }

    int ScriptPatchRuntime::ScriptVmDetour(
        std::uint64_t* stack,
        std::int64_t** globals,
        Types::ScriptProgram* program,
        void* context) noexcept
    {
        auto& runtime = Get();
        runtime.m_ActiveCallbacks.fetch_add(1, std::memory_order_acq_rel);

        auto** originalBlocks = program ? program->codeBlocks : nullptr;
        auto** patchedBlocks = (!runtime.m_ShuttingDown.load(std::memory_order_acquire) && program)
            ? runtime.PrepareProgram(program)
            : nullptr;
        if (program && patchedBlocks)
            program->codeBlocks = patchedBlocks;

        int result{};
        if (runtime.m_OriginalScriptVm)
            result = runtime.m_OriginalScriptVm(stack, globals, program, context);

        if (program && patchedBlocks)
        {
            program->codeBlocks = originalBlocks;
            runtime.ReleaseProgram(program);
        }

        runtime.m_ActiveCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        return result;
    }
}
