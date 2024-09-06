#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "Arduino.h"

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

[[noreturn]]
void toggleLED(void*){
    for(;;){ // infinite loop
        // Turn the LED on
        digitalWrite(LED_BUILTIN, digitalRead(D1) == HIGH ? HIGH : LOW);
        digitalWrite(D0, digitalRead(D1) == HIGH ? HIGH : LOW);
        // Pause the task for 500ms
//        vTaskDelay(500 / portTICK_PERIOD_MS);
//        // Turn the LED off
//        digitalWrite(LED_BUILTIN, LOW);
//        // Pause the task again for 500ms
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}


void setup(){
    Serial.begin(115200);
    while(!Serial){
        // wait for serial port to connect
    }

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(D1, INPUT); // Button
    pinMode(D0, OUTPUT); // LED

    xTaskCreate(
            toggleLED,    // Function that should be called
            "Toggle LED",   // Name of the task (for debugging)
            1000,            // Stack size (bytes)
            nullptr,            // Parameter to pass
            1,               // Task priority
            nullptr             // Task handle
    );

}

void loop(){
}
