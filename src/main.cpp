#include <Arduino.h>

#include <SparkFun_Alphanumeric_Display.h>

#include <Adafruit_MCP23X17.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include <WiFi.h>
#include <painlessMesh.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


#define MESH_PREFIX     "mesh_network"
#define MESH_PASSWORD   "mesh_password"
#define MESH_PORT       5555


class HT16Display {
    HT16K33 driver;
public:
    HT16Display() = default;
    void setup() {
        while (!driver.begin()) {
            Serial.println("Device did not acknowledge!");
        }
        Serial.println("Display acknowledged.");

        driver.print("RYAN");
    }

    // Talks like a duck!
    template<typename... Args>
    auto print(Args&&... args) {
        return driver.print(std::forward<Args>(args)...);
    }
};

class ButtonGrid {
    HT16Display* const display;
    Adafruit_MCP23X17 buttonGpio;

    static constexpr u32_t okPin = 4;
    static constexpr u32_t plusone = 3;
    static constexpr u32_t plusfive = 2;
    static constexpr u32_t negone = 1;
    static constexpr u32_t add = 0;
    static constexpr auto pins = {okPin, plusone, plusfive, negone, add};
public:
    explicit ButtonGrid(HT16Display* const display)
        : display{display} {}

    void setup() {
        buttonGpio.begin_I2C(0x20, &Wire);
        for (auto&& pin : pins) {
            buttonGpio.pinMode(pin, INPUT_PULLUP);
        }
    }

    void loop() {
        Serial.println("Button grid loop");
        if (!buttonGpio.digitalRead(okPin)) {
            Serial.println("Button OK Pressed!");
            display->print("OK");
            delay(100);
        }
        if (!buttonGpio.digitalRead(plusone)) {
            Serial.println("Button +1 Pressed!");
            display->print("+1");
            delay(100);
        }

        if (!buttonGpio.digitalRead(plusfive)) {
            Serial.println("Button +5 Pressed!");
            display->print("+5");
            delay(100);
        }
        if (!buttonGpio.digitalRead(negone)) {
            Serial.println("Button -1 Pressed!");
            display->print("-1");
            delay(100);
        }
        if (!buttonGpio.digitalRead(add)) {
            Serial.println("Button ADD Pressed!");
            display->print("ADD");
            delay(100);
        }
    }
};

class RotaryEncoder {
    #define SS_SWITCH 24
    #define SS_NEOPIX 6
    #define SEESAW_ADDR 0x36

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

    public:
        explicit RotaryEncoder() = default;
        void setup() {
            ss.begin(SEESAW_ADDR);
            sspixel.begin(SEESAW_ADDR);
            sspixel.setBrightness(20);
            sspixel.setPixelColor(0, 0xFF0000);
            sspixel.show();

            ss.pinMode(SS_SWITCH, INPUT_PULLUP);
        }

        void loop() {
            Serial.println("Encoder position: " + String(ss.getEncoderPosition()));

            if (!ss.digitalRead(SS_SWITCH)) {
                Serial.println("Switch pressed");
            }
        }
};

class RTButton {
public:
    RTButton(int pin, int debounceDelay, int doubleClickThreshold)
        : buttonPin(pin), debounceDelayMs(debounceDelay), doubleClickThresholdMs(doubleClickThreshold), lastInterruptTime(0) {
        buttonSemaphore = xSemaphoreCreateBinary();

        debounceTimer = xTimerCreate(
            "Debounce Timer",
            pdMS_TO_TICKS(debounceDelayMs),
            pdFALSE, // One-shot timer
            this,
            [](TimerHandle_t xTimer) {
                auto* button = static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                if (button) button->debounceCallback();
            }
        );

        doubleClickTimer = xTimerCreate(
            "Double-Click Timer",
            pdMS_TO_TICKS(doubleClickThresholdMs),
            pdFALSE, // One-shot timer
            this,
            [](TimerHandle_t xTimer) {
                auto* button = static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                if (button) button->doubleClickCallback();
            }
        );

        if (debounceTimer == nullptr || doubleClickTimer == nullptr) {
            Serial.println("Failed to create timers");
            // TODO: have a failure mode
        }
    }


