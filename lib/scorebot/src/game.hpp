#ifndef GAME_H
#define GAME_H

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

struct ButtonPress {
  uint32_t from;
  char message[128];
};

void seesawTask(void *pvParameters) {
  for (;;) {
    // Wait for the notification from the ISR
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    Serial.println("seesawTask");
  }
}

class Game {
    QueueHandle_t pressed;

    public:
        Game()
        : pressed{xQueueCreate(10, sizeof(ButtonPress))} {}
};

#endif
