#include <cstdio>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "Arduino.h"


// https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/

/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/get-change-esp32-esp8266-mac-address-arduino/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <WiFi.h>
#include <esp_wifi.h>

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


// https://tutoduino.fr/en/discover-freertos-on-an-esp32-with-platformio/
const char *model_info(esp_chip_model_t model)
{
    switch (model)
    {
        case CHIP_ESP32:
            return "ESP32";
        case CHIP_ESP32S2:
            return "ESP32S2";
        case CHIP_ESP32S3:
            return "ESP32S3";
        case CHIP_ESP32C3:
            return "ESP32C3";
        case CHIP_ESP32H2:
            return "ESP32H2";
        default:
            return "Unknown";
    }
}
void print_chip_info()
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("Chip model %s with %d CPU core(s), WiFi%s%s, ",
           model_info(chip_info.model),
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d\n", major_rev, minor_rev);
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

void setup() {
    Serial.begin(115200);
    Serial.println("Serial setup begin.");
    while (!Serial) {
        // wait for serial port to connect
    }
    Serial.println("Serial setup");

    WiFi.mode(WIFI_STA);
    WiFi.begin();

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

void loop() {
    // No need to do anything in the loop since the task and ISR are handling everything
    Serial.println(("Loop still running at tick " + std::to_string(xTaskGetTickCount())).c_str());
    delay(1000);
    readMacAddress();
}
