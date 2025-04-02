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
: mesh{},
  coordinator{c},
  outgoingMsgQueue{xQueueCreate(10, sizeof(String))},
  ack{xSemaphoreCreateBinary()},
  ackReceived{false}
{}

// TODO: typedef for node/peer ID type
uint32_t MyWifi::getMyPeerId() {
    return mesh.getNodeId();
}
std::list<uint32_t> MyWifi::getPeers() {
    return mesh.getNodeList(true);
}

void wifiTask(void*param) {
    static_cast<MyWifi*>(param)->senderTask(); // Cast and call instance method
}

void MyWifi::sendBroadcast(const String& message) const {
    xQueueSend(outgoingMsgQueue, &message, portMAX_DELAY);
}


[[noreturn]]
void MyWifi::senderTask() {
    String outgoing;
    // const int retryIntervalMs = 500;
    // const int maxRetries = 5;

    while (true) {
        if (xQueueReceive(outgoingMsgQueue, &outgoing, portMAX_DELAY) == pdTRUE) {
            mesh.sendBroadcast(outgoing);
            // ackReceived = false;
            // for (int retry = 0; retry < maxRetries && !ackReceived; retry++) {
            //     mesh.sendBroadcast(outgoing);
            //     if (xSemaphoreTake(ack, pdMS_TO_TICKS(retryIntervalMs)) == pdTRUE) {
            //         ackReceived = true;
            //         break;
            //     }
            }
    }
}

void MyWifi::setup() {
    // Initialize the mesh
    mesh.setDebugMsgTypes(ERROR);
    // mesh.setDebugMsgTypes(ERROR | STARTUP | MESH_STATUS | CONNECTION | SYNC | S_TIME |
    //                             COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | APPLICATION | DEBUG);

    mesh.init(MESH_PREFIX, MESH_PASSWORD, &coordinator->scheduler, MESH_PORT, WIFI_MODE_APSTA, 1);

    xTaskCreate(
        wifiTask, // Static method wrapper
        "WifiSenderTask",
        4096,
        this, // Pass instance pointer as param
        2,
        nullptr
    );

    mesh.onDroppedConnection([this](uint32_t from) {
        Event e{};
        e.type = EventType::LostPeer;
        e.lostPeer.peerId = from;
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });

    mesh.onNewConnection([this](uint32_t from) {
        Event e{};
        e.type = EventType::NewPeer;
        e.newPeer.peerId = from;
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });

    // mesh.onDroppedConnection(&lostConnectionCallback);
    mesh.onReceive([this](uint32_t from, const String &msg) {
        Event e{};
        e.type = EventType::MessageReceived;
        e.messageReceived.peerId = from;
        strlcpy(e.messageReceived.wifiMessage, msg.c_str(), sizeof(e.messageReceived.wifiMessage));
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });

    {
        Event addSelf{};
        addSelf.type = EventType::NewPeer;
        addSelf.newPeer.peerId = mesh.getNodeId();

        xQueueSend(coordinator->eventQueue, &addSelf, portMAX_DELAY);
    }
}
void MyWifi::loop() {
    mesh.update();
}
