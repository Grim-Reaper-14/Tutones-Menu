#pragma once

#include "../types/ScriptTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Script
{
    class ScriptLocal final
    {
    public:
        constexpr ScriptLocal(void* stack, std::size_t index) noexcept
            : m_Stack(stack), m_Index(index)
        {
        }

        constexpr ScriptLocal(Types::ScriptThread* thread, std::size_t index) noexcept
            : ScriptLocal(thread ? thread->stack : nullptr, index)
        {
        }

        [[nodiscard]] constexpr ScriptLocal At(std::ptrdiff_t offset) const noexcept
        {
            return ScriptLocal(m_Stack, static_cast<std::size_t>(static_cast<std::ptrdiff_t>(m_Index) + offset));
        }

        [[nodiscard]] bool CanAccess() const noexcept
        {
            return m_Stack != nullptr;
        }

        template<typename T = std::uint64_t>
        [[nodiscard]] T* As() const noexcept
        {
            if (!m_Stack)
                return nullptr;
            auto* bytes = static_cast<std::byte*>(m_Stack);
            return reinterpret_cast<T*>(bytes + (m_Index * sizeof(std::uint64_t)));
        }

    private:
        void* m_Stack{};
        std::size_t m_Index{};
    };
}
