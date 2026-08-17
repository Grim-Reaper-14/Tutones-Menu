#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Tutones::Game::Memory
{
    struct MemoryRange final
    {
        const std::byte* data{};
        std::size_t size{};
    };

    class ModuleView final
    {
    public:
        bool Initialize(const wchar_t* moduleName);
        void Reset() noexcept;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] HMODULE Module() const noexcept;
        [[nodiscard]] std::uintptr_t Base() const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;
        [[nodiscard]] const std::vector<MemoryRange>& CodeRanges() const noexcept;
        [[nodiscard]] bool Contains(const void* address) const noexcept;

    private:
        HMODULE m_Module{};
        std::uintptr_t m_Base{};
        std::size_t m_Size{};
        std::vector<MemoryRange> m_CodeRanges;
    };
}
