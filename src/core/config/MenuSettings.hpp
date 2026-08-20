#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Tutones::Core::Config
{
    struct PlayerMenuSettings final
    {
        bool invincible{};
        bool invisible{};
        bool noRagdoll{};
        bool superJump{};
        bool infiniteStamina{};
        bool neverWanted{};
        bool policeIgnore{};
        bool everyoneIgnore{};
        float runMultiplier{1.0f};
        float swimMultiplier{1.0f};
    };

    struct VehicleMenuSettings final
    {
        bool removeLscRestrictions{};
    };

    struct WeaponMenuSettings final
    {
        bool infiniteAmmo{};
        bool infiniteClip{};
        bool aimbot{};
        bool aimForHead{true};
        bool targetDrivers{true};
        bool releaseDeadPed{true};
        bool explosiveAmmo{};
        int explosionType{18};
        float explosionDamage{1.0f};
        float explosionCameraShake{0.1f};
    };

    struct UiMenuSettings final
    {
        std::string activeTheme{"default.json"};
    };

    struct MenuSettingsData final
    {
        std::uint32_t version{2};
        PlayerMenuSettings player{};
        bool offRadar{};
        VehicleMenuSettings vehicle{};
        WeaponMenuSettings weapons{};
        UiMenuSettings ui{};
    };

    class MenuSettingsService final
    {
    public:
        static MenuSettingsService& Get() noexcept;

        bool Load(const std::filesystem::path& path) noexcept;
        bool Save(const std::filesystem::path& path) const noexcept;
        void Reset() noexcept;

        [[nodiscard]] const MenuSettingsData& Current() const noexcept;
        [[nodiscard]] MenuSettingsData& Current() noexcept;
        [[nodiscard]] bool IsLoaded() const noexcept;

    private:
        MenuSettingsService() = default;
        ~MenuSettingsService() = default;
        MenuSettingsService(const MenuSettingsService&) = delete;
        MenuSettingsService& operator=(const MenuSettingsService&) = delete;

        MenuSettingsData m_Settings{};
        bool m_Loaded{};
    };
}
