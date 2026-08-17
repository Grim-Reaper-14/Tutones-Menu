#pragma once

#include "memory/ModuleView.hpp"
#include "types/ScriptTypes.hpp"

#include <atomic>

namespace Tutones::Game
{
    using InitNativeTablesFn = void(*)(void* program);
    using RunScriptThreadsFn = bool(*)(int operationsToExecute);

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
        std::atomic<bool> m_Resolved{false};
    };
}
