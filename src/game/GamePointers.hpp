#pragma once

#include "memory/ModuleView.hpp"
#include "types/ScriptProgram.hpp"
#include "types/ScriptTypes.hpp"

#include <atomic>
#include <cstdint>

namespace Tutones::Game
{
    using InitNativeTablesFn = void(*)(void* program);
    using RunScriptThreadsFn = bool(*)(int operationsToExecute);
    using PtrToHandleFn = int(*)(void* pointer);
    using ScriptVmFn = int(*)(std::uint64_t* stack, std::int64_t** globals, Types::ScriptProgram* program, void* context);
    using AssistedAimFindNewTargetFn = bool(*)(std::int64_t context);

    class GamePointers final
    {
    public:
        static GamePointers& Get() noexcept;

        bool Resolve();
        void Reset() noexcept;

        [[nodiscard]] bool IsResolved() const noexcept;
        [[nodiscard]] InitNativeTablesFn InitNativeTables() const noexcept;
        [[nodiscard]] RunScriptThreadsFn RunScriptThreads() const noexcept;
        [[nodiscard]] Types::AtArray<Types::ScriptThread*>* ScriptThreads() const noexcept;
        [[nodiscard]] Types::ScriptProgram** ScriptPrograms() const noexcept;
        [[nodiscard]] std::int64_t** ScriptGlobals() const noexcept;
        [[nodiscard]] ScriptVmFn ScriptVm() const noexcept;
        [[nodiscard]] PtrToHandleFn PtrToHandle() const noexcept;
        [[nodiscard]] void* AssistedAimShouldReleaseEntity() const noexcept;
        [[nodiscard]] AssistedAimFindNewTargetFn AssistedAimFindNewTarget() const noexcept;
        [[nodiscard]] const Memory::ModuleView& Module() const noexcept;

    private:
        GamePointers() = default;
        ~GamePointers() = default;
        GamePointers(const GamePointers&) = delete;
        GamePointers& operator=(const GamePointers&) = delete;

        Memory::ModuleView m_Module;
        InitNativeTablesFn m_InitNativeTables{};
        RunScriptThreadsFn m_RunScriptThreads{};
        Types::AtArray<Types::ScriptThread*>* m_ScriptThreads{};
        Types::ScriptProgram** m_ScriptPrograms{};
        std::int64_t** m_ScriptGlobals{};
        ScriptVmFn m_ScriptVm{};
        PtrToHandleFn m_PtrToHandle{};
        void* m_AssistedAimShouldReleaseEntity{};
        AssistedAimFindNewTargetFn m_AssistedAimFindNewTarget{};
        std::atomic<bool> m_Resolved{false};
    };
}
