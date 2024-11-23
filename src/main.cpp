#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <rom/rtc.h>
#include <HardwareSerial.h>
#include "utils.hpp"

const int MODULE_SIZE = 2;
RTC_DATA_ATTR static int rebootCount = -1;


/* function headers */
void appTask(void * parameter);
void initEspnow();


/* espnow */
static char sta_mac[18];
static char softap_mac[18];
uint8_t peerCount = 0;
uint32_t msAfterESPNowRecv = 0;
uint32_t counter = 0;

void setup() {
  /* Stack size in bytes. */ /* Parameter passed as input of the task */ /* Priority of the task. */
  // xTaskCreate(appTask, "appTask", 10000, NULL, 2, NULL);

  rebootCount++;
  Serial.begin(115200);
    while (!Serial) {
        // Serial.print("!Serial");
    }
    Serial.println("Serial.begin()!");

  print_reset_reason(rtc_get_reset_reason(0));
  verbose_print_reset_reason(rtc_get_reset_reason(0));

  WiFi.disconnect();
  initEspnow();
}

esp_now_peer_info_t slave;

void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // sentCnt++;
    Serial.println("SENT!");
};

void recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    Serial.println("RECV!");
    memcpy(&slave.peer_addr, mac_addr, 6);
    esp_err_t addStatus = esp_now_add_peer(&slave);
    if (addStatus == ESP_OK) {
        peerCount++;
        printf("\n=====================");
        printf("\nADD PEER status=0x%02x", ESP_OK);
        printf("\n=====================\n");
    }
    else {
        printf("\n=====================");
        printf("\nADD PEER [FAILED] status=0x%02x", addStatus - ESP_ERR_ESPNOW_BASE);
        printf("\n=====================\n");
    }

    uint8_t time = 1;
    esp_err_t result = esp_now_send(mac_addr, &time, 1);
    if (result == ESP_OK)   {
        Serial.println("esp_now_send [ESP_OK]");
    }
    counter++;
    msAfterESPNowRecv = millis();
}

void registerCallbacks() {
  esp_now_register_send_cb(send_cb);
  esp_now_register_recv_cb(recv_cb);
}
void initEspnow() {
  bzero(&slave, sizeof(slave));
  WiFi.mode(WIFI_AP_STA);
  delay(10);
  strcpy(sta_mac, WiFi.macAddress().c_str());
  strcpy(softap_mac, WiFi.softAPmacAddress().c_str());
  Serial.printf("STA MAC: %s\r\n", sta_mac);
  Serial.printf(" AP MAC: %s\r\n", softap_mac);
  if (esp_now_init() == ESP_OK) {
    Serial.printf("ESPNow Init Success\n");
    registerCallbacks();
  }
  else {
    Serial.printf("ESPNow Init Failed\n");
    ESP.restart();
  }
}

void loop() {
  yield();
}

void appTask(void * parameter)
{
    // rtc->setup();
    // pinMode(EXT_WDT_PIN, OUTPUT);
    while (1) {
      // rtc->loop();
      // lcd->loop();
    }
}
