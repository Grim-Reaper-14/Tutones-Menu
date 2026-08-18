#include "BytePatch.hpp"

#include <Windows.h>

#include <cstring>

namespace Tutones::Game::Memory
{
    bool BytePatch::Configure(void* address, std::span<const std::uint8_t> replacement) noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!address || replacement.empty() || m_Applied)
            return false;

        m_Address = static_cast<std::uint8_t*>(address);
        m_Replacement.assign(replacement.begin(), replacement.end());
        m_Original.resize(m_Replacement.size());
        std::memcpy(m_Original.data(), m_Address, m_Original.size());
        return true;
    }

    bool BytePatch::Apply() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Address || m_Replacement.empty() || m_Original.size() != m_Replacement.size())
            return false;
        if (m_Applied)
            return true;
        if (!Write(m_Replacement))
            return false;
        m_Applied = true;
        return true;
    }

    bool BytePatch::Restore() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (!m_Applied)
            return true;
        if (!m_Address || m_Original.empty() || !Write(m_Original))
            return false;
        m_Applied = false;
        return true;
    }

    void BytePatch::Reset() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        if (m_Applied && m_Address && !m_Original.empty())
        {
            if (Write(m_Original))
                m_Applied = false;
        }

        m_Address = nullptr;
        m_Original.clear();
        m_Replacement.clear();
        m_Applied = false;
    }

    bool BytePatch::IsConfigured() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Address && !m_Replacement.empty() && m_Original.size() == m_Replacement.size();
    }

    bool BytePatch::IsApplied() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Applied;
    }

    void* BytePatch::Address() const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Address;
    }

    bool BytePatch::Write(std::span<const std::uint8_t> bytes) noexcept
    {
        if (!m_Address || bytes.empty() || bytes.size() != m_Original.size())
            return false;

        DWORD oldProtection{};
        if (!::VirtualProtect(m_Address, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtection))
            return false;

        std::memcpy(m_Address, bytes.data(), bytes.size());
        ::FlushInstructionCache(::GetCurrentProcess(), m_Address, bytes.size());

        DWORD ignored{};
        return ::VirtualProtect(m_Address, bytes.size(), oldProtection, &ignored) != FALSE;
    }
}
