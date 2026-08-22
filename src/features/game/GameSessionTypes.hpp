#pragma once

#include <array>
#include <cstddef>

namespace Tutones::Game::SessionFeatures
{
    enum class GameServiceAction : unsigned char
    {
        None,
        AirstrikeAhead,
        AmmoDrop,
        MinigunDrop,
    };

    enum class JoinType : int
    {
        JoinPublic = 0,
        NewPublic = 1,
        ClosedCrew = 2,
        Crew = 3,
        ClosedFriends = 6,
        FindFriend = 9,
        Solo = 10,
        InviteOnly = 11,
        JoinCrew = 12,
        Sctv = 13,
        LeaveOnline = -1,
    };

    struct JoinTypeEntry final
    {
        JoinType value{};
        const char* label{};
    };

    inline constexpr std::array<JoinTypeEntry, 10> OnlineJoinTypes{{
        {JoinType::JoinPublic, "Public"},
        {JoinType::NewPublic, "New / Solo Public"},
        {JoinType::ClosedCrew, "Closed Crew"},
        {JoinType::Crew, "Crew"},
        {JoinType::ClosedFriends, "Closed Friends"},
        {JoinType::FindFriend, "Find Friend"},
        {JoinType::Solo, "Solo"},
        {JoinType::InviteOnly, "Invite Only"},
        {JoinType::JoinCrew, "Join Crew"},
        {JoinType::Sctv, "SCTV"},
    }};
}
