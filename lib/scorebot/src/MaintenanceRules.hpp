#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace scorebot {

enum class MaintenanceAction : uint8_t { None, Reset, Ota, Print };

inline constexpr uint32_t kMaintenanceHoldMs = 5000;
inline constexpr uint32_t kMaintenanceScrollStepMs = 300;
inline constexpr uint32_t kMaintenanceSlowFlashMs = 600;
inline constexpr uint32_t kMaintenanceFastFlashMs = 80;
inline constexpr uint32_t kPrintMaintenanceColor = 0x00ffff;

inline constexpr uint16_t kPlayerAddButtonMask = 1u << 0;
inline constexpr uint16_t kPlayerNegativeOneButtonMask = 1u << 1;
inline constexpr uint16_t kPlayerPlusFiveButtonMask = 1u << 2;
inline constexpr uint16_t kPlayerPlusOneButtonMask = 1u << 3;
inline constexpr uint16_t kPlayerOkButtonMask = 1u << 4;
inline constexpr uint16_t kLeaderboardAddButtonMask = 1u << 1;
inline constexpr uint16_t kLeaderboardNegativeOneButtonMask = 1u << 3;
inline constexpr uint16_t kLeaderboardPlusOneButtonMask = 1u << 0;
inline constexpr uint16_t kLeaderboardOkButtonMask = 1u << 2;
inline constexpr uint16_t kPlayerResetChordMask =
    kPlayerNegativeOneButtonMask | kPlayerPlusOneButtonMask;
inline constexpr uint16_t kLeaderboardResetChordMask =
    kLeaderboardNegativeOneButtonMask | kLeaderboardPlusOneButtonMask;
inline constexpr uint16_t kPlayerOtaChordMask =
    kPlayerAddButtonMask | kPlayerOkButtonMask;
inline constexpr uint16_t kLeaderboardOtaChordMask =
    kLeaderboardAddButtonMask | kLeaderboardOkButtonMask;
inline constexpr uint16_t kLeaderboardPrintChordMask =
    kLeaderboardAddButtonMask | kLeaderboardNegativeOneButtonMask |
    kLeaderboardPlusOneButtonMask | kLeaderboardOkButtonMask;

constexpr uint16_t resetChordMask(bool leaderboard) {
    return leaderboard ? kLeaderboardResetChordMask : kPlayerResetChordMask;
}

constexpr uint16_t otaChordMask(bool leaderboard) {
    return leaderboard ? kLeaderboardOtaChordMask : kPlayerOtaChordMask;
}

constexpr uint16_t printChordMask(bool leaderboard) {
    return leaderboard ? kLeaderboardPrintChordMask : 0;
}

constexpr MaintenanceAction maintenanceActionForButtons(
    uint16_t pressedMask, bool leaderboard) {
    const uint16_t printMask = printChordMask(leaderboard);
    if (printMask != 0 && (pressedMask & printMask) == printMask) {
        return MaintenanceAction::Print;
    }
    const uint16_t resetMask = resetChordMask(leaderboard);
    const bool reset = (pressedMask & resetMask) == resetMask;
    const uint16_t otaMask = otaChordMask(leaderboard);
    const bool ota = (pressedMask & otaMask) == otaMask;
    if (reset == ota) {
        return MaintenanceAction::None;
    }
    return reset ? MaintenanceAction::Reset : MaintenanceAction::Ota;
}

// Preserve an in-progress maintenance action unless the leaderboard's
// higher-priority all-button chord promotes it to PRINT. Callers can compare
// the returned action with `active` to restart the hold interval on promotion.
constexpr MaintenanceAction maintenanceActionTransition(
    MaintenanceAction active, uint16_t pressedMask, bool leaderboard) {
    const MaintenanceAction detected =
        maintenanceActionForButtons(pressedMask, leaderboard);
    if (detected == MaintenanceAction::Print) {
        return MaintenanceAction::Print;
    }
    return active == MaintenanceAction::None ? detected : active;
}

constexpr uint16_t maintenanceChordMask(
    MaintenanceAction action, bool leaderboard) {
    return action == MaintenanceAction::Reset
               ? resetChordMask(leaderboard)
               : action == MaintenanceAction::Ota
                     ? otaChordMask(leaderboard)
                     : action == MaintenanceAction::Print
                           ? printChordMask(leaderboard)
                           : 0;
}

constexpr bool maintenanceChordHeld(
    MaintenanceAction action, uint16_t pressedMask, bool leaderboard) {
    const uint16_t required = maintenanceChordMask(action, leaderboard);
    return required != 0 && (pressedMask & required) == required;
}

constexpr bool maintenanceHoldComplete(uint32_t elapsedMs) {
    return elapsedMs >= kMaintenanceHoldMs;
}

constexpr uint32_t maintenanceFlashIntervalMs(uint32_t elapsedMs) {
    const uint32_t clamped = elapsedMs > kMaintenanceHoldMs
                                 ? kMaintenanceHoldMs
                                 : elapsedMs;
    return kMaintenanceSlowFlashMs -
           clamped * (kMaintenanceSlowFlashMs - kMaintenanceFastFlashMs) /
               kMaintenanceHoldMs;
}

constexpr bool maintenanceLightOn(uint32_t elapsedMs) {
    const uint32_t interval = maintenanceFlashIntervalMs(elapsedMs);
    return ((elapsedMs / interval) & 1u) == 0;
}

constexpr std::array<char, 5> maintenanceDisplayFrame(
    MaintenanceAction action, uint32_t elapsedMs) {
    if (action == MaintenanceAction::Ota) {
        return {'O', 'T', 'A', ' ', '\0'};
    }
    if (action != MaintenanceAction::Reset &&
        action != MaintenanceAction::Print) {
        return {' ', ' ', ' ', ' ', '\0'};
    }

    constexpr std::array<char, 8> resetCycle = {
        'R', 'E', 'S', 'E', 'T', ' ', ' ', ' ',
    };
    constexpr std::array<char, 8> printCycle = {
        'P', 'R', 'I', 'N', 'T', ' ', ' ', ' ',
    };
    const auto& cycle = action == MaintenanceAction::Print
                            ? printCycle
                            : resetCycle;
    const size_t offset =
        static_cast<size_t>((elapsedMs / kMaintenanceScrollStepMs) % cycle.size());
    return {
        cycle[(offset + 0) % cycle.size()],
        cycle[(offset + 1) % cycle.size()],
        cycle[(offset + 2) % cycle.size()],
        cycle[(offset + 3) % cycle.size()],
        '\0',
    };
}

}  // namespace scorebot
