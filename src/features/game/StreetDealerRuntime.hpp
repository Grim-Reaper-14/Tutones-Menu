#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/Natives.hpp"
#include "../../game/native/NativeInvoker.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::StreetDealer
{
    namespace Enhanced173
    {
        inline constexpr std::size_t FreemodeGlobal = 2733326;
        inline constexpr std::size_t DealerBlockOffset = 5635;
        inline constexpr std::size_t ActiveLocationOffset = DealerBlockOffset + 22;
        inline constexpr std::size_t ActiveDealerOffset = DealerBlockOffset + 23;
        inline constexpr std::size_t DealerStride = 7;
        inline constexpr std::size_t DealerCount = 3;

        inline constexpr std::size_t PremiumProductField = 1;
        inline constexpr std::size_t CocainePayoutField = 2;
        inline constexpr std::size_t MethPayoutField = 3;
        inline constexpr std::size_t WeedPayoutField = 4;
        inline constexpr std::size_t AcidPayoutField = 5;
        inline constexpr std::size_t CompletedField = 6;

        inline constexpr int ProductCocaine = 2;
        inline constexpr int ProductMeth = 3;
        inline constexpr int ProductWeed = 4;
        inline constexpr int ProductAcid = 7;
        inline constexpr int MaximumPlausiblePayout = 1000000;

        inline constexpr std::array<int, DealerCount> CompletionPackedStats{42076, 42077, 42078};

        struct Location final
        {
            float x{};
            float y{};
            float z{};
        };

        // Exact Street Dealer spawn coordinates from fm_street_dealer.c::func_359
        // for GTA V Enhanced 1.73 / b1158.13. Indices match the Rockstar location ID.
        inline constexpr std::array<Location, 50> Locations{{
            {550.8953f, -1774.5175f, 28.3121f},
            {-154.924f, 6434.428f, 30.916f},
            {400.9768f, 2635.3691f, 43.5045f},
            {1533.846f, 3796.837f, 33.456f},
            {-1666.642f, -1080.0201f, 12.1537f},
            {-1560.6105f, -413.3221f, 37.1001f},
            {819.2939f, -2988.8562f, 5.0209f},
            {1001.701f, -2162.448f, 29.567f},
            {1388.9678f, -1506.0815f, 57.0407f},
            {-3054.574f, 556.711f, 0.661f},
            {-72.8903f, 80.717f, 70.6161f},
            {198.6676f, -167.0663f, 55.3187f},
            {814.636f, -280.109f, 65.463f},
            {-237.004f, -256.513f, 38.122f},
            {-493.654f, -720.734f, 22.921f},
            {156.1586f, 6656.525f, 30.5882f},
            {1986.3129f, 3786.75f, 31.2791f},
            {-685.5629f, 5762.8706f, 16.511f},
            {1707.703f, 4924.311f, 41.078f},
            {1195.3047f, 2630.4685f, 36.81f},
            {167.0163f, 2228.922f, 89.7867f},
            {2724.0076f, 1483.066f, 23.5007f},
            {1594.9329f, 6452.817f, 24.3172f},
            {-2177.397f, 4275.945f, 48.12f},
            {-2521.249f, 2311.794f, 32.216f},
            {-3162.873f, 1115.6418f, 19.8526f},
            {-1145.026f, -2048.466f, 12.218f},
            {-1304.321f, -1318.848f, 3.88f},
            {-946.727f, 322.081f, 70.357f},
            {-895.112f, -776.624f, 14.91f},
            {-250.614f, -1527.617f, 30.561f},
            {-601.639f, -1026.49f, 21.55f},
            {2712.9868f, 4324.1157f, 44.8521f},
            {726.772f, 4169.101f, 39.709f},
            {178.3272f, 3086.2603f, 42.0742f},
            {2351.592f, 2524.249f, 46.694f},
            {388.9941f, 799.6882f, 186.6764f},
            {2587.9822f, 433.6803f, 107.6139f},
            {830.2875f, -1052.7747f, 27.6666f},
            {-759.662f, -208.396f, 36.271f},
            {-43.7171f, -2015.22f, 17.017f},
            {124.02f, -1039.884f, 28.213f},
            {479.0473f, -597.5507f, 27.4996f},
            {959.67f, 3619.036f, 31.668f},
            {2375.8994f, 3162.9954f, 47.2087f},
            {-1505.687f, 1526.558f, 114.257f},
            {645.737f, 242.173f, 101.153f},
            {1173.1378f, -388.2896f, 70.5896f},
            {-1801.85f, 172.49f, 67.771f},
            {3729.2568f, 4524.872f, 21.4755f},
        }};
    }

    [[nodiscard]] inline const char* ProductName(int productId) noexcept
    {
        switch (productId)
        {
        case Enhanced173::ProductCocaine: return "Cocaine";
        case Enhanced173::ProductMeth: return "Meth";
        case Enhanced173::ProductWeed: return "Weed";
        case Enhanced173::ProductAcid: return "Acid";
        default: return "Unknown";
        }
    }

    struct DealerRecord final
    {
        int premiumProduct{-1};
        int cocainePayout{};
        int methPayout{};
        int weedPayout{};
        int acidPayout{};
        bool completed{};
    };

    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool globalsReady{};
        bool layoutValid{};
        int activeLocation{-1};
        int activeDealer{-1};
        std::array<DealerRecord, Enhanced173::DealerCount> dealers{};
        std::string message{"Ready"};
    };

    struct TeleportSnapshot final
    {
        bool nativeReady{};
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        int lastLocation{-1};
        std::string message{"Ready"};
    };

    class Runtime final
    {
    public:
        static Runtime& Get() noexcept
        {
            static Runtime instance;
            return instance;
        }

        bool QueueRefresh()
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending("Reading Enhanced Street Dealer state");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading Street Dealer state");

                auto* pages = GamePointers::Get().ScriptGlobals();
                state.globalsReady = pages != nullptr;
                if (!state.globalsReady)
                    return Finish(false, std::move(state), "Script globals are unavailable");

                const Script::ScriptGlobal root(Enhanced173::FreemodeGlobal);
                const auto readInt = [pages](Script::ScriptGlobal global, int& output) -> bool
                {
                    const int* value = global.As<int>(pages);
                    if (!value)
                        return false;
                    output = *value;
                    return true;
                };

                if (!readInt(root.At(Enhanced173::ActiveLocationOffset), state.activeLocation) ||
                    !readInt(root.At(Enhanced173::ActiveDealerOffset), state.activeDealer))
                {
                    return Finish(false, std::move(state), "Unable to read Street Dealer globals");
                }

                const bool locationPlausible = state.activeLocation >= -1 && state.activeLocation <= 49;
                const bool dealerPlausible = state.activeDealer >= -1 &&
                    state.activeDealer < static_cast<int>(Enhanced173::DealerCount);
                if (!locationPlausible || !dealerPlausible)
                    return Finish(false, std::move(state), "Enhanced Street Dealer layout validation failed");

                const auto dealerArray = root.At(Enhanced173::DealerBlockOffset);
                for (std::size_t dealer = 0; dealer < state.dealers.size(); ++dealer)
                {
                    const auto record = dealerArray.At(dealer, Enhanced173::DealerStride);
                    auto& output = state.dealers[dealer];
                    int completed{};
                    if (!readInt(record.At(Enhanced173::PremiumProductField), output.premiumProduct) ||
                        !readInt(record.At(Enhanced173::CocainePayoutField), output.cocainePayout) ||
                        !readInt(record.At(Enhanced173::MethPayoutField), output.methPayout) ||
                        !readInt(record.At(Enhanced173::WeedPayoutField), output.weedPayout) ||
                        !readInt(record.At(Enhanced173::AcidPayoutField), output.acidPayout) ||
                        !readInt(record.At(Enhanced173::CompletedField), completed))
                    {
                        return Finish(false, std::move(state), "Unable to read Street Dealer record");
                    }

                    const bool premiumPlausible = output.premiumProduct == Enhanced173::ProductCocaine ||
                        output.premiumProduct == Enhanced173::ProductMeth ||
                        output.premiumProduct == Enhanced173::ProductWeed ||
                        output.premiumProduct == Enhanced173::ProductAcid;
                    const auto payoutPlausible = [](int value) noexcept
                    {
                        return value >= 0 && value <= Enhanced173::MaximumPlausiblePayout;
                    };
                    const bool payoutsPlausible = payoutPlausible(output.cocainePayout) &&
                        payoutPlausible(output.methPayout) && payoutPlausible(output.weedPayout) &&
                        payoutPlausible(output.acidPayout);
                    const bool completionPlausible = completed == 0 || completed == 1;
                    if (!premiumPlausible || !payoutsPlausible || !completionPlausible)
                        return Finish(false, std::move(state), "Enhanced Street Dealer record validation failed");

                    output.completed = completed != 0;
                }

                state.layoutValid = true;
                TUTONES_LOG_DEBUG("street_dealer", "Enhanced Street Dealer state refreshed");
                Finish(true, std::move(state), "Enhanced Street Dealer state refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
            return false;
        }

        bool QueueTeleport(std::size_t locationIndex)
        {
            if (locationIndex >= Enhanced173::Locations.size() || !Native::NativeRegistry::Get().IsReady())
                return false;

            bool expected = false;
            if (!m_TeleportPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_TeleportState.haveResult = false;
                m_TeleportState.lastSucceeded = false;
                m_TeleportState.lastLocation = static_cast<int>(locationIndex);
                m_TeleportState.message = "Teleporting to Street Dealer location " + std::to_string(locationIndex + 1);
            }

            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this, locationIndex] {
                TeleportToLocation(locationIndex);
            }))
            {
                return true;
            }

            FinishTeleport(false, locationIndex, "Game-thread queue unavailable");
            return false;
        }

        [[nodiscard]] Snapshot GetSnapshot() const
        {
            Snapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_State.haveResult;
            snapshot.lastSucceeded = m_State.lastSucceeded;
            snapshot.sessionStarted = m_State.sessionStarted;
            snapshot.globalsReady = m_State.globalsReady;
            snapshot.layoutValid = m_State.layoutValid;
            snapshot.activeLocation = m_State.activeLocation;
            snapshot.activeDealer = m_State.activeDealer;
            snapshot.dealers = m_State.dealers;
            snapshot.message = m_State.message;
            return snapshot;
        }

        [[nodiscard]] TeleportSnapshot GetTeleportSnapshot() const
        {
            TeleportSnapshot snapshot;
            snapshot.nativeReady = Native::NativeRegistry::Get().IsReady();
            snapshot.pending = m_TeleportPending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_TeleportState.haveResult;
            snapshot.lastSucceeded = m_TeleportState.lastSucceeded;
            snapshot.lastLocation = m_TeleportState.lastLocation;
            snapshot.message = m_TeleportState.message;
            return snapshot;
        }

    private:
        Runtime() = default;

        void TeleportToLocation(std::size_t locationIndex)
        {
            if (locationIndex >= Enhanced173::Locations.size())
                return FinishTeleport(false, locationIndex, "Street Dealer location index is invalid");
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return FinishTeleport(false, locationIndex, "Teleport natives are unavailable on the GTA game thread");

            const auto ped = Natives::PlayerPedId();
            if (!ped || *ped == 0)
                return FinishTeleport(false, locationIndex, "Local player ped is unavailable");

            const auto pedExists = Natives::DoesEntityExist(*ped);
            if (!pedExists || !*pedExists)
                return FinishTeleport(false, locationIndex, "Local player ped does not exist");

            Entity target = *ped;
            Vehicle vehicle{};
            bool inVehicle{};
            const auto insideVehicle = Natives::IsPedInAnyVehicle(*ped, true);
            if (insideVehicle && *insideVehicle)
            {
                const auto currentVehicle = Natives::GetVehiclePedIsIn(*ped, true);
                if (!currentVehicle || *currentVehicle == 0)
                    return FinishTeleport(false, locationIndex, "Current vehicle is unavailable");

                const auto vehicleExists = Natives::DoesEntityExist(*currentVehicle);
                if (!vehicleExists || !*vehicleExists)
                    return FinishTeleport(false, locationIndex, "Current vehicle does not exist");

                target = *currentVehicle;
                vehicle = *currentVehicle;
                inVehicle = true;
            }

            const auto& location = Enhanced173::Locations[locationIndex];
            const float destinationZ = location.z + (inVehicle ? 1.0f : 0.5f);

            if (!Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::RequestCollisionAtCoord,
                    location.x,
                    location.y,
                    destinationZ))
            {
                return FinishTeleport(false, locationIndex, "Could not request destination collision");
            }

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityVelocity,
                target,
                0.0f,
                0.0f,
                0.0f));

            if (!Native::NativeInvoker::InvokeVoid(
                    Native::NativeId::SetEntityCoordsNoOffset,
                    target,
                    location.x,
                    location.y,
                    destinationZ,
                    std::int32_t{1},
                    std::int32_t{1},
                    std::int32_t{1}))
            {
                return FinishTeleport(false, locationIndex, "Street Dealer teleport failed");
            }

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::RequestCollisionAtCoord,
                location.x,
                location.y,
                destinationZ));

            if (inVehicle)
                static_cast<void>(Natives::SetVehicleOnGroundProperly(vehicle, 5.0f));

            static_cast<void>(Native::NativeInvoker::InvokeVoid(
                Native::NativeId::SetEntityVelocity,
                target,
                0.0f,
                0.0f,
                0.0f));

            TUTONES_LOG_INFO(
                "street_dealer",
                std::string("Teleported to Enhanced Street Dealer location ") + std::to_string(locationIndex + 1));
            FinishTeleport(
                true,
                locationIndex,
                "Street Dealer location " + std::to_string(locationIndex + 1) + " teleport complete");
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_State.haveResult = false;
            m_State.lastSucceeded = false;
            m_State.message = std::move(message);
        }

        void Finish(bool success, Snapshot state, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                state.pending = false;
                state.haveResult = true;
                state.lastSucceeded = success;
                state.message = std::move(message);
                m_State = std::move(state);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        void FinishTeleport(bool success, std::size_t locationIndex, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_TeleportState.nativeReady = Native::NativeRegistry::Get().IsReady();
                m_TeleportState.pending = false;
                m_TeleportState.haveResult = true;
                m_TeleportState.lastSucceeded = success;
                m_TeleportState.lastLocation = locationIndex < Enhanced173::Locations.size()
                    ? static_cast<int>(locationIndex)
                    : -1;
                m_TeleportState.message = std::move(message);
            }
            m_TeleportPending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        std::atomic<bool> m_TeleportPending{false};
        mutable std::mutex m_Mutex;
        Snapshot m_State{};
        TeleportSnapshot m_TeleportState{};
    };
}
