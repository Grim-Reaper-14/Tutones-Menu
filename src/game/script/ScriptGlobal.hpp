#pragma once

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Script
{
    class ScriptGlobal final
    {
    public:
        explicit constexpr ScriptGlobal(std::size_t index) noexcept
            : m_Index(index)
        {
        }

        [[nodiscard]] constexpr std::size_t Index() const noexcept
        {
            return m_Index;
        }

        [[nodiscard]] constexpr ScriptGlobal At(std::size_t offset) const noexcept
        {
            return ScriptGlobal(m_Index + offset);
        }

        // GTA script arrays store their length in the first slot. Yim-style helpers
        // therefore advance by one slot before indexing an array element.
        [[nodiscard]] constexpr ScriptGlobal At(std::size_t index, std::size_t elementSize) const noexcept
        {
            return ScriptGlobal(m_Index + 1 + (index * elementSize));
        }

        template<typename T = std::int64_t>
        [[nodiscard]] T* As(std::int64_t** pages) const noexcept
        {
            if (!pages)
                return nullptr;

            constexpr std::size_t PageShift = 18;
            constexpr std::size_t PageMask = 0x3F;
            constexpr std::size_t SlotMask = 0x3FFFF;

            const std::size_t page = (m_Index >> PageShift) & PageMask;
            const std::size_t slot = m_Index & SlotMask;
            auto* pageBase = pages[page];
            if (!pageBase)
                return nullptr;

            return reinterpret_cast<T*>(&pageBase[slot]);
        }

    private:
        std::size_t m_Index{};
    };
}
