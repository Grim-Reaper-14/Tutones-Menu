#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../game/GamePointers.hpp"
#include "../../game/script/ScriptGlobal.hpp"
#include "../../runtime/GameRuntime.hpp"
#include "../network/RequestServicesRuntime.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

namespace Tutones::Game::ServicesDiagnostics
{
    struct Snapshot final
    {
        bool pending{};
        bool haveResult{};
        bool lastSucceeded{};
        bool sessionStarted{};
        bool globalsReady{};
        bool contactRequestBusy{};
        int groupedRequestFlags{};
        std::array<int, NetworkFeatures::RequestServiceCatalog.size()> requestValues{};
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

            SetPending("Reading Enhanced contact-request state");
            if (Tutones::Runtime::GameRuntime::Get().Enqueue([this] {
                Snapshot state;
                if (bool* sessionStarted = GamePointers::Get().IsSessionStarted())
                    state.sessionStarted = *sessionStarted;

                if (!state.sessionStarted)
                    return Finish(false, std::move(state), "Join GTA Online before reading Services state");

                auto* pages = GamePointers::Get().ScriptGlobals();
                state.globalsReady = pages != nullptr;
                if (!state.globalsReady)
                    return Finish(false, std::move(state), "Enhanced script globals are unavailable");

                const Script::ScriptGlobal root(NetworkFeatures::RequestServicesGlobal);
                for (std::size_t index = 0; index < NetworkFeatures::RequestServiceCatalog.size(); ++index)
                {
                    const auto& definition = NetworkFeatures::RequestServiceCatalog[index];
                    const int* value = root.At(definition.offset).As<int>(pages);
                    if (!value)
                        return Finish(false, std::move(state), "Unable to read a contact-request global");
                    state.requestValues[index] = *value;
                }

                const int* groupedFlags = root.At(594).As<int>(pages);
                if (!groupedFlags)
                    return Finish(false, std::move(state), "Unable to read grouped contact-request flags");
                state.groupedRequestFlags = *groupedFlags;

                // am_contact_requests.c groups these request latches together and sets
                // its local busy bit when any are active. Mirror only that proven read
                // condition here; do not infer ownership/cooldown semantics from it.
                const auto active = [&state](NetworkFeatures::RequestService service) noexcept
                {
                    const auto index = static_cast<std::size_t>(service);
                    return index < state.requestValues.size() && state.requestValues[index] != 0;
                };
                state.contactRequestBusy =
                    active(NetworkFeatures::RequestService::Kosatka) ||
                    active(NetworkFeatures::RequestService::Dinghy) ||
                    active(NetworkFeatures::RequestService::Avenger) ||
                    active(NetworkFeatures::RequestService::Terrorbyte) ||
                    active(NetworkFeatures::RequestService::MOC) ||
                    active(NetworkFeatures::RequestService::AcidLabBike) ||
                    active(NetworkFeatures::RequestService::AcidLab) ||
                    (state.groupedRequestFlags & (1 << 1)) != 0;

                TUTONES_LOG_DEBUG("services.diagnostics", "Enhanced contact-request state refreshed");
                Finish(true, std::move(state), "Enhanced contact-request state refreshed");
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
            snapshot.globalsReady = m_State.globalsReady;
            snapshot.contactRequestBusy = m_State.contactRequestBusy;
            snapshot.groupedRequestFlags = m_State.groupedRequestFlags;
            snapshot.requestValues = m_State.requestValues;
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
