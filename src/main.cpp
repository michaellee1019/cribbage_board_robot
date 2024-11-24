#include "utils.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <esp_now.h>
#include <rom/rtc.h>

const int MODULE_SIZE = 2;
RTC_DATA_ATTR static int rebootCount = -1;


/* function headers */
void appTask(void* parameter);
void initEspnow();


/* espnow */
static char sta_mac[18];
static char softap_mac[18];
uint8_t peerCount = 0;
uint32_t msAfterESPNowRecv = 0;
uint32_t counter = 0;



uint32_t counter2 = 0;
void loop() {
    Serial.printf("loop %i", counter2++);
    sleep(1);
}

void setup() {
    /* Stack size in bytes. */ /* Parameter passed as input of the task */ /* Priority of the task.
                                                                            */
    // xTaskCreate(appTask, "appTask", 10000, NULL, 2, NULL);

    rebootCount++;
    Serial.begin(115200);
    while (!Serial) {
        // Serial.print("!Serial");
    }
    sleep(3);
    Serial.println("Serial.begin()!");

    print_reset_reason(rtc_get_reset_reason(0));
    verbose_print_reset_reason(rtc_get_reset_reason(0));

    WiFi.disconnect();
    initEspnow();
}

esp_now_peer_info_t slave;

void send_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // sentCnt++;
    Serial.println("SENT!");
};

// TODO: cron task to advertise own IP and current stat]]e

// Setup and find own number.
// Each number knows which goes before it.
// Red: memory="after blue" (turn nr % players, who last went, sth)
// Red: Broadcasts to everyone its current turn number and ~~which player nr just went~~ own player
// nr Blue: Sees that Red broadcasted [turn_numberRed=0, red]
//

// my player number = my mac address
// first device to turn on sees nobody else so declares self red
// second device

/*
I know my mac address, and the code knows which mac address is which color
    blink like hell if my mac isn't found in the known

Every half second, send heartbeat. Includes turn number and state.
If I see a heartbeat with a higher turn number, I trust their external state.

I know it's my turn, and I keep track of the turn number
Upon press I send everyone my state
*/

void recv_cb(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
    Serial.println("RECV!");
    memcpy(&slave.peer_addr, mac_addr, 6);
    esp_err_t addStatus = esp_now_add_peer(&slave);
    if (addStatus == ESP_OK) {
        peerCount++;
        printf("\n=====================");
        printf("\nADD PEER status=0x%02x", ESP_OK);
        printf("\n=====================\n");
    } else {
        printf("\n=====================");
        printf("\nADD PEER [FAILED] status=0x%02x", addStatus - ESP_ERR_ESPNOW_BASE);
        printf("\n=====================\n");
    }

    uint8_t time = 1;
    esp_err_t result = esp_now_send(mac_addr, &time, 1);
    if (result == ESP_OK) {
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
    } else {
        Serial.printf("ESPNow Init Failed\n");
        ESP.restart();
    }
}



void appTask(void* parameter) {
    // rtc->setup();
    // pinMode(EXT_WDT_PIN, OUTPUT);
    while (1) {
        yield();
        // rtc->loop();
        // lcd->loop();
    }
}


////////////////////////////////////////////
//
//
// #include <Arduino.h>
//
// // define two tasks for Blink & AnalogRead
// void TaskBlink( void *pvParameters );
// void TaskAnalogRead( void *pvParameters );
//
// // the setup function runs once when you press reset or power the board
// void setupBlink() {
//
//   // initialize serial communication at 9600 bits per second:
//   Serial.begin(115200);
//
//   while (!Serial) {
//     ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and
//     other 32u4 based boards.
//   }
//   Serial.println("setup");
//
//   // Now set up two tasks to run independently.
//   xTaskCreatePinnedToCore(
//     TaskBlink
//     ,  "Blink"   // A name just for humans
//     ,  2048  // This stack size can be checked & adjusted by reading the Stack Highwater
//     ,  NULL
//     ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the
//     lowest. ,  NULL , 1); // You don't even have a core
//
//   xTaskCreatePinnedToCore(
//     TaskAnalogRead
//     ,  "AnalogRead"
//     ,  2048  // Stack size
//     ,  NULL
//     ,  1  // Priority
//     ,  NULL
//     , 1); // You don't even have a core
//
//     Serial.println("tasks started");
//
//   // Now the task scheduler, which takes over control of scheduling individual tasks, is
//   automatically started.
// }
//
// void loop(){};
//
// void loopButtonDetector()
// {
//   // Empty. Things are done in Tasks.
//     int v = digitalRead(4);
//     int x = digitalRead(3);
//     Serial.printf("4=[%i], 3=[%i]", v, x);
//     sleep(1);
// }
//
// /*--------------------------------------------------*/
// /*---------------------- Tasks ---------------------*/
// /*--------------------------------------------------*/
//
// void TaskBlink(void *pvParameters)  // This is a task.
// {
//   (void) pvParameters;
//
// /*
//   Blink
//   Turns on an LED on for one second, then off for one second, repeatedly.
//
//   Most Arduinos have an on-board LED you can control. On the UNO, LEONARDO, MEGA, and ZERO
//   it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN takes care
//   of use the correct LED pin whatever is the board used.
//
//   The MICRO does not have a LED_BUILTIN available. For the MICRO board please substitute
//   the LED_BUILTIN definition with either LED_BUILTIN_RX or LED_BUILTIN_TX.
//   e.g. pinMode(LED_BUILTIN_RX, OUTPUT); etc.
//
//   If you want to know what pin the on-board LED is connected to on your Arduino model, check
//   the Technical Specs of your board  at https://www.arduino.cc/en/Main/Products
//
//   This example code is in the public domain.
//
//   modified 8 May 2014
//   by Scott Fitzgerald
//
//   modified 2 Sep 2016
//   by Arturo Guadalupi
// */
//
//   // initialize digital LED_BUILTIN on pin 13 as an output.
//   pinMode(LED_BUILTIN, OUTPUT);
//     pinMode(4, INPUT_PULLUP); // Back
//     pinMode(3, INPUT_PULLUP); // Next
//
//     pinMode(1, OUTPUT); // LED Top
//     pinMode(2, OUTPUT); // LED bottom
//
//   for (;;) // A Task shall never return or exit.
//   {
//     digitalWrite(1, HIGH);   // turn the LED on (HIGH is the voltage level)
//       digitalWrite(2, LOW);
//       Serial.println("1HI 2LO");
//     vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
//       digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
//       digitalWrite(1, LOW);
//       Serial.println("1LO 2HI");
//     vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
//   }
// }
//
// void TaskAnalogRead(void *pvParameters)  // This is a task.
// {
//   (void) pvParameters;
//
// /*
//   AnalogReadSerial
//   Reads an analog input on pin 0, prints the result to the serial monitor.
//   Graphical representation is available using serial plotter (Tools > Serial Plotter menu)
//   Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.
//
//   This example code is in the public domain.
// */
//
//   for (;;)
//   {
//     // read the input on analog pin 0:
//     // int sensorValue = analogRead(A0);
//     // print out the value you read:
//     // Serial.println(sensorValue);
//     vTaskDelay(1);  // one tick delay (15ms) in between reads for stability
//   }
// }
//
