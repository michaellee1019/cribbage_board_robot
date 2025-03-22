#include <Arduino.h>
#include <set>

#include <WiFi.h>
#include <painlessMesh.h>

#include <freertos/task.h>

#include <utils.hpp>
#include <RotaryEncoder.hpp>
#include <HT16Display.hpp>
#include <ButtonGrid.hpp>
#include <State.hpp>
#include <game.hpp>

#define MESH_PREFIX "mesh_network"
#define MESH_PASSWORD "mesh_password"
#define MESH_PORT 5555

State state{};


void buttonISR() {
    state.buttonPressed = true;
}




HT16Display primaryDisplay;
HT16Display display2;
HT16Display display3;
HT16Display display4;

RotaryEncoder encoder{&primaryDisplay};
ButtonGrid buttonGrid(&primaryDisplay);


void newConnectionCallback(uint32_t nodeId) {
    state.peers.emplace(nodeId);
    String msg = "con " + String(nodeId);
    Serial.println(msg);
}

void lostConnectionCallback(uint32_t nodeId) {
    state.peers.erase(nodeId);
    String msg = "dis " + String(nodeId);
    Serial.println(msg);
}

void receivedCallback(uint32_t from, String& msg) {
    Serial.printf("received message [%s] from [%u]\n", msg.c_str(), from);
    String toSend = String(from) + " " + msg;
    primaryDisplay.print(msg);
}

Game game{};

static TaskHandle_t seesawTaskHandle = NULL;
void IRAM_ATTR seesawInterrupt() {
    // TODO: remove this legacy thing in a minute
    state.interrupted = true;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Notify the task
    vTaskNotifyGiveFromISR(seesawTaskHandle, &xHigherPriorityTaskWoken);

    // If the notification unblocked a higher priority task, yield
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void meshSetup() {
    // Initialize the mesh
    state.mesh.setDebugMsgTypes(ERROR | STARTUP | MESH_STATUS | CONNECTION | SYNC | S_TIME |
                                COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | APPLICATION | DEBUG);

    state.mesh.init(MESH_PREFIX, MESH_PASSWORD, &state.userScheduler, MESH_PORT, WIFI_MODE_APSTA, 1);
    state.mesh.onNewConnection(&newConnectionCallback);
    state.mesh.onDroppedConnection(&lostConnectionCallback);
    state.mesh.onReceive(&receivedCallback);
    state.peers.emplace(state.mesh.getNodeId());
}

void meshLoop() {
    state.mesh.update();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Wire.begin(5, 6);

    if (numI2C() > 3) {
        state.isLeaderboard = true;
    }

    xTaskCreate(seesawTask, "SeesawTask", 4096, NULL, 5, &seesawTaskHandle);

    meshSetup();

    primaryDisplay.setup(0x70);
    primaryDisplay.print("RED");

    if (state.isLeaderboard) {
        display2.setup(0x71);
        display2.print("BLUE");
        display3.setup(0x72);
        display3.print("GREEN");
        display4.setup(0x73);
        display4.print("YELLOW");
    }

    Serial.println("hardware setup started");

    primaryDisplay.setup(0x70);

    if (state.isLeaderboard) {
        primaryDisplay.print("RED");

        display2.setup(0x71);
        display2.print("BLUE");
        display3.setup(0x72);
        display3.print("GREN");
        display4.setup(0x73);
        display4.print("YELW");
    }
    buttonGrid.setup();
    encoder.setup();

    Serial.println("setup done");
}

void loop() {
    meshLoop();
    buttonGrid.loop(&state);
    encoder.loop(&state);
}
