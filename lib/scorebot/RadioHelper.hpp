#ifndef RADIOHELPER_HPP
#define RADIOHELPER_HPP

#include "RF24.h"

class RadioHelper {
    RF24 radio;

public:

    explicit RadioHelper(RF24 radio)
        : radio{radio} {};

    // Send toSend and invoke callback if successfully sent.
    template <typename T, typename F>
    [[nodiscard]] bool doSend(T* toSend, F&& callback) {
        if (this->radio.write(toSend, sizeof(T))) {
            return callback();
        }
        return false;
    }

    template <typename T>
    [[nodiscard]] bool doRead(T* out) {
        if (this->radio.isAckPayloadAvailable()) {
            this->radio.read(out, sizeof(T));
            return true;
        }
        return false;
    }

    template <typename T>
    [[nodiscard]]
    bool doAck(uint8_t pipe, T* acked) {
        return radio.writeAckPayload(pipe, acked, sizeof(T));
    }

    void doRadioSetup() {
        radio.begin();
        radio.setPALevel(RF24_PA_LOW);
        radio.setDataRate(RF24_250KBPS);
        radio.enableAckPayload();
        radio.setRetries(5, 5);  // delay, count
    }

    auto openWritingPipe(const byte* address) {
        return radio.openWritingPipe(address);
    }

    auto printPrettyDetails() {
        return radio.printPrettyDetails();
    }

    auto stopListening() {
        return radio.stopListening();
    }

    auto openReadingPipe(int pipNumber, const byte* address) {
        return radio.openReadingPipe(pipNumber, address);
    }

    auto startListening() {
        return radio.startListening();
    }

    auto available() {
        return radio.available();
    }
};



#endif  // RADIOHELPER_HPP
