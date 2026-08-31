#pragma once

#include <cstdint>

namespace scorebot::usb {

inline constexpr uint8_t kDisconnectMissCount = 3;
inline constexpr uint32_t kFinalSleepProbeMs = 25;

class ConnectionState {
public:
    constexpr bool observe(bool startOfFrameReceived) {
        if (startOfFrameReceived) {
            connected_ = true;
            consecutiveMisses_ = 0;
        } else if (connected_) {
            if (consecutiveMisses_ < kDisconnectMissCount) {
                ++consecutiveMisses_;
            }
            if (consecutiveMisses_ >= kDisconnectMissCount) {
                connected_ = false;
                consecutiveMisses_ = 0;
            }
        }
        return connected_;
    }

    constexpr bool connected() const {
        return connected_;
    }

    constexpr uint8_t consecutiveMisses() const {
        return consecutiveMisses_;
    }

private:
    bool connected_{false};
    uint8_t consecutiveMisses_{0};
};

}  // namespace scorebot::usb
