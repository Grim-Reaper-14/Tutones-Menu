#pragma once

#include "../../game/Natives.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Tutones::Game::Mods
{
    enum class VehicleModAction : unsigned char
    {
        None,
        Repair,
        Clean,
        FlipUpright,
        MaxVehicle,
        SpawnVehicle,
        CloneNearest,
        SavePreset,
        LoadPreset,
        SetMod,
        RemoveMod,
        ToggleMod,
        SetWheelType,
        SetTireSmokeColor,
        SetXenonColor,
        SetNeonColor,
        SetNeonEnabled,
        SetTyresCanBurst,
        SetDriftTyres,
    };

    struct VehicleModificationSnapshot final
    {
        Vehicle vehicle{};
        int observedModType{};
        int modCount{};
        int currentMod{-1};
        bool customTires{};
        int wheelType{};
        bool turbo{};
        bool tireSmoke{};
        bool xenon{};
        std::vector<std::string> modDisplayNames{};

        int tireSmokeRed{255};
        int tireSmokeGreen{255};
        int tireSmokeBlue{255};
        int xenonColor{-1};
        std::array<bool, 4> neonEnabled{};
        int neonRed{222};
        int neonGreen{222};
        int neonBlue{255};
        bool tyresCanBurst{true};
        bool driftTyres{};
        bool valid{};

        bool spawnPending{};
        Hash pendingSpawnModel{};
        Vehicle lastSpawnedVehicle{};
        std::string lastSavedPreset{};

        VehicleModAction lastAction{VehicleModAction::None};
        bool lastActionSucceeded{};
        bool lastActionRejectedAsStale{};
    };

    struct VehicleCatalogSnapshot final
    {
        std::vector<int> classes{};
        std::vector<std::string> displayNames{};
        std::size_t ready{};
        std::size_t total{};
    };

    class VehicleModificationRuntime final
    {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr auto RefreshInterval = std::chrono::milliseconds{250};
        static constexpr auto SpawnTimeout = std::chrono::seconds{6};

        static VehicleModificationRuntime& Get() noexcept;

        bool Start();
        void Stop() noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;

        void SetObservedModType(int modType) noexcept;
        [[nodiscard]] VehicleModificationSnapshot Snapshot() const;
        [[nodiscard]] VehicleCatalogSnapshot CatalogSnapshot() const;
        [[nodiscard]] std::vector<std::string> SavedPresetNames() const;

        [[nodiscard]] bool QueueRepair();
        [[nodiscard]] bool QueueClean();
        [[nodiscard]] bool QueueFlipUpright();
        [[nodiscard]] bool QueueMaxVehicle();
        [[nodiscard]] bool QueueSpawnVehicle(std::string_view modelName, bool spawnInside, bool maxed);
        [[nodiscard]] bool QueueCloneNearest(bool spawnInside);
        [[nodiscard]] bool QueueSaveCurrentPreset(std::string_view presetName);
        [[nodiscard]] bool QueueLoadPreset(std::string_view presetName, bool spawnInside);
        [[nodiscard]] bool QueueSetMod(int modType, int modIndex, bool customTires);
        [[nodiscard]] bool QueueRemoveMod(int modType);
        [[nodiscard]] bool QueueToggleMod(int modType, bool enabled);
        [[nodiscard]] bool QueueWheelType(int wheelType);
        [[nodiscard]] bool QueueTireSmokeColor(int red, int green, int blue);
        [[nodiscard]] bool QueueXenonColor(int colorIndex);
        [[nodiscard]] bool QueueNeonColor(int red, int green, int blue);
        [[nodiscard]] bool QueueNeonEnabled(int index, bool enabled);
        [[nodiscard]] bool QueueTyresCanBurst(bool canBurst);
        [[nodiscard]] bool QueueDriftTyres(bool enabled);

    private:
        struct VehiclePreset final
        {
            Hash model{};
            int primary{};
            int secondary{};
            int pearlescent{};
            int wheelColor{};
            int primaryPaintType{};
            int primaryModColor{};
            int primaryModPearlescent{};
            int secondaryPaintType{};
            int secondaryModColor{};
            bool primaryCustom{};
            bool secondaryCustom{};
            std::array<int, 3> customPrimary{};
            std::array<int, 3> customSecondary{};
            int wheelType{};
            std::array<int, 50> mods{};
            std::array<bool, 50> variations{};
            std::array<bool, 50> toggles{};
            std::array<int, 3> tireSmoke{{255, 255, 255}};
            int xenonColor{-1};
            std::array<bool, 4> neonEnabled{};
            std::array<int, 3> neonColor{{222, 222, 255}};
            bool tyresCanBurst{true};
            bool driftTyres{};
        };

        VehicleModificationRuntime();
        ~VehicleModificationRuntime() = default;
        VehicleModificationRuntime(const VehicleModificationRuntime&) = delete;
        VehicleModificationRuntime& operator=(const VehicleModificationRuntime&) = delete;

        [[nodiscard]] Vehicle CurrentVehicle() const noexcept;
        bool QueueNextTick();
        void TickOnGameThread() noexcept;
        void ProcessPendingSpawn() noexcept;
        void ProcessCatalogBatch() noexcept;
        bool BeginPendingSpawn(Hash model, bool spawnInside, bool maxed, VehicleModAction action, std::optional<VehiclePreset> preset = std::nullopt) noexcept;
        bool Refresh(Vehicle vehicle) noexcept;
        bool MaxVehicle(Vehicle vehicle) noexcept;
        bool CapturePreset(Vehicle vehicle, VehiclePreset& out) noexcept;
        bool ApplyPreset(Vehicle vehicle, const VehiclePreset& preset) noexcept;
        bool SavePresetToDisk(std::string_view name, const VehiclePreset& preset) noexcept;
        bool LoadPresetFromDisk(std::string_view name, VehiclePreset& preset) const noexcept;
        bool QueueVehicleOperation(VehicleModAction action, std::function<bool(Vehicle)> apply);
        void RecordAction(VehicleModAction action, bool success, bool stale) noexcept;
        void SetSpawnPending(Hash model, bool pending) noexcept;
        void ClearSnapshot() noexcept;
        [[nodiscard]] static Hash Joaat(std::string_view text) noexcept;
        [[nodiscard]] static std::string SanitizePresetName(std::string_view text);

        std::atomic<bool> m_Running{false};
        std::atomic<int> m_ObservedModType{11};
        Vehicle m_LastVehicle{};
        int m_LastObservedModType{-1};
        Clock::time_point m_NextRefresh{};

        Hash m_PendingSpawnModel{};
        bool m_PendingSpawnInside{};
        bool m_PendingSpawnMaxed{};
        std::optional<VehiclePreset> m_PendingSpawnPreset{};
        VehicleModAction m_PendingSpawnAction{VehicleModAction::SpawnVehicle};
        Clock::time_point m_SpawnDeadline{};

        std::vector<int> m_CatalogClasses{};
        std::vector<std::string> m_CatalogDisplayNames{};
        std::size_t m_CatalogCursor{};

        mutable std::mutex m_Mutex;
        VehicleModificationSnapshot m_Snapshot{};
    };
}
