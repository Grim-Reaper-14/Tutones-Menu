#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace Tutones::Game::Memory
{
    class BytePatch final
    {
    public:
        BytePatch() = default;
        ~BytePatch() = default;
        BytePatch(const BytePatch&) = delete;
        BytePatch& operator=(const BytePatch&) = delete;

        bool Configure(void* address, std::span<const std::uint8_t> replacement) noexcept;
        bool Apply() noexcept;
        bool Restore() noexcept;
        void Reset() noexcept;

        [[nodiscard]] bool IsConfigured() const noexcept;
        [[nodiscard]] bool IsApplied() const noexcept;
        [[nodiscard]] void* Address() const noexcept;

    private:
        bool Write(std::span<const std::uint8_t> bytes) noexcept;

        mutable std::mutex m_Mutex;
        std::uint8_t* m_Address{};
        std::vector<std::uint8_t> m_Original;
        std::vector<std::uint8_t> m_Replacement;
        bool m_Applied{};
    };
}
