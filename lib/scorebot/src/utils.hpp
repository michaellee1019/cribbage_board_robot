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



#endif
