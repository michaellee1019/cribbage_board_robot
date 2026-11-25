#include <ButtonGestureRules.hpp>
#include <ButtonInputRules.hpp>
#include <MaintenanceRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::ButtonAction;

void testScoreButtonGuard() {
    using namespace scorebot;
    constexpr uint16_t score = kPlayerPlusFiveButtonMask;
    constexpr uint16_t add = kPlayerAddButtonMask;
    constexpr uint16_t ok = kPlayerOkButtonMask;
    constexpr uint16_t confirm = add | ok;

    const auto fastScoreTap = buttonEdges(0, 0, score, 0, false);
    assert(fastScoreTap.pressed == score && fastScoreTap.completed == score);
    assert(!rejectButtonGesture(fastScoreTap, score, confirm, false));

    StagedScoreButtons<int> staged;
    assert(staged.stage(20000, 5, 3));
    assert(staged.stage(50000, 5, 3));
    int delta = 0;
    assert(!staged.pop(19999, delta));  // Commit IRQ predated release dispatch.
    assert(!staged.pop(139999, delta));
    assert(rejectButtonGesture(buttonEdges(0, ok, ok, 0, false),
                               score, confirm, !staged.empty()));
    // The first score matured independently; the newer staged tap can be
    // discarded without rolling back or committing the earlier edit.
    assert(staged.pop(140000, delta) && delta == 5);
    assert(!staged.pop(140000, delta));
    assert(rejectButtonGesture(buttonEdges(0, add, add, 0, false),
                               score, confirm, !staged.empty()));
    staged.clear();
    assert(staged.empty());
    assert(!rejectButtonGesture(buttonEdges(0, ok, ok, 0, false),
                                score, confirm, !staged.empty()));

    assert(staged.stage(UINT32_MAX - 60000, 1, 2));
    assert(!staged.pop(59998, delta));
    assert(staged.pop(59999, delta) && delta == 1);
}

void testScoreReleaseMustSettle() {
    using namespace scorebot;
    constexpr uint8_t plusFive = 3;
    constexpr uint8_t plusOne = 2;
    StagedScoreButtons<int> staged;
    int delta = 0;

    // An early apparent release during switch bounce must not count as a
    // second tap when the user's real release arrives much later.
    assert(staged.stage(20000, 5, plusFive));
    staged.discardBouncedReleases(35000, 1u << plusFive);
    assert(staged.empty());
    assert(staged.stage(120000, 5, plusFive));
    assert(staged.pop(240000, delta) && delta == 5);
    assert(staged.empty());

    // Same-key bounce leaves the other key and a distinct earlier tap in FIFO order.
    assert(staged.stage(250000, 5, plusFive));
    staged.discardBouncedReleases(285000, 1u << plusFive);
    assert(staged.stage(295000, 1, plusOne));
    assert(staged.stage(300000, 5, plusFive));
    staged.discardBouncedReleases(310000, 1u << plusFive);
    assert(staged.stage(311000, 5, plusFive));
    assert(staged.pop(431000, delta) && delta == 5);
    assert(staged.pop(431000, delta) && delta == 1);
    assert(staged.pop(431000, delta) && delta == 5);
    assert(staged.empty());
}

void testOverlappingButtons() {
    using namespace scorebot;
    constexpr uint16_t score = kLeaderboardPlusOneButtonMask;
    constexpr uint16_t add = kLeaderboardAddButtonMask;
    constexpr uint16_t ok = kLeaderboardOkButtonMask;
    constexpr uint16_t confirm = add | ok;

    assert(rejectButtonGesture(buttonEdges(score, score | ok, score | ok, 0, false),
                               score, confirm, false));
    assert(rejectButtonGesture(buttonEdges(0, 0, score | add, 0, false),
                               score, confirm, false));
    assert(rejectButtonGesture(buttonEdges(0, 0, 0, score | ok, true),
                               score, confirm, false));
    assert(rejectButtonGesture(buttonEdges(score, add, 0, 0, false),
                               score, confirm, false));
    assert(rejectButtonGesture(buttonEdges(0, kLeaderboardNegativeOneButtonMask | add, 0, 0, false),
                               score, confirm, false));
    // GameState checks live maintenance chords before rejecting overlaps.
    assert(maintenanceActionForButtons(confirm, true) == MaintenanceAction::Ota);
    assert(maintenanceActionForButtons(kLeaderboardResetChordMask, true) == MaintenanceAction::Reset);
    assert(maintenanceActionForButtons(kLeaderboardPrintChordMask, true) == MaintenanceAction::Print);
    assert(maintenanceActionForButtons(score | ok, true) == MaintenanceAction::None);

    ButtonReleaseDebounce debounce;
    assert(debounce.accept(0, 100));
    assert(!debounce.accept(0, 103));
    assert(debounce.accept(1, 103));
    assert(debounce.accept(0, 115));
}

int main() {
    assert(scorebot::buttonCapturedPressed(0, 0b11110));
    assert(scorebot::buttonCapturedPressed(4, 0b01111));
    assert(!scorebot::buttonCapturedPressed(0, 0b11111));
    assert(!scorebot::buttonCapturedPressed(4, 0b11111));
    assert(!scorebot::buttonCapturedPressed(16, 0));
    assert(scorebot::buttonActionForPin(false, 0) == ButtonAction::Add);
    assert(scorebot::buttonActionForPin(false, 1) == ButtonAction::NegativeOne);
    assert(scorebot::buttonActionForPin(false, 2) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 3) == ButtonAction::PlusFive);
    assert(scorebot::buttonActionForPin(false, 4) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 0) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(true, 1) == ButtonAction::Add);
    assert(scorebot::buttonActionForPin(true, 2) == ButtonAction::Ok);
    assert(scorebot::buttonActionForPin(true, 3) == ButtonAction::NegativeOne);
    assert(scorebot::buttonActionForPin(true, 4) == ButtonAction::None);
    // Physical Blue +1/+5 use the alternate MCP wiring regardless of color.
    assert(scorebot::buttonActionForPin(false, 3, 860931256u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 860931256u) == ButtonAction::PlusFive);
    assert(scorebot::buttonActionForPin(false, 2, 2934574912u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 2934416992u) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(true, 2, 860931256u) == ButtonAction::Ok);
    assert(scorebot::usesPlayerBoardV9(0x76c3cfc0));
    assert(!scorebot::usesPlayerBoardV9(860931256u));
    assert(!scorebot::usesPlayerBoardV9(2934574912u));
    assert(scorebot::buttonActionForPin(false, 3, 0x76c3cfc0) == ButtonAction::PlusOne);
    assert(scorebot::buttonActionForPin(false, 2, 0x76c3cfc0) == ButtonAction::PlusFive);
    testScoreButtonGuard();
    testScoreReleaseMustSettle();
    testOverlappingButtons();
    std::cout << "Button-input tests passed\n";
    return 0;
}
