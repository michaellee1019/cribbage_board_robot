#include <cstdio>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "Arduino.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#ifdef ROLE_SENDER
#define ROLE "SENDER"
#endif

#ifdef ROLE_RECEIVER
#define ROLE "RECEIVER"
#endif

// https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/

void readMacAddress(){
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    } else {
        Serial.println("Failed to read MAC address");
    }
}

#define LED_PIN D0       // Define the pin for the LED
#define BUTTON_PIN D1    // Define the pin for the button

QueueHandle_t buttonQueue;  // Queue to communicate button press/release states

void IRAM_ATTR buttonISR() {
    // ISR is triggered on button press or release (change)
    bool buttonState = digitalRead(BUTTON_PIN);
    xQueueSendFromISR(buttonQueue, &buttonState, NULL);  // Send button state to the queue
}

void LEDTask(void *pvParameters) {
    bool buttonState;

    while (1) {
        // Wait for the button state to be sent from the ISR
        Serial.println("Check xQueueReceive'd");
        if (xQueueReceive(buttonQueue, &buttonState, portMAX_DELAY)) {
            digitalWrite(LED_PIN, buttonState ? HIGH : LOW);  // Set LED state
            digitalWrite(LED_BUILTIN, buttonState ? HIGH : LOW);  // Set LED state
        }
        Serial.print("End xQueueReceive'd. buttonState=");
        Serial.println(buttonState);
    }
}

void serialSetup() {
    Serial.begin(115200);
    Serial.println("Serial receiverSetup begin.");
    while (!Serial) {
        // wait for serial port to connect
    }
    Serial.println("Serial receiverSetup");
}

void buttonSetup() {
    // Initialize the LED and button pins
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    // Create a FreeRTOS queue with space for 10 boolean entries
    buttonQueue = xQueueCreate(10, sizeof(bool));

    // Attach the ISR to the button pin
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);

    // Create the LED task
    xTaskCreate(LEDTask, "LEDTask", 1024, NULL, 1, NULL);
    Serial.println("xTaskCreate'd LEDTask");
}


// REPLACE WITH YOUR RECEIVER MAC Address
/*
     // https://apple.stackexchange.com/a/467403
     ioreg -r -c IOUSBHostDevice -x -l | perl -ne 'BEGIN {print "USB Serial Number,idProduct,idVendor,IOCalloutDevice\n"} /"USB Serial Number" = "(.+)"/ && ($sn=$1); /"idProduct" = (.+)/ && ($ip=$1); /"idVendor" = (.+)/ && ($iv=$1); /"IOCalloutDevice" = "(.+)"/ && print "$sn,$ip,$iv,$1\n"'
*/
// /dev/cu.usbmodem14401
uint8_t senderAddress[] =   {0x30, 0x30, 0xF9, 0x33, 0xE9, 0x78};

// /dev/cu.usbmodem14601
uint8_t receiverAddress[] = {0x30, 0x30, 0xF9, 0x33, 0xEA, 0x20};


// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

// Create a struct_message called toSend
struct_message toSend;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("OnSend:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void senderWifiSetup() {
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }
}

void senderWifiLoop() {
    // Set values to send
    strcpy(toSend.a, "THIS IS A CHAR");
    toSend.b = random(1, 20);
    toSend.c = 1.2;
    toSend.d = false;

    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &toSend, sizeof(toSend));

    if (result == ESP_OK) {
        Serial.println((std::string{"Sent with success. b="} + std::to_string(toSend.b)).c_str());
    }
    else {
        Serial.println("Error sending the data");
    }
    delay(2000);
}


// Create a struct_message called receivedData
struct_message receivedData;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&receivedData, incomingData, sizeof(receivedData));
    Serial.print("Bytes received: ");
    Serial.println(len);
    Serial.print("Char: ");
    Serial.println(receivedData.a);
    Serial.print("Int: b=");
    Serial.println(receivedData.b);
    Serial.print("Float: ");
    Serial.println(receivedData.c);
    Serial.print("Bool: ");
    Serial.println(receivedData.d);
    Serial.println();
}

void receiverWifiSetup() {
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void setup() {
    serialSetup();
    Serial.println((ROLE " Starting Setup " + std::to_string(xTaskGetTickCount())).c_str());

//#ifdef ROLE_SENDER
//    senderWifiSetup();
//#endif
//#ifdef ROLE_RECEIVER
//    receiverWifiSetup();
//#endif

//    buttonSetup();
    Serial.println((ROLE " Finished Setup " + std::to_string(xTaskGetTickCount())).c_str());
}



void loop() {
    // No need to do anything in the loop since the task and ISR are handling everything
//    readMacAddress();
    Serial.println((ROLE " @ Tick" + std::to_string(xTaskGetTickCount())).c_str());
    delay(1000);
//#ifdef ROLE_SENDER
//    senderWifiLoop();
//#endif
}
