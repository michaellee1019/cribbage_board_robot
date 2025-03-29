#pragma once

#include <painlessMesh.h>

class Coordinator;

class MyWifi {
public:
    explicit MyWifi(Coordinator *c);
    void setup();
    void loop();
private:
    painlessMesh mesh;
    Coordinator* coordinator;
};