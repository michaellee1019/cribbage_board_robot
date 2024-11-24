#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

// Counter to send
volatile int counter = 0;

// Task Handles
TaskHandle_t sendTaskHandle;

// Callback for received data
void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.printf("Received message from %s: %d\n", macStr, *(int *)data);
}

// Function to send broadcast messages
void sendBroadcast(void *param) {
    for (;;) {
        counter++;
        esp_now_send(NULL, (uint8_t *)&counter, sizeof(counter)); // NULL sends to all peers
        Serial.printf("Sent counter: %d\n", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Send every 1 second
    }
}


// pio run -t monitor -e sender

void setup() {
    Serial.begin(115200);
    sleep(3);
    Serial.println("Hello. Starting wifi.");

    // Initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    if (esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        Serial.println("Failed to set channel");
    }

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv); // Register receive callback

    // Register broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memset(&peerInfo, 0, sizeof(peerInfo));
    peerInfo.channel = 1;  // Same channel as WiFi
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    // Create FreeRTOS task for sending messages
    xTaskCreatePinnedToCore(sendBroadcast, "SendBroadcast", 2048, NULL, 1, &sendTaskHandle, 1);
}

void loop() {
    // Nothing to do in loop; tasks run in the background
}