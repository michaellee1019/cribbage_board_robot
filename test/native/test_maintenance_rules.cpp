#include <MaintenanceRules.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <string>

using scorebot::MaintenanceAction;

std::string frame(MaintenanceAction action, uint32_t elapsedMs) {
    const auto value = scorebot::maintenanceDisplayFrame(action, elapsedMs);
    return value.data();
}

int main() {
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kPlayerResetChordMask, false) ==
           MaintenanceAction::Reset);
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kLeaderboardResetChordMask, true) ==
           MaintenanceAction::Reset);
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kPlayerOtaChordMask, false) ==
           MaintenanceAction::Ota);
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kLeaderboardOtaChordMask, true) ==
           MaintenanceAction::Ota);
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kLeaderboardPrintChordMask, true) ==
           MaintenanceAction::Print);
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kPlayerNegativeOneButtonMask, false) ==
           MaintenanceAction::None);
    const uint16_t staggeredReset = static_cast<uint16_t>(
        scorebot::kPlayerNegativeOneButtonMask |
        scorebot::kPlayerPlusOneButtonMask);
    assert(scorebot::maintenanceActionForButtons(staggeredReset, false) ==
           MaintenanceAction::Reset);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Reset, false) ==
           scorebot::kPlayerResetChordMask);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Reset, true) ==
           scorebot::kLeaderboardResetChordMask);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Ota, false) ==
           scorebot::kPlayerOtaChordMask);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Ota, true) ==
           scorebot::kLeaderboardOtaChordMask);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Print, true) ==
           scorebot::kLeaderboardPrintChordMask);
    assert(scorebot::kLeaderboardPrintChordMask == 0x0f);
    assert(scorebot::maintenanceChordMask(MaintenanceAction::Print, false) ==
           0);
    assert(scorebot::printChordMask(false) == 0);
    assert((scorebot::kPlayerNegativeOneButtonMask &
            scorebot::maintenanceChordMask(MaintenanceAction::Reset, false)) !=
           scorebot::maintenanceChordMask(MaintenanceAction::Reset, false));
    assert(scorebot::maintenanceActionForButtons(
               scorebot::kPlayerResetChordMask |
                   scorebot::kPlayerOtaChordMask,
               false) ==
           MaintenanceAction::None);

    // Adding unrelated buttons does not change existing player-board chord
    // behavior, and the leaderboard-only PRINT gesture never appears there.
    assert(scorebot::maintenanceActionForButtons(0x0f, false) ==
           MaintenanceAction::Reset);
    assert(scorebot::maintenanceActionTransition(
               MaintenanceAction::Reset, 0x0f, false) ==
           MaintenanceAction::Reset);

    // RESET or OTA can be recognized while the four leaderboard buttons are
    // assembled in any order, but the fourth press always promotes to PRINT.
    std::array<uint16_t, 4> leaderboardButtons = {
        scorebot::kLeaderboardAddButtonMask,
        scorebot::kLeaderboardNegativeOneButtonMask,
        scorebot::kLeaderboardPlusOneButtonMask,
        scorebot::kLeaderboardOkButtonMask,
    };
    std::sort(leaderboardButtons.begin(), leaderboardButtons.end());
    size_t pressOrderCount = 0;
    do {
        ++pressOrderCount;
        MaintenanceAction active = MaintenanceAction::None;
        uint16_t pressed = 0;
        size_t pressedCount = 0;
        for (const uint16_t button : leaderboardButtons) {
            ++pressedCount;
            pressed = static_cast<uint16_t>(pressed | button);
            active = scorebot::maintenanceActionTransition(
                active, pressed, true);
            if (pressedCount < leaderboardButtons.size()) {
                assert(active != MaintenanceAction::Print);
            }
        }
        assert(pressed == scorebot::kLeaderboardPrintChordMask);
        assert(active == MaintenanceAction::Print);
    } while (std::next_permutation(
        leaderboardButtons.begin(), leaderboardButtons.end()));
    assert(pressOrderCount == 24);

    assert(scorebot::maintenanceActionTransition(
               MaintenanceAction::Reset,
               scorebot::kLeaderboardPrintChordMask, true) ==
           MaintenanceAction::Print);
    assert(scorebot::maintenanceActionTransition(
               MaintenanceAction::Ota,
               scorebot::kLeaderboardPrintChordMask, true) ==
           MaintenanceAction::Print);
    assert(scorebot::maintenanceActionTransition(
               MaintenanceAction::Print,
               scorebot::kLeaderboardResetChordMask, true) ==
           MaintenanceAction::Print);

    assert(scorebot::maintenanceChordHeld(
        MaintenanceAction::Print,
        scorebot::kLeaderboardPrintChordMask, true));
    assert(!scorebot::maintenanceChordHeld(
        MaintenanceAction::Print,
        static_cast<uint16_t>(scorebot::kLeaderboardPrintChordMask &
                              ~scorebot::kLeaderboardOkButtonMask),
        true));
    assert(!scorebot::maintenanceChordHeld(
        MaintenanceAction::Print,
        scorebot::kLeaderboardPrintChordMask, false));
    assert(!scorebot::maintenanceChordHeld(
        MaintenanceAction::None, 0xffff, true));

    assert(!scorebot::maintenanceHoldComplete(4999));
    assert(scorebot::maintenanceHoldComplete(5000));
    assert(scorebot::maintenanceFlashIntervalMs(0) == 600);
    assert(scorebot::maintenanceFlashIntervalMs(2500) == 340);
    assert(scorebot::maintenanceFlashIntervalMs(5000) == 80);
    assert(scorebot::maintenanceFlashIntervalMs(6000) == 80);

    assert(frame(MaintenanceAction::Reset, 0) == "RESE");
    assert(frame(MaintenanceAction::Reset, 300) == "ESET");
    assert(frame(MaintenanceAction::Reset, 600) == "SET ");
    assert(frame(MaintenanceAction::Ota, 0) == "OTA ");
    assert(frame(MaintenanceAction::Print, 0) == "PRIN");
    assert(frame(MaintenanceAction::Print, 300) == "RINT");
    assert(frame(MaintenanceAction::Print, 600) == "INT ");
    assert(frame(MaintenanceAction::None, 0) == "    ");
    assert(scorebot::kPrintMaintenanceColor == 0x00ffff);
    std::cout << "Maintenance-rule tests passed\n";
}
