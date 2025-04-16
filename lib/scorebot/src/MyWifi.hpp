#pragma once

#include <HT16Display.hpp>
#include <painlessMesh.h>

class Coordinator;

class MyWifi {
public:
    explicit MyWifi(Coordinator *c);
    void setup();
    void loop();

    void senderTask();
    void sendBroadcast(const String& message) const;
    void shutdown();
    void start();
    uint32_t getMyPeerId();
    std::list<uint32_t> getPeers();

private:
    painlessMesh mesh;
    Coordinator* coordinator;
    QueueHandle_t outgoingMsgQueue;
    SemaphoreHandle_t ack;
    volatile bool ackReceived;
};

void performOTAUpdate(HT16Display* display);
