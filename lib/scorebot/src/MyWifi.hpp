#pragma once

#include <painlessMesh.h>

class Coordinator;

class MyWifi {
public:
    explicit MyWifi(Coordinator *c);
    void setup();
    void loop();

    auto sendBroadcast(const String& message) {
        return mesh.sendBroadcast(message);
    }
private:
    painlessMesh mesh;
    Coordinator* coordinator;
};