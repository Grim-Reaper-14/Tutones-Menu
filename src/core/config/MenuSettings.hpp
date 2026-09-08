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
        bool ghostOrganization{};
        float runMultiplier{1.0f};
        float swimMultiplier{1.0f};
    };

    struct VehicleMenuSettings final
    {
        bool removeLscRestrictions{};
        bool enableDlcVehicles{};
        bool vehicleGodMode{};
        bool keepVehicleClean{};
        bool loweredStance{};
        bool hornBoost{};
        bool infiniteVehicleAmmo{};
        bool nitrousEnabled{};
        bool nitrousUnlimited{true};
        float nitrousLevel{2.5f};
        float nitrousPower{2.0f};
        bool suspensionLoweringEnabled{};
        float suspensionLoweringAmount{0.08f};
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
        bool proximityWarningsEnabled{true};
        bool restrictWatchlistedActions{true};
        bool autoWatchHighRisk{};
        float proximityRadius{150.0f};
    };

    struct ProtectionMenuSettings final
    {
        bool blockMalformed{true};
        bool blockForcedLeave{};
        bool blockKnownCrashes{true};
        bool blockSounds{};
        bool blockExplosions{};
        bool blockFire{};
        bool blockWeaponDamage{};
        bool blockRagdoll{};
        bool blockClearTasks{};
        bool blockPtfx{};
        bool blockScriptEvents{};
        bool blockMalformedScriptEvents{true};
    };

    struct SessionMenuSettings final
    {
        bool noIdle{};
    };

    struct BusinessMenuSettings final
    {
        bool vehicleCargoAutoSource{};
        bool vehicleCargoInstantGarage{};
        bool vehicleCargoInstantSell{};
    };

    struct RecoveryMenuSettings final
    {
        bool rpMultiplierEnabled{};
        float rpMultiplier{1.0f};
        bool casinoSlotRig{};
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
        float menuWidth{1460.0f};
        float menuHeight{820.0f};
    };

    struct MenuSettingsData final
    {
        std::uint32_t version{6};
        PlayerMenuSettings player{};
        bool offRadar{};
        VehicleMenuSettings vehicle{};
        WeaponMenuSettings weapons{};
        NetworkMenuSettings network{};
        ProtectionMenuSettings protections{};
        SessionMenuSettings session{};
        BusinessMenuSettings business{};
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
