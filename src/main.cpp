#include <cstdio>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "Arduino.h"

#include <WiFi.h>
#include <esp_wifi.h>


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
    Serial.println("Serial setup begin.");
    while (!Serial) {
        // wait for serial port to connect
    }
    Serial.println("Serial setup");
}

void wifiSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.begin();
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

void setup() {
    serialSetup();
    wifiSetup();
    buttonSetup();
}

void loop() {
    // No need to do anything in the loop since the task and ISR are handling everything
    Serial.println(("Loop still running at tick " + std::to_string(xTaskGetTickCount())).c_str());
    delay(1000);
    readMacAddress();
}
