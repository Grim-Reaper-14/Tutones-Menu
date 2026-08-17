#include "ModuleView.hpp"

#include <algorithm>

namespace Tutones::Game::Memory
{
    bool ModuleView::Initialize(const wchar_t* moduleName)
    {
        Reset();

        if (!moduleName || moduleName[0] == L'\0')
            return false;

        m_Module = ::GetModuleHandleW(moduleName);
        if (!m_Module)
            return false;

        m_Base = reinterpret_cast<std::uintptr_t>(m_Module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m_Base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            Reset();
            return false;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(m_Base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            Reset();
            return false;
        }

        m_Size = nt->OptionalHeader.SizeOfImage;
        if (m_Size == 0)
        {
            Reset();
            return false;
        }

        const auto* section = IMAGE_FIRST_SECTION(nt);
        m_CodeRanges.reserve(nt->FileHeader.NumberOfSections);

        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            const auto& current = section[i];
            if ((current.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;

            const auto offset = static_cast<std::size_t>(current.VirtualAddress);
            if (offset >= m_Size)
                continue;

            const auto requestedSize = static_cast<std::size_t>(current.Misc.VirtualSize);
            const auto rangeSize = std::min(requestedSize, m_Size - offset);
            if (rangeSize == 0)
                continue;

            m_CodeRanges.push_back({reinterpret_cast<const std::byte*>(m_Base + offset), rangeSize});
        }

        if (m_CodeRanges.empty())
        {
            Reset();
            return false;
        }

        return true;
    }

    void ModuleView::Reset() noexcept
    {
        m_Module = nullptr;
        m_Base = 0;
        m_Size = 0;
        m_CodeRanges.clear();
    }

    bool ModuleView::IsValid() const noexcept
    {
        return m_Module != nullptr && m_Base != 0 && m_Size != 0 && !m_CodeRanges.empty();
    }

    HMODULE ModuleView::Module() const noexcept { return m_Module; }
    std::uintptr_t ModuleView::Base() const noexcept { return m_Base; }
    std::size_t ModuleView::Size() const noexcept { return m_Size; }
    const std::vector<MemoryRange>& ModuleView::CodeRanges() const noexcept { return m_CodeRanges; }

    bool ModuleView::Contains(const void* address) const noexcept
    {
        if (!address || !IsValid())
            return false;

        const auto value = reinterpret_cast<std::uintptr_t>(address);
        return value >= m_Base && value < (m_Base + m_Size);
    }
}
