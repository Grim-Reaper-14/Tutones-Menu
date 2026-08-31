#pragma once

#include "NetworkRuntime.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Tutones::Game::NetworkFeatures
{
    enum class PlayerRiskLevel : std::uint8_t
    {
        Clear,
        Notice,
        Elevated,
        High,
    };

    struct PlayerRiskAssessment final
    {
        int score{};
        PlayerRiskLevel level{PlayerRiskLevel::Clear};
        bool watchlisted{};
        bool trusted{};
        bool avoidInteractions{};
        bool proximityAlert{};
        std::vector<std::string> reasons{};
    };

    struct PlayerDefenseDiagnostic final
    {
        std::uint64_t rosterGeneration{};
        int playerId{-1};
        PlayerRiskLevel level{PlayerRiskLevel::Clear};
        std::string playerName{};
        std::string message{};
    };

    struct NetworkPlayerDefenseSnapshot final
    {
        bool proximityWarningsEnabled{true};
        bool restrictWatchlistedActions{true};
        bool autoWatchHighRisk{};
        float proximityRadius{150.0f};
        int watchlistedCount{};
        int trustedCount{};
        int noticeCount{};
        int elevatedCount{};
        int highCount{};
        std::array<PlayerRiskAssessment, 32> assessments{};
        std::vector<PlayerDefenseDiagnostic> diagnostics{};
    };

    class NetworkPlayerDefenseRuntime final
    {
    public:
        static NetworkPlayerDefenseRuntime& Get() noexcept
        {
            static NetworkPlayerDefenseRuntime instance;
            return instance;
        }

        void ObserveRoster(const NetworkPlayerRosterSnapshot& roster)
        {
            std::scoped_lock lock(m_Mutex);
            if (roster.generation == m_LastRosterGeneration)
                return;

            m_LastRosterGeneration = roster.generation;
            m_WatchlistedCount = 0;
            m_TrustedCount = 0;
            m_NoticeCount = 0;
            m_ElevatedCount = 0;
            m_HighCount = 0;

            for (std::size_t index = 0; index < roster.players.size(); ++index)
            {
                const auto& player = roster.players[index];
                auto& identity = m_Identity[index];
                auto& previousLevel = m_PreviousLevel[index];
                auto& previousProximity = m_PreviousProximity[index];

                if (!player.active)
                {
                    m_Assessments[index] = {};
                    previousLevel = PlayerRiskLevel::Clear;
                    previousProximity = false;
                    continue;
                }

                if (!identity.name.empty() && identity.name != player.name)
                {
                    identity = {};
                    previousLevel = PlayerRiskLevel::Clear;
                    previousProximity = false;
                }
                identity.name = player.name;

                auto assessment = AssessPlayer(player, identity);
                if (m_AutoWatchHighRisk
                    && assessment.level == PlayerRiskLevel::High
                    && !identity.trusted
                    && !identity.watchlisted)
                {
                    identity.watchlisted = true;
                    assessment.watchlisted = true;
                    AppendDiagnostic(
                        roster.generation,
                        player,
                        assessment.level,
                        "Automatically added to watchlist after a high-risk diagnostic state");
                }

                assessment.proximityAlert = m_ProximityWarningsEnabled
                    && identity.watchlisted
                    && !player.local
                    && player.distanceReadable
                    && player.distance >= 0.0f
                    && player.distance <= m_ProximityRadius;

                if (assessment.level != previousLevel)
                {
                    if (assessment.level == PlayerRiskLevel::Clear)
                    {
                        if (previousLevel != PlayerRiskLevel::Clear)
                        {
                            AppendDiagnostic(
                                roster.generation,
                                player,
                                assessment.level,
                                "Risk diagnostics returned to clear");
                        }
                    }
                    else
                    {
                        AppendDiagnostic(
                            roster.generation,
                            player,
                            assessment.level,
                            std::string("Risk state changed to ") + RiskLevelName(assessment.level));
                    }
                    previousLevel = assessment.level;
                }

                if (assessment.proximityAlert && !previousProximity)
                {
                    AppendDiagnostic(
                        roster.generation,
                        player,
                        assessment.level,
                        "Watchlisted player entered the configured proximity radius");
                }
                else if (!assessment.proximityAlert && previousProximity)
                {
                    AppendDiagnostic(
                        roster.generation,
                        player,
                        assessment.level,
                        "Watchlisted player left the configured proximity radius");
                }
                previousProximity = assessment.proximityAlert;

                if (identity.watchlisted)
                    ++m_WatchlistedCount;
                if (identity.trusted)
                    ++m_TrustedCount;

                switch (assessment.level)
                {
                case PlayerRiskLevel::Notice:
                    ++m_NoticeCount;
                    break;
                case PlayerRiskLevel::Elevated:
                    ++m_ElevatedCount;
                    break;
                case PlayerRiskLevel::High:
                    ++m_HighCount;
                    break;
                case PlayerRiskLevel::Clear:
                default:
                    break;
                }

                m_Assessments[index] = std::move(assessment);
            }
        }

        [[nodiscard]] NetworkPlayerDefenseSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            NetworkPlayerDefenseSnapshot out;
            out.proximityWarningsEnabled = m_ProximityWarningsEnabled;
            out.restrictWatchlistedActions = m_RestrictWatchlistedActions;
            out.autoWatchHighRisk = m_AutoWatchHighRisk;
            out.proximityRadius = m_ProximityRadius;
            out.watchlistedCount = m_WatchlistedCount;
            out.trustedCount = m_TrustedCount;
            out.noticeCount = m_NoticeCount;
            out.elevatedCount = m_ElevatedCount;
            out.highCount = m_HighCount;
            out.assessments = m_Assessments;
            out.diagnostics = m_Diagnostics;
            return out;
        }

        void SetWatchlisted(int playerId, const std::string& playerName, bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            auto* identity = ResolveIdentity(playerId, playerName);
            if (!identity)
                return;

            identity->watchlisted = enabled;
            if (enabled)
                identity->trusted = false;
            RefreshManualFlags(playerId, *identity);
        }

        void SetTrusted(int playerId, const std::string& playerName, bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            auto* identity = ResolveIdentity(playerId, playerName);
            if (!identity)
                return;

            identity->trusted = enabled;
            if (enabled)
            {
                identity->watchlisted = false;
                identity->avoidInteractions = false;
            }
            RefreshManualFlags(playerId, *identity);
        }

        void SetAvoidInteractions(int playerId, const std::string& playerName, bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            auto* identity = ResolveIdentity(playerId, playerName);
            if (!identity)
                return;

            identity->avoidInteractions = enabled;
            RefreshManualFlags(playerId, *identity);
        }

        void SetProximityWarningsEnabled(bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            m_ProximityWarningsEnabled = enabled;
            if (!enabled)
            {
                m_PreviousProximity.fill(false);
                for (auto& assessment : m_Assessments)
                    assessment.proximityAlert = false;
            }
        }

        void SetRestrictWatchlistedActions(bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            m_RestrictWatchlistedActions = enabled;
        }

        void SetAutoWatchHighRisk(bool enabled)
        {
            std::scoped_lock lock(m_Mutex);
            m_AutoWatchHighRisk = enabled;
        }

        void SetProximityRadius(float radius)
        {
            std::scoped_lock lock(m_Mutex);
            m_ProximityRadius = std::clamp(radius, 25.0f, 1000.0f);
        }

        void ClearDiagnostics()
        {
            std::scoped_lock lock(m_Mutex);
            m_Diagnostics.clear();
        }

        [[nodiscard]] static const char* RiskLevelName(PlayerRiskLevel level) noexcept
        {
            switch (level)
            {
            case PlayerRiskLevel::Notice:
                return "NOTICE";
            case PlayerRiskLevel::Elevated:
                return "ELEVATED";
            case PlayerRiskLevel::High:
                return "HIGH";
            case PlayerRiskLevel::Clear:
            default:
                return "CLEAR";
            }
        }

    private:
        struct PlayerIdentityState final
        {
            std::string name{};
            bool watchlisted{};
            bool trusted{};
            bool avoidInteractions{};
        };

        static constexpr std::size_t MaxDiagnostics = 64;

        NetworkPlayerDefenseRuntime() = default;
        ~NetworkPlayerDefenseRuntime() = default;
        NetworkPlayerDefenseRuntime(const NetworkPlayerDefenseRuntime&) = delete;
        NetworkPlayerDefenseRuntime& operator=(const NetworkPlayerDefenseRuntime&) = delete;

        [[nodiscard]] PlayerRiskAssessment AssessPlayer(
            const NetworkPlayerSnapshot& player,
            const PlayerIdentityState& identity) const
        {
            PlayerRiskAssessment assessment;
            assessment.watchlisted = identity.watchlisted;
            assessment.trusted = identity.trusted;
            assessment.avoidInteractions = identity.avoidInteractions;

            if (player.local)
                return assessment;

            if (player.managerSlotPresent && !player.managerIndexMatches)
            {
                assessment.score += 55;
                assessment.reasons.emplace_back("Player-manager slot/index mismatch");
            }

            if (player.managerSlotPresent && player.managerLocalFlag)
            {
                assessment.score += 70;
                assessment.reasons.emplace_back("Remote player slot reports the local-player flag");
            }

            if (player.statsReadable && (player.rank < 0 || player.rank > 8000))
            {
                assessment.score += 30;
                assessment.reasons.emplace_back("Rank is outside the normal GTA Online range");
            }

            if (player.healthReadable && player.maxHealth > 0 && player.health > player.maxHealth + 50)
            {
                assessment.score += 30;
                assessment.reasons.emplace_back("Health is materially above the reported maximum");
            }

            if (player.armourReadable && player.armour > 100)
            {
                assessment.score += 25;
                assessment.reasons.emplace_back("Armor is above the normal 100-point cap");
            }

            if (player.statsReadable && player.kdRatio > 50.0f)
            {
                assessment.score += 15;
                assessment.reasons.emplace_back("Unusually high K/D ratio");
            }

            if (player.statsReadable && player.weaponAccuracy > 100.0f)
            {
                assessment.score += 20;
                assessment.reasons.emplace_back("Weapon accuracy is above the expected percentage range");
            }

            if (player.statsReadable && player.communicationRestrictions != 0)
            {
                assessment.score += 10;
                assessment.reasons.emplace_back("Communication restrictions are present");
            }

            if (player.packetLossReadable && player.averagePacketLoss > 0.20f)
            {
                assessment.score += 10;
                assessment.reasons.emplace_back("High packet loss / unstable peer connection");
            }

            if (player.resendReadable && player.highestReliableResendCount > 10)
            {
                assessment.score += 10;
                assessment.reasons.emplace_back("High reliable resend count / unstable connection");
            }

            if (assessment.score >= 70)
                assessment.level = PlayerRiskLevel::High;
            else if (assessment.score >= 40)
                assessment.level = PlayerRiskLevel::Elevated;
            else if (assessment.score >= 20)
                assessment.level = PlayerRiskLevel::Notice;
            else
                assessment.level = PlayerRiskLevel::Clear;

            return assessment;
        }

        [[nodiscard]] PlayerIdentityState* ResolveIdentity(int playerId, const std::string& playerName)
        {
            if (playerId < 0 || playerId >= static_cast<int>(m_Identity.size()) || playerName.empty())
                return nullptr;

            auto& identity = m_Identity[static_cast<std::size_t>(playerId)];
            if (!identity.name.empty() && identity.name != playerName)
                identity = {};
            identity.name = playerName;
            return &identity;
        }

        void RefreshManualFlags(int playerId, const PlayerIdentityState& identity)
        {
            if (playerId < 0 || playerId >= static_cast<int>(m_Assessments.size()))
                return;

            auto& assessment = m_Assessments[static_cast<std::size_t>(playerId)];
            assessment.watchlisted = identity.watchlisted;
            assessment.trusted = identity.trusted;
            assessment.avoidInteractions = identity.avoidInteractions;
        }

        void AppendDiagnostic(
            std::uint64_t generation,
            const NetworkPlayerSnapshot& player,
            PlayerRiskLevel level,
            std::string message)
        {
            if (m_Diagnostics.size() >= MaxDiagnostics)
                m_Diagnostics.erase(m_Diagnostics.begin());

            PlayerDefenseDiagnostic event;
            event.rosterGeneration = generation;
            event.playerId = player.id;
            event.level = level;
            event.playerName = player.name;
            event.message = std::move(message);
            m_Diagnostics.push_back(std::move(event));
        }

        mutable std::mutex m_Mutex;
        std::array<PlayerIdentityState, 32> m_Identity{};
        std::array<PlayerRiskAssessment, 32> m_Assessments{};
        std::array<PlayerRiskLevel, 32> m_PreviousLevel{};
        std::array<bool, 32> m_PreviousProximity{};
        std::vector<PlayerDefenseDiagnostic> m_Diagnostics{};
        std::uint64_t m_LastRosterGeneration{};
        bool m_ProximityWarningsEnabled{true};
        bool m_RestrictWatchlistedActions{true};
        bool m_AutoWatchHighRisk{};
        float m_ProximityRadius{150.0f};
        int m_WatchlistedCount{};
        int m_TrustedCount{};
        int m_NoticeCount{};
        int m_ElevatedCount{};
        int m_HighCount{};
    };
}
