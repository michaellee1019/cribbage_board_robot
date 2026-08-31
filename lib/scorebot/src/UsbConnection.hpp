#pragma once

#include <UsbConnectionRules.hpp>

#include <cstdint>

class UsbConnectionMonitor {
public:
    void begin();
    bool poll();
    bool connected() const;
    bool connectionAppearsWithin(uint32_t windowMs);

private:
    scorebot::usb::ConnectionState state_;

    bool sampleStartOfFrame();
};
