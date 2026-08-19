#pragma once

#include "ScriptPointer.hpp"
#include "ScriptRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Tutones::Game::Script
{
    using ScriptPatchHandle = std::uint64_t;

    struct ScriptPatchStatus final
    {
        bool registered{};
        bool enabled{};
        bool supported{};
        bool active{};
    };

    class ScriptPatchRuntime final
    {
    public:
        static ScriptPatchRuntime& Get() noexcept;

        bool Start() noexcept;
        void Stop() noexcept;

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] bool HookActive() const noexcept;

        ScriptPatchHandle AddPatch(
            std::uint32_t scriptHash,
            ScriptPointer pointer,
            std::vector<std::uint8_t> replacement);
        void RemovePatch(ScriptPatchHandle handle) noexcept;
        bool SetPatchEnabled(ScriptPatchHandle handle, bool enabled) noexcept;
        [[nodiscard]] ScriptPatchStatus Status(ScriptPatchHandle handle) const noexcept;

    private:
        struct Patch final
        {
            ScriptPatchHandle handle{};
            std::uint32_t scriptHash{};
            ScriptPointer pointer{"", ""};
            std::vector<std::uint8_t> replacement;
            Types::ScriptProgram* resolvedProgram{};
            std::uint32_t pc{};
            bool enabled{};
            bool supported{};
        };

        struct ProgramShadow final
        {
            Types::ScriptProgram* program{};
            std::uint8_t** originalBlocks{};
            std::uint32_t codeSize{};
            std::vector<std::vector<std::uint8_t>> pages;
            std::vector<std::uint8_t*> blockPointers;
            std::uint32_t activeUsers{};
            bool dirty{true};
            bool active{};
        };

        ScriptPatchRuntime() = default;
        ~ScriptPatchRuntime() = default;
        ScriptPatchRuntime(const ScriptPatchRuntime&) = delete;
        ScriptPatchRuntime& operator=(const ScriptPatchRuntime&) = delete;

        static int ScriptVmDetour(
            std::uint64_t* stack,
            std::int64_t** globals,
            Types::ScriptProgram* program,
            void* context) noexcept;

        [[nodiscard]] std::uint8_t** PrepareProgram(Types::ScriptProgram* program) noexcept;
        void ReleaseProgram(Types::ScriptProgram* program) noexcept;
        bool RebuildShadow(ProgramShadow& shadow, Types::ScriptProgram* program, std::uint32_t scopeHash) noexcept;
        bool CopyOriginal(ProgramShadow& shadow) noexcept;
        bool WriteShadow(ProgramShadow& shadow, std::uint32_t pc, const std::vector<std::uint8_t>& bytes) noexcept;
        [[nodiscard]] bool PatchMatchesProgram(const Patch& patch, const Types::ScriptProgram* program) const noexcept;

        mutable std::mutex m_Mutex;
        std::vector<Patch> m_Patches;
        std::unordered_map<std::uint32_t, ProgramShadow> m_Shadows;
        ScriptPatchHandle m_NextHandle{1};

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_ShuttingDown{false};
        std::atomic<bool> m_HookActive{false};
        std::atomic<std::uint32_t> m_ActiveCallbacks{0};
        ScriptVmFn m_OriginalScriptVm{};
        void* m_ScriptVmTarget{};
    };
}
