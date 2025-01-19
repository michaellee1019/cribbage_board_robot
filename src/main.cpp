#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <Message.hpp>

#define LED_PIN         33
#define BUTTON_PIN      27

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


// Counter to send
volatile int counter = 0;

// Task Handles
// TODO: can we just kill this; it's a void* and appears to only be here so we can delete the task
TaskHandle_t sendTaskHandle;

// Callback for received data
void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
    char macStr[18];
    const MacAddress mac{mac_addr};
    mac.print(macStr);
    Serial.printf("Received message from %s: %d\n", macStr, *(int*)data);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
}

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


volatile bool buttonPressed = false; // A flag set by the ISR
SemaphoreHandle_t buttonSemaphore;   // A semaphore to synchronize the task

void IRAM_ATTR buttonISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Set the flag or directly signal the semaphore
    buttonPressed = true;

    // Give the semaphore to wake up the task
    xSemaphoreGiveFromISR(buttonSemaphore, &xHigherPriorityTaskWoken);

    // Request a context switch if a higher-priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void buttonTask(void *pvParameters) {
    while (true) {
        // Wait for the semaphore
        if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
            // Clear the flag (optional, depending on your logic)
            buttonPressed = false;

            // Handle the button press (update state, etc.)
            Serial.println("Button pressed! Updating state...");
        }
    }
}

void setup() {
    pinMode(LED_PIN,OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    sleep(3);
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

    // if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
    }
    //}

    // Create FreeRTOS task for sending messages
    xTaskCreatePinnedToCore(sendBroadcast, "SendBroadcast", 2048, nullptr, 1, &sendTaskHandle, 1);

    // Create the semaphore
    buttonSemaphore = xSemaphoreCreateBinary();

    // Attach the interrupt
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    // Create the task to handle button presses
    xTaskCreate(buttonTask, "Button Task", 2048, NULL, 1, NULL);
}

void loop() {}