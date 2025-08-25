#include <MyWifi.hpp>
#include <Event.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>

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
{
    CHECK_POINTER(outgoingMsgQueue, ErrorCode::QUEUE_CREATE_FAILED, "MyWifi outgoing message queue");
    CHECK_POINTER(ack, ErrorCode::SEMAPHORE_CREATE_FAILED, "MyWifi ACK semaphore");
}

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

void MyWifi::sendTo(uint32_t nodeId, const String& message) {
    mesh.sendSingle(nodeId, message);
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

    // Set the leaderboard as the root node for now.
    // TODO: consider if this is necessary.
    if (coordinator->myRole() == BoardRole::Leader) {
        mesh.setRoot(true);
        Serial.println("Set as mesh root node (Leader)");
    } else {
        mesh.setRoot(false);
        Serial.println("Set as mesh child node (not Leader)");
    }
    mesh.setContainsRoot(true);

    auto roleConfig = coordinator->myRoleConfig();
    Serial.printf("NodeId: %u, Role: %s\n", 
                 mesh.getNodeId(), roleConfig ? roleConfig->name.c_str() : "UNKNOWN");
    Serial.printf("Mesh subnet: %s\n", mesh.subConnectionJson().c_str());

    BaseType_t taskResult = xTaskCreate(
        wifiTask, // Static method wrapper
        "WifiSenderTask",
        4096,
        this, // Pass instance pointer as param
        2,
        nullptr
    );
    CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "MyWifi sender task");

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
        Serial.printf("[Received from=%i] [%s]\n", from, msg.c_str());
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
