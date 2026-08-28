#pragma once

#include "BusinessScriptMonitorRuntime.hpp"
#include "VehicleCargoAutoSourceRuntime.hpp"
#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeHandlerValidation.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace Tutones::Game::Business
{
    struct VehicleCargoInstantGarageSnapshot final
    {
        bool enabled{};
        bool pending{};
        bool sessionReady{};
        bool warehouseReady{};
        bool missionRunning{};
        bool sourceVehicleReady{};
        bool deliveryIssued{};
        bool lastSucceeded{};
        int warehouseProperty{};
        int sourceVariation{};
        int warehouseStock{};
        std::string message{"Instant Source + Delivery is off"};
    };

    class VehicleCargoInstantGarageRuntime final
    {
    public:
        static VehicleCargoInstantGarageRuntime& Get() noexcept
        {
            static VehicleCargoInstantGarageRuntime instance;
            return instance;
        }

        void SetEnabled(bool enabled) noexcept
        {
            const bool previous = m_Enabled.exchange(enabled, std::memory_order_acq_rel);
            if (previous == enabled)
                return;

            if (enabled)
                VehicleCargoAutoSourceRuntime::Get().SetEnabled(false);

            m_NextPollMs.store(0, std::memory_order_release);
            if (!enabled)
            {
                m_ManualCycleActive.store(false, std::memory_order_release);
                ResetCycle();
            }

            std::scoped_lock lock(m_Mutex);
            m_Snapshot.message = enabled
                ? "Instant Source + Delivery armed; Tutones will acquire and store the exact sourced car automatically"
                : "Instant Source + Delivery is off";
        }

        [[nodiscard]] bool Enabled() const noexcept
        {
            return m_Enabled.load(std::memory_order_acquire);
        }

        bool QueueStoreNow()
        {
            m_ManualCycleActive.store(true, std::memory_order_release);
            return QueueEvaluate(true);
        }

        void Tick() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire)
                && !m_ManualCycleActive.load(std::memory_order_acquire))
            {
                return;
            }

            const auto now = NowMs();
            auto next = m_NextPollMs.load(std::memory_order_acquire);
            if (now < next)
                return;
            if (!m_NextPollMs.compare_exchange_strong(next, now + PollIntervalMs, std::memory_order_acq_rel))
                return;

            static_cast<void>(QueueEvaluate(false));
        }

        [[nodiscard]] VehicleCargoInstantGarageSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            auto out = m_Snapshot;
            out.enabled = m_Enabled.load(std::memory_order_acquire);
            out.pending = m_Pending.load(std::memory_order_acquire);
            return out;
        }

    private:
        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        enum DeliveryHandlerIndex : std::size_t
        {
            NetworkRequestControlOfEntity,
            NetworkHasControlOfEntity,
            RequestCollision,
            SetEntityCoords,
            FreezeEntity,
            SetEntityVelocity,
            DeliveryHandlerCount,
        };

        struct WarehouseTarget final
        {
            float x{};
            float y{};
            float z{};
        };

        static constexpr std::array<std::uint64_t, DeliveryHandlerCount> DeliveryHandlerHashes{
            0xF093E270C0B6B318ull, // NETWORK_REQUEST_CONTROL_OF_ENTITY
            0x1B1A446EFA398EB5ull, // NETWORK_HAS_CONTROL_OF_ENTITY
            0xEA2D52183C7EA9CFull, // REQUEST_COLLISION_AT_COORD
            0x62C438C53BB57AFDull, // SET_ENTITY_COORDS_NO_OFFSET
            0x5D7CD709B34C90F0ull, // FREEZE_ENTITY_POSITION
            0x1AB7223AC0702871ull, // SET_ENTITY_VELOCITY
        };

        static constexpr std::array<WarehouseTarget, 10> WarehouseTargets{{
            {-631.693f, -1778.812f, 22.980f},
            {1007.344f, -1854.104f, 30.055f},
            {-72.690f, -1820.721f, 25.960f},
            {36.290f, -1283.851f, 28.300f},
            {1213.935f, -1251.067f, 35.340f},
            {808.9337f, -2226.6355f, 30.5702f},
            {1755.0826f, -1652.7717f, 113.9896f},
            {144.163f, -3006.280f, 6.025f},
            {-514.9109f, -2200.7783f, 8.504f},
            {-1157.2069f, -2167.5227f, 14.6173f},
        }};

        // Rotating search grid used after GB_VEHICLE_EXPORT has spawned the exact
        // requested model/plate. Only a vehicle whose model AND Import/Export plate
        // match the requested variation can ever be selected.
        static constexpr std::array<WarehouseTarget, 35> SourceSearchPoints{{
            {-3000.0f, -3000.0f, 80.0f}, {-1500.0f, -3000.0f, 80.0f}, {0.0f, -3000.0f, 80.0f}, {1500.0f, -3000.0f, 80.0f}, {3000.0f, -3000.0f, 80.0f},
            {-3000.0f, -1500.0f, 80.0f}, {-1500.0f, -1500.0f, 80.0f}, {0.0f, -1500.0f, 80.0f}, {1500.0f, -1500.0f, 80.0f}, {3000.0f, -1500.0f, 80.0f},
            {-3000.0f, 0.0f, 100.0f}, {-1500.0f, 0.0f, 100.0f}, {0.0f, 0.0f, 100.0f}, {1500.0f, 0.0f, 100.0f}, {3000.0f, 0.0f, 100.0f},
            {-3000.0f, 1500.0f, 120.0f}, {-1500.0f, 1500.0f, 120.0f}, {0.0f, 1500.0f, 120.0f}, {1500.0f, 1500.0f, 120.0f}, {3000.0f, 1500.0f, 120.0f},
            {-3000.0f, 3000.0f, 140.0f}, {-1500.0f, 3000.0f, 140.0f}, {0.0f, 3000.0f, 140.0f}, {1500.0f, 3000.0f, 140.0f}, {3000.0f, 3000.0f, 140.0f},
            {-3000.0f, 4500.0f, 120.0f}, {-1500.0f, 4500.0f, 120.0f}, {0.0f, 4500.0f, 120.0f}, {1500.0f, 4500.0f, 120.0f}, {3000.0f, 4500.0f, 120.0f},
            {-3000.0f, 6000.0f, 80.0f}, {-1500.0f, 6000.0f, 80.0f}, {0.0f, 6000.0f, 80.0f}, {1500.0f, 6000.0f, 80.0f}, {3000.0f, 6000.0f, 80.0f},
        }};

        static constexpr std::array<const char*, 32> Models{{
            "prototipo", "tyrus", "bestiagts", "t20", "sheava", "osiris", "fmj", "reaper",
            "pfister811", "alpha", "mamba", "tampa", "btype3", "feltzer3", "ztype", "tropos",
            "entityxf", "sultanrs", "zentorno", "omnis", "coquette3", "seven70", "verlierer2", "feltzer2",
            "coquette2", "cheetah", "nightshade", "banshee2", "turismor", "massacro", "sabregt2", "jester",
        }};

        static constexpr std::array<const char*, 96> Plates{{
            "FUTUR3", "M4K3B4NK", "TURB0",
            "C1TRUS", "B35TL4P", "TR3X",
            "BE4STY", "5T34LTH", "5M00TH",
            "CAR4M3L", "T0PSP33D", "D3V1L",
            "B1GB0Y", "M0N4RCH", "PR3TTY",
            "OH3LL0", "PH4R40H", "SL33K",
            "C4TCHM3", "J0K3R", "H0T4U",
            "2FA5T4U", "D34TH4U", "GR1M",
            "M1DL1F3", "R3G4L", "SL1CK",
            "V1S1ONRY", "L0NG80Y", "R31GN",
            "0LDBLU3", "BLKM4MB4", "V1P",
            "CH4RG3D", "CRU151N", "MU5CL3",
            "L4WLE55", "0LDT1M3R", "V4L0R",
            "M4J3ST1C", "T0UR3R", "R4LLY",
            "B1GMON3Y", "K1NGP1N", "CE0",
            "1MS0RAD", "31GHT135", "1985",
            "IML4TE", "0V3RFL0D", "W1DEB0D",
            "SN0WFLK3", "F1D3L1TY", "5H0W0FF",
            "W1NN1NG", "0LDN3W5", "H3R0",
            "0BEYM3", "W1D3B0D", "D1RTY",
            "V1NT4G3", "W1P30UT", "BLKF1N",
            "FRU1TY", "4LL0Y5", "SP33DY",
            "PR3C1OUS", "0UTFR0NT", "CURV35",
            "P0W3RFUL", "K3YL1M3", "R4C3R",
            "T0PL3SS", "T0FF33", "CL45SY",
            "BUZZ3D", "M1DN1GHT", "B1GC4T",
            "DE4DLY", "TH37OS", "E4TM3",
            "DR1FT3R", "D0M1N0", "H0WL3R",
            "IN4H4ZE", "M1LKYW4Y", "TPD4WG",
            "TR0P1CAL", "B4N4N4", "B055",
            "GUNZ0UT", "0R1G1N4L", "B0UNC3",
            "H0TP1NK", "T0PCL0WN", "NOF00L",
        }};

        static constexpr std::size_t PlayerOrganizationGlobal = 1893070;
        static constexpr std::size_t PlayerOrganizationEntrySize = 615;
        static constexpr std::size_t CurrentActivityOffset = 10 + 33;
        static constexpr std::size_t VehicleExportOffset = 10 + 188;
        static constexpr std::size_t VehicleExportArraySize = 4;
        static constexpr int SourceActivity = 178;
        static constexpr int MaxPlayers = 32;

        static constexpr std::int64_t PollIntervalMs = 250;
        static constexpr std::int64_t SourceLaunchTimeoutMs = 20000;
        static constexpr std::int64_t AcquireRetryMs = 500;
        static constexpr std::int64_t AcquireSettleMs = 750;
        static constexpr std::int64_t DeliveryRetryMs = 1500;
        static constexpr std::int64_t NextSourceDelayMs = 2000;
        static constexpr float SourceSearchRadius = 2600.0f;
        static constexpr int SearchPointsPerTick = 8;
        static constexpr int MaxControlAttempts = 40;
        static constexpr int MaxDeliveryAttempts = 5;

        VehicleCargoInstantGarageRuntime() = default;

        [[nodiscard]] static std::int64_t NowMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] static std::string NormalizePlate(std::string_view plate)
        {
            std::string out;
            out.reserve(plate.size());
            for (char c : plate)
            {
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                    continue;
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
                out.push_back(c);
            }
            return out;
        }

        [[nodiscard]] static bool ReadWarehouse(int& outProperty, int& outStock, WarehouseTarget& outTarget)
        {
            outProperty = 0;
            outStock = 0;

            const auto property = Stats::GetInt("MPX_PROP_IE_WAREHOUSE");
            if (!property || *property < 115 || *property > 124)
                return false;

            outProperty = *property;
            outTarget = WarehouseTargets[static_cast<std::size_t>(*property - 115)];

            for (int slot = 0; slot < 40; ++slot)
            {
                const auto value = Stats::GetInt(
                    std::string("MPX_IE_WH_OWNED_VEHICLE_") + std::to_string(slot));
                if (value && *value != 0)
                    ++outStock;
            }
            return true;
        }

        [[nodiscard]] static int CurrentActivity(std::int64_t** pages, int playerId) noexcept
        {
            if (!pages || playerId < 0 || playerId >= MaxPlayers)
                return -1;

            const auto entry = Script::ScriptGlobal(PlayerOrganizationGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize);
            const int* activity = entry.At(CurrentActivityOffset).As<int>(pages);
            return activity ? *activity : -1;
        }

        [[nodiscard]] static int RequestedVariation(std::int64_t** pages, int playerId) noexcept
        {
            if (!pages || playerId < 0 || playerId >= MaxPlayers)
                return 0;

            const auto entry = Script::ScriptGlobal(PlayerOrganizationGlobal)
                .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize);
            const auto exportArray = entry.At(VehicleExportOffset);
            const int* count = exportArray.As<int>(pages);
            const int* variation = exportArray.At(0, 1).As<int>(pages);
            if (!count || *count != static_cast<int>(VehicleExportArraySize) || !variation)
                return 0;
            return *variation >= 1 && *variation <= 96 ? *variation : 0;
        }

        [[nodiscard]] static bool MatchesVariation(Vehicle vehicle, int variation) noexcept
        {
            if (vehicle == 0 || variation < 1 || variation > 96)
                return false;

            const auto exists = Natives::DoesEntityExist(vehicle);
            if (!exists || !*exists)
                return false;

            const auto model = Natives::GetEntityModel(vehicle);
            const auto plate = VehicleNatives::GetVehicleNumberPlateText(vehicle);
            if (!model || !plate)
                return false;

            const std::size_t index = static_cast<std::size_t>(variation - 1);
            const std::uint32_t expectedModel = Stats::Detail::Joaat(Models[index / 3]);
            return *model == expectedModel && NormalizePlate(*plate) == Plates[index];
        }

        [[nodiscard]] static Vehicle CurrentPlayerVehicle() noexcept
        {
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return 0;

            const auto inVehicle = Natives::IsPedInAnyVehicle(*ped, false);
            if (!inVehicle || !*inVehicle)
                return 0;

            const auto vehicle = VehicleNatives::GetVehiclePedIsUsing(*ped);
            return vehicle ? *vehicle : 0;
        }

        [[nodiscard]] Vehicle FindSourceVehicleAnywhere(int variation) noexcept
        {
            if (variation < 1 || variation > 96)
                return 0;

            const Vehicle current = CurrentPlayerVehicle();
            if (current != 0 && MatchesVariation(current, variation))
                return current;

            if (m_SourceVehicleCandidate != 0 && MatchesVariation(m_SourceVehicleCandidate, variation))
                return m_SourceVehicleCandidate;

            const std::size_t variationIndex = static_cast<std::size_t>(variation - 1);
            const Hash expectedModel = static_cast<Hash>(Stats::Detail::Joaat(Models[variationIndex / 3]));

            const auto ped = Natives::PlayerPedId();
            if (ped && *ped != 0)
            {
                const auto coords = VehicleNatives::GetEntityCoords(*ped, false);
                if (coords)
                {
                    const auto nearPlayer = VehicleNatives::GetClosestVehicle(
                        coords->x, coords->y, coords->z, 12000.0f, expectedModel, 70);
                    if (nearPlayer && *nearPlayer != 0 && MatchesVariation(*nearPlayer, variation))
                    {
                        m_SourceVehicleCandidate = *nearPlayer;
                        return *nearPlayer;
                    }
                }
            }

            for (int i = 0; i < SearchPointsPerTick; ++i)
            {
                const std::size_t index = m_SourceSearchCursor++ % SourceSearchPoints.size();
                const auto& point = SourceSearchPoints[index];
                const auto candidate = VehicleNatives::GetClosestVehicle(
                    point.x, point.y, point.z, SourceSearchRadius, expectedModel, 70);
                if (!candidate || *candidate == 0 || !MatchesVariation(*candidate, variation))
                    continue;

                m_SourceVehicleCandidate = *candidate;
                return *candidate;
            }

            return 0;
        }

        [[nodiscard]] bool ResolveDeliveryHandlers() noexcept
        {
            bool ready = true;
            for (const auto handler : m_DeliveryHandlers)
                ready = ready && handler != nullptr;
            if (ready)
                return true;

            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            auto slots = DeliveryHandlerHashes;
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            return Native::AssignValidatedHandlers(slots, m_DeliveryHandlers);
        }

        template<typename Ret, typename... Args>
        [[nodiscard]] bool CallDelivery(std::size_t index, Ret& out, Args... args) const noexcept
        {
            if (index >= m_DeliveryHandlers.size() || !m_DeliveryHandlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_DeliveryHandlers[index](&context);
            context.FixVectors();
            out = context.GetReturnValue<Ret>();
            return true;
        }

        template<typename... Args>
        [[nodiscard]] bool CallDeliveryVoid(std::size_t index, Args... args) const noexcept
        {
            if (index >= m_DeliveryHandlers.size() || !m_DeliveryHandlers[index])
                return false;

            Native::CallContext context;
            if (!(context.PushArg(args) && ...))
                return false;
            m_DeliveryHandlers[index](&context);
            context.FixVectors();
            return true;
        }

        [[nodiscard]] bool EnsureVehicleControl(Vehicle vehicle) noexcept
        {
            if (vehicle == 0 || !ResolveDeliveryHandlers())
                return false;

            std::int32_t hasControl{};
            if (!CallDelivery(NetworkHasControlOfEntity, hasControl, vehicle))
                return false;

            if (hasControl != 0)
            {
                m_ControlAttempts = 0;
                return true;
            }

            std::int32_t requested{};
            static_cast<void>(CallDelivery(NetworkRequestControlOfEntity, requested, vehicle));
            ++m_ControlAttempts;
            return false;
        }

        [[nodiscard]] bool AcquireSourceVehicle(Vehicle vehicle, std::int64_t now) noexcept
        {
            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0 || vehicle == 0)
                return false;

            if (CurrentPlayerVehicle() == vehicle)
            {
                if (m_AcquiredAtMs == 0)
                    m_AcquiredAtMs = now;
                return true;
            }

            if ((now - m_LastAcquireAttemptMs) < AcquireRetryMs)
                return false;

            m_LastAcquireAttemptMs = now;
            static_cast<void>(VehicleNatives::SetPedIntoVehicle(*ped, vehicle, -1));
            return false;
        }

        [[nodiscard]] bool DeliverToWarehouse(Vehicle vehicle, const WarehouseTarget& target) noexcept
        {
            const auto exists = Natives::DoesEntityExist(vehicle);
            if (vehicle == 0 || !exists || !*exists || !ResolveDeliveryHandlers())
                return false;

            const float z = target.z + 0.35f;
            static_cast<void>(CallDeliveryVoid(RequestCollision, target.x, target.y, z));
            static_cast<void>(CallDeliveryVoid(FreezeEntity, vehicle, std::int32_t{1}));

            const bool moved = CallDeliveryVoid(
                SetEntityCoords,
                vehicle,
                target.x,
                target.y,
                z,
                std::int32_t{1},
                std::int32_t{1},
                std::int32_t{1});

            static_cast<void>(CallDeliveryVoid(SetEntityVelocity, vehicle, 0.0f, 0.0f, 0.0f));
            static_cast<void>(CallDeliveryVoid(RequestCollision, target.x, target.y, z));
            static_cast<void>(CallDeliveryVoid(FreezeEntity, vehicle, std::int32_t{0}));
            return moved;
        }

        bool QueueEvaluate(bool manual)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            if (Runtime::GameRuntime::Get().Enqueue([this, manual] { Evaluate(manual); }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            StoreSnapshot(false, false, false, false, false, false, 0, 0, 0,
                "Game-thread queue unavailable");
            return false;
        }

        void ResetCycle() noexcept
        {
            m_LaunchRequested = false;
            m_LaunchRequestedAtMs = 0;
            m_MissionWasRunning = false;
            m_DeliveryIssued = false;
            m_DeliveryAttempts = 0;
            m_LastDeliveryAtMs = 0;
            m_LastDeliveredVehicle = 0;
            m_RequestedVariation = 0;
            m_SourceVehicleCandidate = 0;
            m_SourceSearchCursor = 0;
            m_ControlAttempts = 0;
            m_LastAcquireAttemptMs = 0;
            m_AcquiredAtMs = 0;
        }

        void CompleteOneShot() noexcept
        {
            if (!m_Enabled.load(std::memory_order_acquire))
                m_ManualCycleActive.store(false, std::memory_order_release);
        }

        void Evaluate(bool manual)
        {
            if (manual)
                m_ManualCycleActive.store(true, std::memory_order_release);

            const bool active = m_Enabled.load(std::memory_order_acquire)
                || m_ManualCycleActive.load(std::memory_order_acquire);
            if (!active)
                return Finish(true, false, false, false, false, false, 0, 0, 0, "Instant Source + Delivery is off");

            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                ResetCycle();
                CompleteOneShot();
                return Finish(false, false, false, false, false, false, 0, 0, 0,
                    "Join GTA Online before using Instant Source + Delivery");
            }

            auto& scripts = Script::ScriptRuntime::Get();
            if (!scripts.IsReady())
                return Finish(false, true, false, false, false, false, 0, 0, 0,
                    "Enhanced script runtime unavailable");

            const auto playerId = Native::NativeInvoker::Invoke<std::int32_t>(Native::NativeId::PlayerId);
            if (!playerId || *playerId < 0 || *playerId >= MaxPlayers)
                return Finish(false, true, false, false, false, false, 0, 0, 0, "PLAYER_ID unavailable");

            WarehouseTarget warehouse{};
            int warehouseProperty = 0;
            int warehouseStock = 0;
            if (!ReadWarehouse(warehouseProperty, warehouseStock, warehouse))
            {
                ResetCycle();
                CompleteOneShot();
                return Finish(false, true, false, false, false, false, 0, 0, 0,
                    "Purchase a Vehicle Warehouse before using Instant Source + Delivery");
            }

            if (warehouseStock >= 40)
            {
                m_Enabled.store(false, std::memory_order_release);
                ResetCycle();
                CompleteOneShot();
                return Finish(false, true, true, false, false, false,
                    warehouseProperty, 0, warehouseStock, "Vehicle Warehouse is full (40/40)");
            }

            const auto* cargo = scripts.FindThread(BusinessScriptMonitorRuntime::VehicleCargoScriptHash);
            const bool cargoRunning = cargo && cargo->stack;
            const auto now = NowMs();
            auto* pages = GamePointers::Get().ScriptGlobals();
            const int activity = CurrentActivity(pages, *playerId);

            if (!cargoRunning)
            {
                if (m_MissionWasRunning)
                {
                    ResetCycle();
                    m_NotBeforeMs = now + NextSourceDelayMs;
                    CompleteOneShot();
                    return Finish(true, true, true, false, false, false,
                        warehouseProperty, 0, warehouseStock,
                        "Vehicle Cargo mission ended; Rockstar owns the warehouse save");
                }

                if (m_LaunchRequested)
                {
                    if ((now - m_LaunchRequestedAtMs) < SourceLaunchTimeoutMs)
                    {
                        return Finish(true, true, true, false, false, false,
                            warehouseProperty, m_RequestedVariation, warehouseStock,
                            "Real Vehicle Cargo source request sent; waiting for GB_VEHICLE_EXPORT");
                    }

                    ResetCycle();
                    m_NotBeforeMs = now + NextSourceDelayMs;
                    if (!m_Enabled.load(std::memory_order_acquire))
                        CompleteOneShot();
                    return Finish(false, true, true, false, false, false,
                        warehouseProperty, 0, warehouseStock,
                        "Vehicle Cargo source did not start; request reset for retry");
                }

                if (now < m_NotBeforeMs)
                    return Finish(true, true, true, false, false, false,
                        warehouseProperty, 0, warehouseStock,
                        "Instant Source waiting before the next source request");

                auto& autoSource = VehicleCargoAutoSourceRuntime::Get();
                autoSource.SetEnabled(false);
                if (!autoSource.QueueSourceNow())
                    return Finish(false, true, true, false, false, false,
                        warehouseProperty, 0, warehouseStock,
                        "Vehicle Cargo source queue is busy; retrying shortly");

                m_LaunchRequested = true;
                m_LaunchRequestedAtMs = now;
                return Finish(true, true, true, false, false, false,
                    warehouseProperty, 0, warehouseStock,
                    "Starting a real Vehicle Cargo source mission");
            }

            m_MissionWasRunning = true;
            m_LaunchRequested = true;

            if (activity != SourceActivity)
            {
                return Finish(true, true, true, true, false, false,
                    warehouseProperty, 0, warehouseStock,
                    "A Vehicle Cargo export/sell mission is running; Instant Source is standing by");
            }

            const int variation = RequestedVariation(pages, *playerId);
            if (variation > 0)
                m_RequestedVariation = variation;

            if (m_RequestedVariation <= 0)
            {
                return Finish(false, true, true, true, false, false,
                    warehouseProperty, 0, warehouseStock,
                    "Source mission is running, but its VehicleExport variation is not available yet");
            }

            const Vehicle sourceVehicle = FindSourceVehicleAnywhere(m_RequestedVariation);
            if (sourceVehicle == 0)
            {
                return Finish(true, true, true, true, false, false,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    std::string("Instant Source scanning for exact variation ")
                        + std::to_string(m_RequestedVariation)
                        + " model/plate anywhere on the map");
            }

            m_SourceVehicleCandidate = sourceVehicle;

            if (!EnsureVehicleControl(sourceVehicle))
            {
                if (m_ControlAttempts >= MaxControlAttempts)
                {
                    m_SourceVehicleCandidate = 0;
                    m_ControlAttempts = 0;
                    return Finish(false, true, true, true, false, false,
                        warehouseProperty, m_RequestedVariation, warehouseStock,
                        "Exact source car found, but network control timed out; rescanning");
                }

                return Finish(true, true, true, true, false, false,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    std::string("Exact source car found; requesting network control (")
                        + std::to_string(m_ControlAttempts) + "/" + std::to_string(MaxControlAttempts) + ")");
            }

            if (!AcquireSourceVehicle(sourceVehicle, now))
            {
                return Finish(true, true, true, true, false, false,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    "Instant Source found the exact mission car; warping you into the driver seat");
            }

            if ((now - m_AcquiredAtMs) < AcquireSettleMs)
            {
                return Finish(true, true, true, true, true, false,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    "Instant Source acquired the exact car; letting GB_VEHICLE_EXPORT register possession");
            }

            const bool canDeliver = !m_DeliveryIssued
                || (sourceVehicle == m_LastDeliveredVehicle
                    && m_DeliveryAttempts < MaxDeliveryAttempts
                    && (now - m_LastDeliveryAtMs) >= DeliveryRetryMs);

            if (!canDeliver)
            {
                return Finish(true, true, true, true, true, m_DeliveryIssued,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    "Source vehicle acquired; waiting for Rockstar's warehouse delivery trigger");
            }

            if (!DeliverToWarehouse(sourceVehicle, warehouse))
            {
                return Finish(false, true, true, true, true, false,
                    warehouseProperty, m_RequestedVariation, warehouseStock,
                    "Source vehicle acquired, but the Enhanced delivery teleport natives are unavailable");
            }

            m_DeliveryIssued = true;
            m_LastDeliveredVehicle = sourceVehicle;
            m_LastDeliveryAtMs = now;
            ++m_DeliveryAttempts;

            TUTONES_LOG_INFO("business.vehicle_cargo",
                std::string("Instant source + delivery: variation=") + std::to_string(m_RequestedVariation)
                    + " property=" + std::to_string(warehouseProperty)
                    + " stock=" + std::to_string(warehouseStock)
                    + " attempt=" + std::to_string(m_DeliveryAttempts));

            Finish(true, true, true, true, true, true,
                warehouseProperty, m_RequestedVariation, warehouseStock,
                std::string("Instant Source acquired variation ")
                    + std::to_string(m_RequestedVariation)
                    + " and delivered it to your Vehicle Warehouse; Rockstar will perform the actual storage/save.");
        }

        void StoreSnapshot(
            bool success,
            bool sessionReady,
            bool warehouseReady,
            bool missionRunning,
            bool sourceVehicleReady,
            bool deliveryIssued,
            int warehouseProperty,
            int sourceVariation,
            int warehouseStock,
            std::string message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.sessionReady = sessionReady;
            m_Snapshot.warehouseReady = warehouseReady;
            m_Snapshot.missionRunning = missionRunning;
            m_Snapshot.sourceVehicleReady = sourceVehicleReady;
            m_Snapshot.deliveryIssued = deliveryIssued;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.warehouseProperty = warehouseProperty;
            m_Snapshot.sourceVariation = sourceVariation;
            m_Snapshot.warehouseStock = warehouseStock;
            m_Snapshot.message = std::move(message);
        }

        void Finish(
            bool success,
            bool sessionReady,
            bool warehouseReady,
            bool missionRunning,
            bool sourceVehicleReady,
            bool deliveryIssued,
            int warehouseProperty,
            int sourceVariation,
            int warehouseStock,
            std::string message) noexcept
        {
            StoreSnapshot(success, sessionReady, warehouseReady, missionRunning,
                sourceVehicleReady, deliveryIssued, warehouseProperty,
                sourceVariation, warehouseStock, std::move(message));
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Enabled{false};
        std::atomic<bool> m_ManualCycleActive{false};
        std::atomic<bool> m_Pending{false};
        std::atomic<std::int64_t> m_NextPollMs{0};

        bool m_LaunchRequested{};
        std::int64_t m_LaunchRequestedAtMs{};
        bool m_MissionWasRunning{};
        bool m_DeliveryIssued{};
        int m_DeliveryAttempts{};
        std::int64_t m_LastDeliveryAtMs{};
        std::int64_t m_NotBeforeMs{};
        Vehicle m_LastDeliveredVehicle{};
        int m_RequestedVariation{};

        Vehicle m_SourceVehicleCandidate{};
        std::size_t m_SourceSearchCursor{};
        int m_ControlAttempts{};
        std::int64_t m_LastAcquireAttemptMs{};
        std::int64_t m_AcquiredAtMs{};

        std::array<Native::NativeHandler, DeliveryHandlerCount> m_DeliveryHandlers{};

        mutable std::mutex m_Mutex;
        VehicleCargoInstantGarageSnapshot m_Snapshot{};
    };
}
