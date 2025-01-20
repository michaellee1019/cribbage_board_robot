#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <Message.hpp>

#define LED_PIN         33
#define BUTTON_PIN      27

// #define COLOR_RED       1
// #define COLOR_BLUE      2
// #define IS_COLOR(x)     (COLOR == X)

static const uint8_t address_RED[] =
    {0xC8, 0x2E, 0x18, 0xF0, 0x2E, 0x6C};
static const uint8_t address_BLUE[] =
    {0x08, 0xB6, 0x1F, 0xB8, 0xAA, 0x08};

struct MacAddress {
    const uint8_t* mac_addr;
    static constexpr size_t macSize = 18;
    void print(char macStr[macSize]) const {
        snprintf(macStr,
                 macSize,
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5]);
    }
};


struct ESPNowHandler {
    // Task Handles
    // TODO: can we just kill this; it's a void* and appears to only be here so we can delete the task
    TaskHandle_t sendTaskHandle;

    // Callback for received data
    static void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(LED_PIN, LOW);
    }


    void espSetup() {
        Serial.println("Hello. Starting wifi.");

        // Initialize WiFi in station mode
        WiFiClass::mode(WIFI_STA);
        esp_wifi_start();
        WiFi.disconnect();
        if (esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
            Serial.println("Failed to set channel");
        }

        // Get the MAC address of the ESP32
        String macAddress = WiFi.macAddress();

        // Print the MAC address to the Serial Monitor
        Serial.println("ESP32 MAC Address: " + macAddress);

        // Initialize ESP-NOW
        if (esp_now_init() != ESP_OK) {
            Serial.println("ESP-NOW initialization failed");
            return;
        }
        if (esp_now_register_recv_cb(onDataRecv) != ESP_OK) {  // Register receive callback
            Serial.println("ESP-NOW callback register failed");
            return;
        }

        // Add broadcast peer manually (FF:FF:FF:FF:FF:FF for broadcast)
        esp_now_peer_info_t peerInfo = {};
        memset(&peerInfo, 0, sizeof(peerInfo));
        peerInfo.channel = 1;  // Must match your channel
        peerInfo.encrypt = false;
#if (COLOR==1)
        memcpy(peerInfo.peer_addr, address_BLUE, 6);
#elif (COLOR==2)
        memcpy(peerInfo.peer_addr, address_RED, 6);
#endif

        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to add broadcast peer");
        }}
};
ESPNowHandler espNow;

// Counter to send
volatile int counter = 0;




// Function to send broadcast messages
[[noreturn]] void
sendBroadcast(void* param) {
    for (;;) {
        counter++;

        const auto resp = esp_now_send(
            // nullptr sends to all peers
            nullptr,
            // const_cast<int*>(&counter) removes volatile (reinterpret_cast cannot handle volatile)
            address_RED,
            sizeof(counter)
        );

        Serial.printf("Status: %d", resp);
        Serial.printf(" Sent counter: %d\n", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Send every 1 second
    }
}


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
        esp_now_send(
            // nullptr sends to all peers
            nullptr,
            // const_cast<int*>(&counter) removes volatile (reinterpret_cast cannot handle volatile)
            address_RED,
            sizeof(counter)
        );
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

RTButton button(BUTTON_PIN, 50, 500);



void setup() {
    pinMode(LED_PIN,OUTPUT);

    Serial.begin(115200);
    sleep(3);

    espNow.espSetup();

    // Create FreeRTOS task for sending messages
    // xTaskCreatePinnedToCore(sendBroadcast, "SendBroadcast", 2048, nullptr, 1, &espNow.sendTaskHandle, 1);

    button.init();
}

void loop() {}

