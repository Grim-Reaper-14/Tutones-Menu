#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Tutones::Core::Config
{
    struct PlayerMenuSettings final
    {
        bool invincible{};
        bool bulletproof{};
        bool aquaLungs{};
        bool infiniteOxygen{};
        bool invisible{};
        bool noRagdoll{};
        bool superJump{};
        bool infiniteStamina{};
        bool keepPlayerClean{};
        bool disableCriticalHits{};
        bool standOnVehicles{};
        bool disableActionMode{};
        bool infiniteParachutes{};
        bool mobileRadio{};
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

    struct NetworkMenuSettings final
    {
        bool silencePhoneCalls{};
        bool disableDeathBarriers{};
    };

    struct RecoveryMenuSettings final
    {
        bool rpMultiplierEnabled{};
        float rpMultiplier{1.0f};
    };

    struct WorldMenuSettings final
    {
        float pedDensity{1.0f};
        float scenarioPedDensity{1.0f};
        float vehicleDensity{1.0f};
        float randomVehicleDensity{1.0f};
        float parkedVehicleDensity{1.0f};
        bool freezeClock{};
        bool forceWeather{};
        bool blackout{};
        bool autoWaypoint{};
        // Crosshair inspection runs synchronous world shape tests. Keep it opt-in so
        // simply opening World > Entities never starts a native scan loop.
        bool entityInspectorLive{false};
        int setHour{12};
        int setMinute{};
        int weatherIndex{};
        float clearRadius{50.0f};
    };

    struct MiscMenuSettings final
    {
        bool showCoordinates{};
        bool showHeading{};
        bool showFps{};
        bool showSessionInfo{};
        bool disableCameraShake{};
    };

    struct UiMenuSettings final
    {
        std::string activeTheme{"default.json"};
        bool resizable{true};
        bool anchorTopLeft{true};
        float menuWidth{1120.0f};
        float menuHeight{754.0f};
    };

    struct MenuSettingsData final
    {
        std::uint32_t version{5};
        PlayerMenuSettings player{};
        bool offRadar{};
        VehicleMenuSettings vehicle{};
        WeaponMenuSettings weapons{};
        NetworkMenuSettings network{};
        RecoveryMenuSettings recovery{};
        WorldMenuSettings world{};
        MiscMenuSettings misc{};
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
