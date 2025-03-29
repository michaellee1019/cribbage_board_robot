#include <MyWifi.hpp>
#include <Event.hpp>
#include <Coordinator.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <painlessMesh.h>

#define MESH_PREFIX "mesh_network"
#define MESH_PASSWORD "mesh_password"
#define MESH_PORT 5555

MyWifi::MyWifi(Coordinator *c)
: coordinator{c},
  mesh{}
{}


void MyWifi::setup() {
    // Initialize the mesh
    mesh.setDebugMsgTypes(ERROR);
    // mesh.setDebugMsgTypes(ERROR | STARTUP | MESH_STATUS | CONNECTION | SYNC | S_TIME |
    //                             COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | APPLICATION | DEBUG);

    mesh.init(MESH_PREFIX, MESH_PASSWORD, &coordinator->scheduler, MESH_PORT, WIFI_MODE_APSTA, 1);

    mesh.onNewConnection([this](uint32_t from) {
        Event e{};
        e.type = EventType::NewPeer;
        e.peerId = from;
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });

    // mesh.onDroppedConnection(&lostConnectionCallback);
    mesh.onReceive([this](uint32_t from, const String &msg) {
        Event e{};
        e.type = EventType::MessageReceived;
        e.peerId = from;
        strlcpy(e.wifiMessage, msg.c_str(), sizeof(e.wifiMessage));
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });

    {
        Event addSelf{};
        addSelf.type = EventType::NewPeer;
        addSelf.peerId = mesh.getNodeId();

        xQueueSend(coordinator->eventQueue, &addSelf, portMAX_DELAY);
    }
}
void MyWifi::loop() {
    mesh.update();
}
