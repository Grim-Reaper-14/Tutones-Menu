#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::NetworkFeatures
{
    enum class RequestService : std::size_t
    {
        MOC,
        Avenger,
        Terrorbyte,
        Kosatka,
        Dinghy,
        AcidLab,
        AcidLabBike,
        BailOfficeTransporter,
        BallisticEquipment,
        AmmoDrop,
        RCBandito,
        RCTank,
        Taxi,
        BoatPickup,
        HelicopterPickup,
        BullSharkTestosterone,
        BackupHelicopter,
        CayoHelicopterBackup,
        Airstrike,
        Count,
    };

    struct RequestServiceDefinition final
    {
        const char* label;
        std::size_t offset;
    };

    inline constexpr std::size_t RequestServicesGlobal = 2733326;

    inline constexpr std::array<RequestServiceDefinition, static_cast<std::size_t>(RequestService::Count)> RequestServiceCatalog{{
        {"MOC", 577},
        {"Avenger", 585},
        {"Terrorbyte", 591},
        {"Kosatka", 613},
        {"Dinghy", 626},
        {"Acid Lab", 592},
        {"Acid Lab Bike", 648},
        {"Bail Office Transporter", 362},
        {"Ballistic Equipment", 548},
        {"Ammo Drop", 538},
        {"RC Bandito", 5832},
        {"RC Tank", 5833},
        {"Taxi", 509},
        {"Boat Pickup", 539},
        {"Helicopter Pickup", 540},
        {"Bull Shark Testosterone", 546},
        {"Backup Helicopter", 3579},
        {"Cayo Helicopter Backup", 490},
        {"Airstrike", 3580},
    }};

    struct RequestServicesSnapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        std::string message{"Ready"};
    };

    class RequestServicesRuntime final
    {
    public:
        static RequestServicesRuntime& Get() noexcept
        {
            static RequestServicesRuntime instance;
            return instance;
        }

        bool QueueRequest(RequestService service)
        {
            const auto index = static_cast<std::size_t>(service);
            if (index >= RequestServiceCatalog.size())
                return false;

            const auto definition = RequestServiceCatalog[index];
            return Queue(std::string(definition.label) + " request queued", [this, definition] {
                int* target = ResolveGlobal(definition.offset);
                if (!target)
                    return;

                *target = 1;
                const bool success = *target == 1;
                if (success)
                    TUTONES_LOG_INFO("network.request_services", std::string("Requested ") + definition.label + " via Global_2733326.f_" + std::to_string(definition.offset));
                Finish(success, success ? std::string(definition.label) + " request applied" : std::string(definition.label) + " request failed read-back verification");
            });
        }

        bool QueueSuperVolito()
        {
            return Queue("SuperVolito pickup request queued", [this] {
                int* superVolito = ResolveGlobal(547);
                int* helicopter = ResolveGlobal(540);
                if (!superVolito || !helicopter)
                    return;

                // Supplied Enhanced sequence: toggle SuperVolito first, then request pickup.
                *superVolito = 1;
                *helicopter = 1;
                const bool success = *superVolito == 1 && *helicopter == 1;
                if (success)
                    TUTONES_LOG_INFO("network.request_services", "Requested SuperVolito pickup via f_547 then f_540");
                Finish(success, success ? "SuperVolito pickup request applied" : "SuperVolito pickup request failed read-back verification");
            });
        }

        bool QueueBallisticInstantEquip()
        {
            return QueueSingleOffset("Ballistic instant equip", 549);
        }

        bool QueueBallisticInstantRemove()
        {
            return QueueSingleOffset("Ballistic instant remove", 550);
        }

        [[nodiscard]] RequestServicesSnapshot Snapshot() const
        {
            RequestServicesSnapshot snapshot;
            snapshot.pending = m_Pending.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.haveResult = m_HaveResult;
            snapshot.lastSucceeded = m_LastSucceeded;
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        RequestServicesRuntime() = default;

        bool QueueSingleOffset(const char* label, std::size_t offset)
        {
            return Queue(std::string(label) + " queued", [this, label, offset] {
                int* target = ResolveGlobal(offset);
                if (!target)
                    return;

                *target = 1;
                const bool success = *target == 1;
                if (success)
                    TUTONES_LOG_INFO("network.request_services", std::string(label) + " via Global_2733326.f_" + std::to_string(offset));
                Finish(success, success ? std::string(label) + " applied" : std::string(label) + " failed read-back verification");
            });
        }

        template<typename Callback>
        bool Queue(std::string pendingMessage, Callback&& callback)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            SetPending(std::move(pendingMessage));
            if (Runtime::GameRuntime::Get().Enqueue(std::forward<Callback>(callback)))
                return true;

            Finish(false, "Game-thread queue unavailable");
            return false;
        }

        int* ResolveGlobal(std::size_t offset)
        {
            bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            if (!sessionStarted || !*sessionStarted)
            {
                Finish(false, "Join GTA Online before requesting a service");
                return nullptr;
            }

            auto* pages = GamePointers::Get().ScriptGlobals();
            if (!pages)
            {
                Finish(false, "Enhanced script globals are unavailable");
                return nullptr;
            }

            int* target = Script::ScriptGlobal(RequestServicesGlobal).At(offset).As<int>(pages);
            if (!target)
                Finish(false, "Requested service global is unavailable");
            return target;
        }

        void SetPending(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_HaveResult = false;
            m_LastSucceeded = false;
            m_Message = std::move(message);
        }

        void Finish(bool success, std::string message)
        {
            {
                std::scoped_lock lock(m_Mutex);
                m_HaveResult = true;
                m_LastSucceeded = success;
                m_Message = std::move(message);
            }
            m_Pending.store(false, std::memory_order_release);
        }

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        bool m_HaveResult{};
        bool m_LastSucceeded{};
        std::string m_Message{"Ready"};
    };
}
