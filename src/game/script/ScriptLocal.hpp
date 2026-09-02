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
            : m_Stack(thread ? thread->stack : nullptr), m_Index(index), m_Thread(thread)
        {
        }

        [[nodiscard]] constexpr ScriptLocal At(std::ptrdiff_t offset) const noexcept
        {
            if (offset < 0)
            {
                const auto magnitude = static_cast<std::size_t>(-(offset + 1)) + 1;
                if (magnitude > m_Index)
                    return ScriptLocal(nullptr, 0, m_Thread);
                return ScriptLocal(m_Stack, m_Index - magnitude, m_Thread);
            }

            const auto magnitude = static_cast<std::size_t>(offset);
            if (m_Index > static_cast<std::size_t>(-1) - magnitude)
                return ScriptLocal(nullptr, 0, m_Thread);
            return ScriptLocal(m_Stack, m_Index + magnitude, m_Thread);
        }

        [[nodiscard]] bool CanAccess() const noexcept
        {
            if (!m_Stack)
                return false;
            if (!m_Thread)
                return true;
            return m_Thread->context.threadId != 0
                && m_Thread->context.state != Types::ScriptThreadState::Killed
                && m_Thread->stack == m_Stack
                && m_Index < static_cast<std::size_t>(m_Thread->context.stackSize);
        }

        template<typename T = std::uint64_t>
        [[nodiscard]] T* As() const noexcept
        {
            constexpr std::size_t slotSize = sizeof(std::uint64_t);
            constexpr std::size_t slotsRequired = (sizeof(T) + slotSize - 1) / slotSize;
            if (!CanAccess())
                return nullptr;
            if (m_Thread)
            {
                const auto slotCount = static_cast<std::size_t>(m_Thread->context.stackSize);
                if (slotsRequired > slotCount - m_Index)
                    return nullptr;
            }
            auto* bytes = static_cast<std::byte*>(m_Stack);
            return reinterpret_cast<T*>(bytes + (m_Index * sizeof(std::uint64_t)));
        }

    private:
        constexpr ScriptLocal(void* stack, std::size_t index, Types::ScriptThread* thread) noexcept
            : m_Stack(stack), m_Index(index), m_Thread(thread)
        {
        }

        void* m_Stack{};
        std::size_t m_Index{};
        Types::ScriptThread* m_Thread{};
    };
}
