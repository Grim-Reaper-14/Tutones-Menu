#pragma once

#include "../types/ScriptProgram.hpp"
#include "../types/ScriptTypes.hpp"

#include <cstdint>
#include <mutex>

namespace Tutones::Game::Script
{
    using ScriptVmFn = std::int64_t(*)(std::uint64_t* stack,
        std::int64_t** globals,
        Types::ScriptProgram* program,
        Types::ScriptThreadContext* context);

    class ScriptRuntime final
    {
    public:
        static ScriptRuntime& Get() noexcept;

        void Configure(
            Types::AtArray<Types::ScriptThread*>* threads,
            Types::AtArray<Types::ScriptProgram*>* programs,
            std::int64_t** globals,
            ScriptVmFn scriptVm) noexcept;

        void Reset() noexcept;

        [[nodiscard]] bool IsReady() const noexcept;
        [[nodiscard]] Types::ScriptThread* FindThread(std::uint32_t scriptHash) const noexcept;
        [[nodiscard]] Types::ScriptProgram* FindProgram(std::uint32_t scriptHash) const noexcept;
        [[nodiscard]] std::int64_t** Globals() const noexcept;
        [[nodiscard]] ScriptVmFn ScriptVm() const noexcept;

    private:
        ScriptRuntime() = default;

        mutable std::mutex m_Mutex;
        Types::AtArray<Types::ScriptThread*>* m_Threads{};
        Types::AtArray<Types::ScriptProgram*>* m_Programs{};
        std::int64_t** m_Globals{};
        ScriptVmFn m_ScriptVm{};
    };
}
