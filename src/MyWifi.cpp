#include <MyWifi.hpp>
#include <Event.hpp>
#include <Coordinator.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <painlessMesh.h>

MyWifi::MyWifi(Coordinator *coordinator) : coordinator{coordinator} {}

void MyWifi::setup() {
    mesh.onReceive([this](uint32_t from, const String &msg) {
        Event e;
        e.type = EventType::WifiConnected;
        strlcpy(e.wifiMessage, msg.c_str(), sizeof(e.wifiMessage));
        xQueueSend(coordinator->eventQueue, &e, portMAX_DELAY);
    });
}
