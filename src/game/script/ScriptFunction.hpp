#pragma once

#include "ScriptPointer.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Tutones::Game::Script
{
    class ScriptFunction final
    {
    public:
        ScriptFunction(std::uint32_t scriptHash, ScriptPointer pointer);

        template<typename... Args>
        bool CallVoid(Args&&... args)
        {
            std::vector<std::uint64_t> params;
            params.reserve(sizeof...(Args));
            (PushArg(params, std::forward<Args>(args)), ...);
            return CallImpl(params, nullptr, 0);
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] std::optional<Ret> TryCall(Args&&... args)
        {
            static_assert(!std::is_void_v<Ret>);
            std::vector<std::uint64_t> params;
            params.reserve(sizeof...(Args));
            (PushArg(params, std::forward<Args>(args)), ...);

            Ret result{};
            if (!CallImpl(params, &result, static_cast<std::uint32_t>(sizeof(result))))
                return std::nullopt;
            return result;
        }

        [[nodiscard]] std::uint32_t ProgramCounter() const noexcept { return m_ProgramCounter; }

    private:
        template<typename Arg>
        static void PushArg(std::vector<std::uint64_t>& stack, Arg&& value)
        {
            using Value = std::remove_cv_t<std::remove_reference_t<Arg>>;
            static_assert(sizeof(Value) <= sizeof(std::uint64_t), "ScriptFunction arguments must fit one GTA script stack slot");

            std::uint64_t slot{};
            const Value copy = std::forward<Arg>(value);
            std::memcpy(&slot, &copy, sizeof(Value));
            stack.push_back(slot);
        }

        bool CallImpl(const std::vector<std::uint64_t>& args, void* returnValue, std::uint32_t returnSize);

        std::uint32_t m_ScriptHash{};
        ScriptPointer m_Pointer;
        std::uint32_t m_ProgramCounter{};
    };
}
