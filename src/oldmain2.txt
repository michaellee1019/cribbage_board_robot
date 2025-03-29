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

#define MESH_PREFIX "mesh_network"
#define MESH_PASSWORD "mesh_password"
#define MESH_PORT 5555

GameState gameState;
State state{&gameState};

void seesawTask(void *pvParameters) {
    for (;;) {
        // Wait for the notification from the ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        state.primaryDisplay.print(state.encoder.ss.getEncoderPosition());

        Serial.println("seesawTask");

    }
}

void buttonISR() {
    gameState.buttonPressed = true;
}






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
    state.primaryDisplay.print(msg);
}

static TaskHandle_t seesawTaskHandle = NULL;
void IRAM_ATTR seesawInterrupt() {
    // TODO: remove this legacy thing in a minute
    gameState.interrupted = true;

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

void setupIO() {
    Serial.begin(115200);
    delay(2000);
    Wire.begin(5, 6);
    Serial.println("Wire.begin(5, 6);");
}

class Leaderboard {
    bool isLeaderboard = false;
public:
    void setup() {
        isLeaderboard = numI2C() > 3;
        if (isLeaderboard) {
            state.display2.setup(0x71);
            state.display2.print("BLUE");
            state.display3.setup(0x72);
            state.display3.print("GREN");
            state.display4.setup(0x73);
            state.display4.print("WHTE");
        }
    }
};

Leaderboard leaderboard;

void setup() {
    setupIO();

    meshSetup();
    leaderboard.setup();

    state.primaryDisplay.setup(0x70);

    state.buttonGrid.setup();
    state.encoder.setup();

    xTaskCreate(seesawTask, "SeesawTask", 4096, NULL, 5, &seesawTaskHandle);

    Serial.println("setup done");
}

void loop() {
    meshLoop();
    state.buttonGrid.loop(&gameState);
    state.encoder.loop(&gameState);
}
