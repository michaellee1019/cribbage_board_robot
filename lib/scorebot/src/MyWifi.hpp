#pragma once

#include <painlessMesh.h>

class Coordinator;

class MyWifi {
public:
    MyWifi(Coordinator *c);
    void setup();
private:
    painlessMesh mesh;
    Coordinator* coordinator;
};