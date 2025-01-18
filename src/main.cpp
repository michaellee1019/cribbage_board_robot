#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// RED
uint8_t redAddress[] = {0xC8, 0x2E, 0x18, 0xF0, 0x2E, 0x6C };

// BLUE
uint8_t blueAddress[] = {0x08, 0xB6, 0x1F, 0xB8, 0xAA, 0x08 };

uint8_t LED_PIN = 33;
uint8_t BUTTON_PIN = 14;

// Counter to send
volatile int counter = 0;

// Task Handles
TaskHandle_t sendTaskHandle;

// Callback for received data
void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
    char macStr[18];
    snprintf(macStr,
             sizeof(macStr),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);
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
            redAddress,
            sizeof(counter)
        );

        Serial.printf("Status: %d", resp);
        Serial.printf(" Sent counter: %d\n", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Send every 1 second
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
    memcpy(peerInfo.peer_addr, blueAddress, 6);
#elif (COLOR==2)
    memcpy(peerInfo.peer_addr, redAddress, 6);
#endif

    // if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
    }
    //}

    // Create FreeRTOS task for sending messages
    xTaskCreatePinnedToCore(sendBroadcast, "SendBroadcast", 2048, nullptr, 1, &sendTaskHandle, 1);
}

void loop() {
    if(digitalRead(BUTTON_PIN)==LOW) {
        Serial.println("BUTTON PRESSED");
    }
}