    virtual ~RTButton() {
        if (debounceTimer != nullptr) {
            xTimerDelete(debounceTimer, portMAX_DELAY);
        }
        if (doubleClickTimer != nullptr) {
            xTimerDelete(doubleClickTimer, portMAX_DELAY);
        }
        if (buttonSemaphore != nullptr) {
            vSemaphoreDelete(buttonSemaphore);
        }
    }

    void init() {
        pinMode(buttonPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(buttonPin), ISRHandler, this, CHANGE);

        BaseType_t taskCreated = xTaskCreate(
            [](void* param) {
                auto* button = static_cast<RTButton*>(param);
                if (button) button->buttonTask();
            },
            "Button Task", 2048, this, 1, nullptr);

        if (taskCreated != pdPASS) {
            Serial.println("Failed to create button task");
            // TODO: handle failure modes
        }
    }

protected:
    virtual void onPress() {
        Serial.println("Button Pressed");
    }

    virtual void onRelease() {
        Serial.println("Button Released");
    }

    virtual void onSingleClick() {
        Serial.println("Single Click Detected");
    }

    virtual void onDoubleClick() {
        Serial.println("Double Click Detected");
        // TODO: send
    }

private:
    const unsigned short buttonPin;
    const unsigned short debounceDelayMs;
    const unsigned short doubleClickThresholdMs;

    SemaphoreHandle_t buttonSemaphore;
    TimerHandle_t debounceTimer;
    TimerHandle_t doubleClickTimer;

    volatile bool buttonPressed = false;
    volatile bool isDoubleClick = false;
    volatile size_t clickCount = 0;
    volatile TickType_t lastInterruptTime = 0;

    static void IRAM_ATTR ISRHandler(void* arg) {
        auto* button = static_cast<RTButton*>(arg);
        if (button) button->handleInterrupt();
    }

    void handleInterrupt() {
        TickType_t interruptTime = xTaskGetTickCountFromISR();

        if ((interruptTime - lastInterruptTime) * portTICK_PERIOD_MS > debounceDelayMs) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xTimerResetFromISR(debounceTimer, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        lastInterruptTime = interruptTime;
    }

    void debounceCallback() {
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            xSemaphoreGive(buttonSemaphore);
        } else {
            buttonPressed = false;
            xSemaphoreGive(buttonSemaphore);
        }
    }

    void doubleClickCallback() {
        if (clickCount == 1) {
            onSingleClick();
        } else if (clickCount == 2) {
            isDoubleClick = true;
            onDoubleClick();
        }
        clickCount = 0;
    }

    [[noreturn]]
    void buttonTask() {
        while (true) {
            if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
                if (buttonPressed) {
                    onPress();
                    clickCount++;
                    xTimerStart(doubleClickTimer, 0);
                } else {
                    onRelease();
                }
            }
        }
    }
};

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Mesh] New Connection, nodeId = %u\n", nodeId);
}

void lostConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Mesh] Lost Connection, nodeId = %u\n", nodeId);
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("[Mesh] Received from %u: %s\n", from, msg.c_str());
}

RotaryEncoder encoder;
HT16Display display;
ButtonGrid buttonGrid(&display);

painlessMesh mesh;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting up");

  // TODO: do we need this Wire.begin?
  Wire.begin(5, 6);

  encoder.setup();
  display.setup();
  buttonGrid.setup();

  // Initialize the mesh
  // mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);  // set before init()
  mesh.setDebugMsgTypes(ERROR);  // set before init()
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&lostConnectionCallback);
  mesh.onReceive(&receivedCallback);
}

void loop() {
  mesh.update();
  buttonGrid.loop();
  encoder.loop();
}
