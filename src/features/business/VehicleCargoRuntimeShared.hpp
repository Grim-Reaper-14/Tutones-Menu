#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/Stats.hpp"
#include "../../game/VehicleNatives.hpp"
#include "../../game/script/ScriptGlobal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Tutones::Game::Business::VehicleCargoRuntimeShared
{
    struct WarehouseTarget final
    {
        float x{};
        float y{};
        float z{};
        float heading{};
    };

    inline constexpr std::size_t PlayerOrganizationGlobal = 1893070;
    inline constexpr std::size_t PlayerOrganizationEntrySize = 615;
    inline constexpr std::size_t CurrentActivityOffset = 10 + 33;
    inline constexpr std::size_t ContrabandOwnerPlayerOffset = 10 + 178;
    inline constexpr std::size_t VehicleExportOffset = 10 + 188;
    inline constexpr std::size_t ContrabandDeliveryTypeOffset = 10 + 468;
    inline constexpr std::size_t VehicleExportArraySize = 4;

    inline constexpr int SourceActivity = 178;
    inline constexpr int SellActivity = 188;
    inline constexpr int VehicleCargoDeliveryType = -81613951;
    inline constexpr int MaxPlayers = 32;

    // Rockstar freemode property cases 60..69. These are the actual
    // Vehicle Warehouse transition positions (f_6.f_28) and headings
    // (f_6.f_31), not the exterior camera/door helper positions.
    inline constexpr std::array<WarehouseTarget, 10> WarehouseTargets{{
        {-631.693f, -1778.812f, 22.980f, 36.36f},
        {1007.344f, -1854.104f, 30.055f, 84.24f},
        {-72.690f, -1820.721f, 25.960f, 138.96f},
        {36.290f, -1283.851f, 28.300f, -180.0f},
        {1213.935f, -1251.067f, 35.340f, -4.68f},
        {809.470f, -2222.665f, 28.602f, -95.04f},
        {1753.699f, -1649.109f, 111.650f, 98.28f},
        {144.163f, -3006.280f, 6.025f, -90.0f},
        {-522.064f, -2197.247f, 5.396f, 138.96f},
        {-1160.481f, -2162.972f, 12.411f, -23.04f},
    }};

    inline constexpr std::array<const char*, 32> Models{{
        "prototipo", "tyrus", "bestiagts", "t20", "sheava", "osiris", "fmj", "reaper",
        "pfister811", "alpha", "mamba", "tampa", "btype3", "feltzer3", "ztype", "tropos",
        "entityxf", "sultanrs", "zentorno", "omnis", "coquette3", "seven70", "verlierer2", "feltzer2",
        "coquette2", "cheetah", "nightshade", "banshee2", "turismor", "massacro", "sabregt2", "jester",
    }};

    inline constexpr std::array<const char*, 96> Plates{{
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
        "IML4TE", "0V3RFL0D", "W1DEB0Y",
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

    [[nodiscard]] inline std::string NormalizePlate(std::string_view plate)
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

    [[nodiscard]] inline int CurrentActivity(std::int64_t** pages, int playerId) noexcept
    {
        if (!pages || playerId < 0 || playerId >= MaxPlayers)
            return -1;

        const auto entry = Script::ScriptGlobal(PlayerOrganizationGlobal)
            .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize);
        const int* activity = entry.At(CurrentActivityOffset).As<int>(pages);
        return activity ? *activity : -1;
    }

    [[nodiscard]] inline int RequestedVariation(std::int64_t** pages, int playerId) noexcept
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

    [[nodiscard]] inline bool MatchesVariation(Vehicle vehicle, int variation) noexcept;
    [[nodiscard]] inline Vehicle CurrentPlayerVehicle() noexcept;

    // Mirrors the current Enhanced freemode func_8315/8316 warehouse check.
    // If Rockstar has already replicated the cargo owner/type, this is a pure
    // readiness check. The only repair path is entered after the player is
    // already sitting in the exact requested source model+plate; this prevents
    // source discovery from forcing the gate before the mission car exists.
    [[nodiscard]] inline bool RockstarWarehouseGateReady(std::int64_t** pages, int playerId) noexcept
    {
        if (!pages || playerId < 0 || playerId >= MaxPlayers)
            return false;

        const auto localEntry = Script::ScriptGlobal(PlayerOrganizationGlobal)
            .At(static_cast<std::size_t>(playerId), PlayerOrganizationEntrySize);
        int* ownerPlayer = localEntry.At(ContrabandOwnerPlayerOffset).As<int>(pages);
        if (!ownerPlayer)
            return false;

        int owner = *ownerPlayer;
        if (owner >= 0 && owner < MaxPlayers)
        {
            const auto ownerEntry = Script::ScriptGlobal(PlayerOrganizationGlobal)
                .At(static_cast<std::size_t>(owner), PlayerOrganizationEntrySize);
            const int* deliveryType = ownerEntry.At(ContrabandDeliveryTypeOffset).As<int>(pages);
            if (deliveryType && *deliveryType == VehicleCargoDeliveryType)
                return true;
        }

        if (CurrentActivity(pages, playerId) != SourceActivity)
            return false;

        const int variation = RequestedVariation(pages, playerId);
        const Vehicle currentVehicle = CurrentPlayerVehicle();
        if (variation <= 0 || currentVehicle == 0 || !MatchesVariation(currentVehicle, variation))
            return false;

        // The real source car is possessed. Repair only the two transient
        // freemode acceptance fields; activity 178 and warehouse inventory are
        // deliberately untouched.
        if (owner < 0 || owner >= MaxPlayers)
        {
            owner = playerId;
            *ownerPlayer = owner;
        }

        const auto ownerEntry = Script::ScriptGlobal(PlayerOrganizationGlobal)
            .At(static_cast<std::size_t>(owner), PlayerOrganizationEntrySize);
        int* deliveryType = ownerEntry.At(ContrabandDeliveryTypeOffset).As<int>(pages);
        if (!deliveryType)
            return false;

        if (*deliveryType != VehicleCargoDeliveryType)
            *deliveryType = VehicleCargoDeliveryType;

        return *ownerPlayer == owner && *deliveryType == VehicleCargoDeliveryType;
    }

    [[nodiscard]] inline bool ReadWarehouse(int& outProperty, int& outStock, WarehouseTarget& outTarget)
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

    [[nodiscard]] inline bool MatchesVariation(Vehicle vehicle, int variation) noexcept
    {
        if (vehicle == 0 || variation < 1 || variation > 96)
            return false;

        const auto exists = Natives::DoesEntityExist(vehicle);
        if (!exists || !*exists)
            return false;

        const std::size_t index = static_cast<std::size_t>(variation - 1);
        const auto model = Natives::GetEntityModel(vehicle);
        if (!model || *model != Stats::Detail::Joaat(Models[index / 3]))
            return false;

        const auto plate = VehicleNatives::GetVehicleNumberPlateText(vehicle);
        return plate && NormalizePlate(*plate) == Plates[index];
    }

    [[nodiscard]] inline Vehicle CurrentPlayerVehicle() noexcept
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
}
