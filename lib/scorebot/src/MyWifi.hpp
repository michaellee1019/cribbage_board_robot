#pragma once

#include <painlessMesh.h>

class Coordinator;

class MyWifi {
public:
    explicit MyWifi(Coordinator *c);
    void setup();
    void loop();

    void senderTask();
    void sendBroadcast(const String& message) const;
    uint32_t getMyPeerId();
    std::list<uint32_t> getPeers();

private:
    painlessMesh mesh;
    Coordinator* coordinator;
    QueueHandle_t outgoingMsgQueue;
    SemaphoreHandle_t ack;
    volatile bool ackReceived;
};