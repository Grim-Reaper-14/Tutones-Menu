#pragma once

#include <cstddef>
#include <cstdint>

namespace Tutones::Game::Mods::VehicleStealth
{
    enum class Hardware : unsigned char
    {
        Unsupported,
        FoldingWings,
        MissileBays,
    };

    inline constexpr std::uint32_t AkulaModel = 0x46699F47u;
    inline constexpr std::uint32_t Annihilator2Model = 0x11962E49u;
    inline constexpr std::uint32_t RaijuModel = 0x0E4C8C4Du;

    inline constexpr std::size_t PlayerFreemodeGlobal = 1845347;
    inline constexpr std::size_t PlayerFreemodeStride = 884;
    inline constexpr std::size_t StealthFlagsOffset = 868;
    inline constexpr std::uint32_t StealthBit = 1u << 1;

    [[nodiscard]] constexpr Hardware HardwareForModel(std::uint32_t model) noexcept
    {
        if (model == AkulaModel || model == Annihilator2Model)
            return Hardware::FoldingWings;
        if (model == RaijuModel)
            return Hardware::MissileBays;
        return Hardware::Unsupported;
    }

    [[nodiscard]] constexpr bool PhysicalStateMatches(bool deployed, bool stealthEnabled) noexcept
    {
        return deployed == !stealthEnabled;
    }

    [[nodiscard]] constexpr bool ScriptStateMatches(std::uint32_t flags, bool stealthEnabled) noexcept
    {
        return ((flags & StealthBit) != 0) == stealthEnabled;
    }

    [[nodiscard]] constexpr std::size_t PlayerStealthFlagsIndex(std::size_t player) noexcept
    {
        // ScriptGlobal::At(index, stride) advances past GTA's array-size slot.
        return PlayerFreemodeGlobal + 1 + (player * PlayerFreemodeStride) + StealthFlagsOffset;
    }
}
