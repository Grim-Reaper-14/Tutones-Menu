#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptRuntime.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::EnhancedScripts
{
    namespace Detail
    {
        [[nodiscard]] constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                hash += static_cast<std::uint8_t>(c);
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }
    }

    enum class Area : std::uint8_t
    {
        Business,
        StreetDealer,
        Services,
        Mission,
        Daily,
        Property,
        Dlc,
    };

    struct Definition final
    {
        const char* semanticName{};
        const char* label{};
        const char* script{};
        const char* decompileFile{};
        Area area{};
        std::uint32_t hash{};
    };

    inline constexpr std::array Definitions{
        Definition{"Business.Nightclub.Controller", "Nightclub / Business Hub", "am_mp_business_hub", "am_mp_business_hub.c", Area::Business, Detail::Joaat("am_mp_business_hub")},
        Definition{"Business.Bunker.Controller", "Bunker", "am_mp_bunker", "am_mp_bunker.c", Area::Business, Detail::Joaat("am_mp_bunker")},
        Definition{"Business.AcidLab.Controller", "Acid Lab", "am_mp_acid_lab", "am_mp_acid_lab.c", Area::Business, Detail::Joaat("am_mp_acid_lab")},
        Definition{"Business.BikerWarehouse.Controller", "Biker Warehouse", "am_mp_biker_warehouse", "am_mp_biker_warehouse.c", Area::Business, Detail::Joaat("am_mp_biker_warehouse")},
        Definition{"Activity.StreetDealer.Controller", "Street Dealer", "fm_street_dealer", "fm_street_dealer.c", Area::StreetDealer, Detail::Joaat("fm_street_dealer")},
        Definition{"Services.ContactRequests.Controller", "Contact Requests", "am_contact_requests", "am_contact_requests.c", Area::Services, Detail::Joaat("am_contact_requests")},
        Definition{"Mission.Launch.Controller", "Mission Launch", "am_mission_launch", "am_mission_launch.c", Area::Mission, Detail::Joaat("am_mission_launch")},
        Definition{"Mission.Freemode.Controller", "Freemode", "freemode", "freemode.c", Area::Mission, Detail::Joaat("freemode")},
        Definition{"Activity.Daily.Freemode", "Daily / Freemode Activity Host", "freemode", "freemode.c", Area::Daily, Detail::Joaat("freemode")},
        Definition{"Property.AutoShop.Controller", "Auto Shop", "am_mp_auto_shop", "am_mp_auto_shop.c", Area::Property, Detail::Joaat("am_mp_auto_shop")},
        Definition{"Property.BailOffice.Controller", "Bail Office", "am_mp_bail_office", "am_mp_bail_office.c", Area::Property, Detail::Joaat("am_mp_bail_office")},
        Definition{"Property.Arcade.Controller", "Arcade", "am_mp_arcade", "am_mp_arcade.c", Area::Property, Detail::Joaat("am_mp_arcade")},
        Definition{"Property.Casino.Controller", "Casino", "am_mp_casino", "am_mp_casino.c", Area::Property, Detail::Joaat("am_mp_casino")},
        Definition{"Property.CasinoNightclub.Controller", "Casino Nightclub", "am_mp_casino_nightclub", "am_mp_casino_nightclub.c", Area::Property, Detail::Joaat("am_mp_casino_nightclub")},
        Definition{"DLC.CarWash.Controller", "Car Wash", "am_mp_carwash_launch", "am_mp_carwash_launch.c", Area::Dlc, Detail::Joaat("am_mp_carwash_launch")},
        Definition{"DLC.LuxuryShowroom.Controller", "Luxury Showroom", "am_luxury_showroom", "am_luxury_showroom.c", Area::Dlc, Detail::Joaat("am_luxury_showroom")},
        Definition{"DLC.Mansion.Limo", "Mansion Limo", "am_mansion_limo", "am_mansion_limo.c", Area::Dlc, Detail::Joaat("am_mansion_limo")},
        Definition{"DLC.Mansion.LuxuryCar", "Mansion Luxury Car", "am_mansion_luxury_car", "am_mansion_luxury_car.c", Area::Dlc, Detail::Joaat("am_mansion_luxury_car")},
    };

    struct ScriptState final
    {
        bool threadFound{};
        bool running{};
        bool stackReady{};
        bool programLoaded{};
        Types::ScriptThreadState threadState{Types::ScriptThreadState::Idle};
        std::uint32_t programCounter{};
        std::uint32_t stackSize{};
        std::uint32_t codeSize{};
        std::uint32_t localCount{};
        std::uint32_t globalCount{};
        std::uint32_t nativeCount{};
    };

    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool scriptRuntimeReady{};
        std::array<ScriptState, Definitions.size()> scripts{};
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

            SetPending("Refreshing Enhanced decompile script catalog");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                auto& scripts = Script::ScriptRuntime::Get();
                state.scriptRuntimeReady = scripts.IsReady();
                if (!state.scriptRuntimeReady)
                    return Finish(false, std::move(state), "Shared Enhanced script runtime is unavailable");

                for (std::size_t index = 0; index < Definitions.size(); ++index)
                {
                    const auto& definition = Definitions[index];
                    auto& output = state.scripts[index];

                    if (const auto* thread = scripts.FindThread(definition.hash))
                    {
                        output.threadFound = true;
                        output.stackReady = thread->stack != nullptr;
                        output.threadState = thread->context.state;
                        output.running = output.stackReady && thread->context.state == Types::ScriptThreadState::Running;
                        output.programCounter = thread->context.programCounter;
                        output.stackSize = thread->context.stackSize;
                    }

                    if (const auto* program = scripts.FindProgram(definition.hash))
                    {
                        output.programLoaded = true;
                        output.codeSize = program->codeSize;
                        output.localCount = program->localCount;
                        output.globalCount = program->globalCount;
                        output.nativeCount = program->nativeCount;
                    }
                }

                TUTONES_LOG_DEBUG("enhanced.script_center", "Enhanced decompile script catalog refreshed");
                Finish(true, std::move(state), "Enhanced decompile script catalog refreshed");
            }))
            {
                return true;
            }

            Finish(false, {}, "Game-thread queue unavailable");
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
            snapshot.scriptRuntimeReady = m_State.scriptRuntimeReady;
            snapshot.scripts = m_State.scripts;
            snapshot.message = m_State.message;
            return snapshot;
        }

    private:
        Runtime() = default;

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

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        Snapshot m_State{};
    };
}
