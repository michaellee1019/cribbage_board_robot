#include <Arduino.h>
#include <set>


#include <Adafruit_MCP23X17.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include <WiFi.h>
#include <painlessMesh.h>

#include "utils.hpp"
#include <HT16Display.hpp>

#define MESH_PREFIX "mesh_network"
#define MESH_PASSWORD "mesh_password"
#define MESH_PORT 5555

struct State {
    Scheduler userScheduler;  // Required for custom scheduled tasks
    painlessMesh mesh;

    std::set<uint32_t> peers;

    bool isLeaderboard = false;
    int playerNumber = 0;
    volatile bool buttonPressed = false;
};

State state{};

std::map<int, String> playerNumberMap = {
    {1, "RED"},
    {2, "BLUE"},
    {3, "GREN"},
    {4, "WHIT"},
};



static void buttonISR() {
    state.buttonPressed = true;
}


class ButtonGrid {
    HT16Display* const display;
    Adafruit_MCP23X17 buttonGpio;

    static constexpr u32_t interruptPin = 8;

    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusone = 3;
    static constexpr u32_t plusfive = 2;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;
    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};

public:
    explicit ButtonGrid(HT16Display* const display) : display{display} {}

    void setup() {
        buttonGpio.begin_I2C(0x20, &Wire);
        buttonGpio.setupInterrupts(true, false, LOW);
        for (auto&& pin : pins) {
            buttonGpio.pinMode(pin, INPUT_PULLUP);
            buttonGpio.setupInterruptPin(pin, CHANGE);
        }
        pinMode(interruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(interruptPin), buttonISR, CHANGE);
        buttonGpio.clearInterrupts();
    }

    void loop() {
        if (!state.buttonPressed) {
            return;
        }

        uint8_t intPin = buttonGpio.getLastInterruptPin();   // Which pin caused it?
        uint8_t intVal = buttonGpio.getCapturedInterrupt();  // What was the level?
        if (intPin != MCP23XXX_INT_ERR) {
            display->print(strFormat("%d %2x", intPin, intVal));
        }
        // } else {
        //     display->print("Sad");
        // }
        state.buttonPressed = false;
        buttonGpio.clearInterrupts();
    }
};

volatile bool interrupted = false;
static void seesawInterrupt() {
    interrupted = true;
}

class RotaryEncoder {
#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36
#define SEESAW_INTERRUPT 7

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

    HT16Display* const display;

public:
    explicit RotaryEncoder(HT16Display* const display) : display{display} {}

    void setup() {
        ss.begin(SEESAW_ADDR);
        sspixel.begin(SEESAW_ADDR);
        sspixel.setBrightness(20);
        sspixel.setPixelColor(0, 0xFAEDED);
        sspixel.show();

        // https://github.com/adafruit/Adafruit_Seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

        pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
        ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
        ss.setGPIOInterrupts(mask, true);
        ss.enableEncoderInterrupt();

        attachInterrupt(digitalPinToInterrupt(SEESAW_INTERRUPT), seesawInterrupt, CHANGE);
    }

    void loop(State* const state) {
        if (!interrupted) {
            return;
        }
        auto pressed = !ss.digitalRead(SS_SWITCH);
        auto val = ss.getEncoderPosition();

        Serial.printf("playerNumber: %d\n", state->playerNumber);
        Serial.printf("playerColor: %s\n", playerNumberMap[state->playerNumber].c_str());
        Serial.printf("encoder val: %d\n", val);

        // initialize player selection
        if (!state->isLeaderboard && state->playerNumber == 0) {
            if (val > -1) {
                display->print(playerNumberMap[val]);
            }

            if (pressed) {
                state->playerNumber = val;
                Serial.printf("Player set to: %s\n", playerNumberMap[state->playerNumber].c_str());
            }
        }
        // String msg = String(val) + " " + String(pressed);
        // const auto sent = mesh.sendBroadcast(msg, false);
        // Serial.printf("send message [%s], result [%i]\n", msg.c_str(), sent);
        // display->print(msg);
        interrupted = false;
    }
};


int numI2C() {
    byte count = 0;

    for (byte i = 8; i < 120; i++) {
        Wire.beginTransmission(i);        // Begin I2C transmission Address (i)
        if (Wire.endTransmission() == 0)  // Receive 0 = success (ACK response)
        {
            count++;
        }
    }
    return count;
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

void setup() {
    Serial.begin(115200);

    delay(2000);
    Wire.begin(5, 6);

    if (numI2C() > 3) {
        state.isLeaderboard = true;
    }
    // Initialize the mesh
    state.mesh.setDebugMsgTypes(ERROR | STARTUP | MESH_STATUS | CONNECTION | SYNC | S_TIME |
                                COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | APPLICATION | DEBUG);

    state.mesh.init(MESH_PREFIX, MESH_PASSWORD, &state.userScheduler, MESH_PORT, WIFI_MODE_APSTA, 1);
    state.mesh.onNewConnection(&newConnectionCallback);
    state.mesh.onDroppedConnection(&lostConnectionCallback);
    state.mesh.onReceive(&receivedCallback);
    state.peers.emplace(state.mesh.getNodeId());

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
    state.mesh.update();
    buttonGrid.loop();
    encoder.loop(&state);
}
