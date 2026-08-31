#pragma once

#include <BoardRole.hpp>

namespace scorebot {

constexpr bool mayAcceptSnapshot(BoardRole localRole, BoardRole transportSenderRole) {
    const bool localIsPlayer = localRole == BoardRole::Player_Red ||
                               localRole == BoardRole::Player_Blue ||
                               localRole == BoardRole::Player_Green ||
                               localRole == BoardRole::Player_White;
    return localIsPlayer && transportSenderRole == BoardRole::Leader;
}

}  // namespace scorebot
