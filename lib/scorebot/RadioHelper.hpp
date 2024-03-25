#ifndef RADIOHELPER_HPP
#define RADIOHELPER_HPP

#include "RF24.h"

// Send toSend and invoke callback if successfully sent.
template <typename T, typename F>
[[nodiscard]] bool doSend(RF24* radio, T* toSend, F&& callback) {
    if (radio->write(toSend, sizeof(T))) {
        return callback();
    }
    return false;
}

template <typename T>
[[nodiscard]] bool doRead(RF24* radio, T* out) {
    if (radio->isAckPayloadAvailable()) {
        radio->read(out, sizeof(T));
        return true;
    }
    return false;
}

template <typename T>
[[nodiscard]]
bool doAck(RF24* radio, uint8_t pipe, T* acked) {
    return radio->writeAckPayload(pipe, acked, sizeof(T));
}

void doRadioSetup(RF24& rf24) {
    rf24.begin();
    rf24.setDataRate(RF24_250KBPS);
    rf24.enableAckPayload();
    rf24.setRetries(5, 5);  // delay, count
}

#endif // RADIOHELPER_HPP
