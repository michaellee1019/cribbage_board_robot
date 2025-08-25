#ifndef UTILS_H
#define UTILS_H

#include <Wire.h>
#include <Arduino.h>

template <typename... Args>
String strFormat(const char* const format, Args... args) {
    char buffer[10];
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
      Serial.println("Scanning for I2C devices...");
      for (byte address = 1; address < 127; address++) {
          Wire.beginTransmission(address);
          byte error = Wire.endTransmission();
          
          if (error == 0) {
              Serial.printf("I2C device found at address 0x%02X (decimal: %d)\n", address, address);
          } else if (error == 4) {
              Serial.printf("Unknown error at address 0x%02X\n", address);
          }
      }
      Serial.println("I2C scan complete.");
}

#endif
