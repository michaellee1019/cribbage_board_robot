#pragma once

#ifdef NATIVE_BUILD
    // Native/desktop testing environment
    #include <iostream>
    #include <cstdlib>
    #define ESP_LOGE(tag, format, ...) printf("[ERROR][%s] " format "\n", tag, ##__VA_ARGS__)
    #define esp_restart() exit(1)
    #define delay(ms) // No-op for testing
    typedef int BaseType_t;
    #define pdPASS 1
    #define pdFAIL 0
#else
    // ESP32 embedded environment
    #include <Arduino.h>
    #include <esp_system.h>
    #include <esp_log.h>
#endif