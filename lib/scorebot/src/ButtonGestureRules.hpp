#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace scorebot {

inline constexpr uint32_t kScoreButtonGuardUs = 120000;
inline constexpr int32_t kScoreReleaseBounceUs = 25000;
inline constexpr uint32_t kButtonReleaseDebounceMs = 12;

struct ButtonEdges {
    uint16_t pressed;
    uint16_t completed;
    bool overlap;
};

constexpr bool hasMultipleButtons(uint16_t mask) {
    return (mask & (mask - 1)) != 0;
}

constexpr ButtonEdges buttonEdges(uint16_t previous, uint16_t current,
                                  uint16_t latchedLevels, uint16_t latchedPresses,
                                  bool latchedOverlap) {
    const uint16_t seen = current | latchedLevels | latchedPresses;
    return {
        static_cast<uint16_t>(seen & ~previous),
        static_cast<uint16_t>((previous | seen) & ~current),
        latchedOverlap || hasMultipleButtons(current) || hasMultipleButtons(latchedLevels),
    };
}

constexpr bool rejectButtonGesture(ButtonEdges edges, uint16_t scoreButtons,
                                   uint16_t commitButtons, bool recentScore) {
    const bool newCommit = (edges.pressed & commitButtons) != 0;
    // An expander can deliver a tap and a later press in one dispatch. If
    // either could commit an ambiguous edit, consume the gesture as a unit.
    const bool batchedCommit = (edges.completed & commitButtons) != 0 &&
                               hasMultipleButtons(edges.pressed | edges.completed);
    return edges.overlap || batchedCommit ||
           (newCommit && (recentScore || (edges.completed & scoreButtons) != 0));
}

template <typename Score>
class StagedScoreButtons {
    struct Entry {
        uint32_t releasedAt;
        Score score;
        uint8_t pin;
    };

    std::array<Entry, 16> entries_{};
    size_t first_{0};
    size_t size_{0};

public:
    bool empty() const { return size_ == 0; }
    void clear() { size_ = 0; }

    bool stage(uint32_t now, Score score, uint8_t pin) {
        if (size_ == entries_.size()) return false;
        entries_[(first_ + size_) % entries_.size()] = {now, score, pin};
        ++size_;
        return true;
    }

    void discardBouncedReleases(uint32_t now, uint16_t pressed) {
        if (pressed == 0) return;
        size_t kept = 0;
        for (size_t offset = 0; offset < size_; ++offset) {
            const Entry& entry = entries_[(first_ + offset) % entries_.size()];
            if ((pressed & (1u << entry.pin)) != 0 &&
                static_cast<int32_t>(now - entry.releasedAt) < kScoreReleaseBounceUs) {
                // The key closed again before the release could settle. Its
                // next release can still complete the same physical tap.
                continue;
            }
            entries_[(first_ + kept) % entries_.size()] = entry;
            ++kept;
        }
        size_ = kept;
    }

    bool pop(uint32_t now, Score& score) {
        if (empty()) return false;
        const Entry& next = entries_[first_];
        // A queued press may have been captured before the dispatcher
        // observed the preceding release. Treat that gap as inside the guard.
        if (static_cast<int32_t>(now - next.releasedAt) <
            static_cast<int32_t>(kScoreButtonGuardUs)) return false;
        score = next.score;
        first_ = (first_ + 1) % entries_.size();
        --size_;
        return true;
    }
};

class ButtonReleaseDebounce {
    std::array<uint32_t, 5> lastRelease_{};
    uint16_t observed_{0};

public:
    bool accept(uint8_t pin, uint32_t now) {
        const uint16_t bit = 1u << pin;
        const bool fresh = (observed_ & bit) == 0 ||
                           now - lastRelease_[pin] >= kButtonReleaseDebounceMs;
        observed_ |= bit;
        lastRelease_[pin] = now;
        return fresh;
    }
};

}  // namespace scorebot
