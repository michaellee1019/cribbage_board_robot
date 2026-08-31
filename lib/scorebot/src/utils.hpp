#ifndef UTILS_H
#define UTILS_H

#include <Wire.h>
#include <Arduino.h>

// Debug macros - compile to nothing when SCOREBOT_DEBUG=0 for maximum efficiency
#if SCOREBOT_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x) ((void)0)
    #define DEBUG_PRINTLN(x) ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#endif

template <typename... Args>
String strFormat(const char* const format, Args... args) {
    // Scores and diagnostic values may include a sign and a 32-bit magnitude.
    // Keep this bounded, but large enough for every formatted value we display.
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), format, args...);
    return {buffer};
}

inline int numI2C() {
  byte count = 0;

  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);        // Begin I2C transmission Address (i)
    if (Wire.endTransmission() == 0)  // Receive 0 = success (ACK response)
    {
      count++;
    }
  }
  return count;
}

inline void printI2CDevices() {
      // Scan for I2C devices
      DEBUG_PRINTLN("Scanning for I2C devices...");
      for (byte address = 1; address < 127; address++) {
          Wire.beginTransmission(address);
          byte error = Wire.endTransmission();
          
          if (error == 0) {
              DEBUG_PRINTF("I2C device found at address 0x%02X (decimal: %d)\n", address, address);
          } else if (error == 4) {
              DEBUG_PRINTF("Unknown error at address 0x%02X\n", address);
          }
      }
      DEBUG_PRINTLN("I2C scan complete.");
}

#endif
