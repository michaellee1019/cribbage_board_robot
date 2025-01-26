#include "SparkFun_Alphanumeric_Display.h"
#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

// define two tasks for Blink & AnalogRead
void TaskBlink(void* pvParameters);
void TaskAnalogRead(void* pvParameters);

HT16K33 display;
Adafruit_MCP23X17 mcp;
u32_t okPin = 4;
u32_t plusone = 3;
u32_t plusfive = 2;
u32_t negone = 1;
u32_t add = 0;

void Scanner() {
    Serial.println();
    Serial.println("I2C scanner. Scanning ...");
    byte count = 0;

    // Wire.begin();
    for (byte i = 8; i < 120; i++) {
        Wire.beginTransmission(i);        // Begin I2C transmission Address (i)
        if (Wire.endTransmission() == 0)  // Receive 0 = success (ACK response)
        {
            Serial.print("Found address: ");
            Serial.print(i, DEC);
            Serial.print(" (0x");
            Serial.print(i, HEX);  // PCF8574 7 bit address
            Serial.println(")");
            count++;
        }
    }
    Serial.print("Found ");
    Serial.print(count, DEC);  // numbers of devices
    Serial.println(" device(s).");
}

class RotaryEncoder {
    #define SS_SWITCH 24
    #define SS_NEOPIX 6
    #define SEESAW_ADDR 0x36

    Adafruit_seesaw ss;
    seesaw_NeoPixel sspixel{1, SS_NEOPIX, NEO_GRB + NEO_KHZ800};

    public:
        explicit RotaryEncoder() {}
        void setup() {
            ss.begin(SEESAW_ADDR);
            sspixel.begin(SEESAW_ADDR);
            sspixel.setBrightness(20);
            sspixel.setPixelColor(0, 0xFF0000);
            sspixel.show();

            ss.pinMode(SS_SWITCH, INPUT_PULLUP);
        }

        void loop() {
            Serial.println("Encoder position: " + String(ss.getEncoderPosition()));
            
            if (!ss.digitalRead(SS_SWITCH)) {
                Serial.println("Switch pressed");
            }
        }
};

RotaryEncoder encoder;

// the setup function runs once when you press reset or power the board
void setup() {
    // initialize serial communication at 9600 bits per second:
    Serial.begin(115200);
    Serial.println("I just began.");

    encoder.setup();

    // Don't do this unless you really need to, won't work unless connected to serial monitor
    while (!Serial) {
        // ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and
        // other 32u4 based boards.
    }
    delay(1000);
    Wire.begin(5, 6);

    Scanner();

    if (!mcp.begin_I2C(0x20, &Wire)) {
        Serial.println("Error initializing MCP.");
    } else {
        Serial.println("MCP Found!");
    }

    mcp.pinMode(okPin, INPUT_PULLUP);
    mcp.pinMode(plusone, INPUT_PULLUP);
    mcp.pinMode(plusfive, INPUT_PULLUP);
    mcp.pinMode(negone, INPUT_PULLUP);
    mcp.pinMode(add, INPUT_PULLUP);

    while (display.begin() == false) {
        Serial.println("Device did not acknowledge!");
        Scanner();
    }
    Serial.println("Display acknowledged.");

    display.print("BEEF");

    // Now set up two tasks to run independently.
    // xTaskCreate(
    //   TaskBlink
    //   ,  "Blink"   // A name just for humans
    //   ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    //   ,  NULL
    //   ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the
    //   lowest. ,  NULL );

    // xTaskCreate(
    //   TaskAnalogRead
    //   ,  "AnalogRead"
    //   ,  128  // Stack size
    //   ,  NULL
    //   ,  1  // Priority
    //   ,  NULL );

    // Now the task scheduler, which takes over control of scheduling individual tasks, is
    // automatically started.
}

bool buttonPressed = false;

void loop() {
    if (!mcp.digitalRead(okPin)) {
        Serial.println("Button OK Pressed!");
        display.print("OK");
        delay(100);
    }
    if (!mcp.digitalRead(plusone)) {
        Serial.println("Button +1 Pressed!");
        display.print("+1");
        delay(100);
    }

    if (!mcp.digitalRead(plusfive)) {
        Serial.println("Button +5 Pressed!");
        display.print("+5");
        delay(100);
    }
    if (!mcp.digitalRead(negone)) {
        Serial.println("Button -1 Pressed!");
        display.print("-1");
        delay(100);
    }
    if (!mcp.digitalRead(add)) {
        Serial.println("Button ADD Pressed!");
        display.print("ADD");
        delay(100);
    }
    encoder.loop();
    // for (uint8_t i = '!'; i <= 'z'; i++);
    //     //Serial.println(buttonPressed);
    //     display.clear();
    //     display.printChar(i, 0);
    //     display.printChar(i + 1, 1);
    //     display.printChar(i + 2, 2);
    //     display.printChar(i + 3, 3);
    //     display.updateDisplay();
    //     delay(100);
    // }
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskBlink(void* pvParameters)  // This is a task.
{
    (void)pvParameters;

    /*
      Blink
      Turns on an LED on for one second, then off for one second, repeatedly.

      Most Arduinos have an on-board LED you can control. On the UNO, LEONARDO, MEGA, and ZERO
      it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN takes care
      of use the correct LED pin whatever is the board used.

      The MICRO does not have a LED_BUILTIN available. For the MICRO board please substitute
      the LED_BUILTIN definition with either LED_BUILTIN_RX or LED_BUILTIN_TX.
      e.g. pinMode(LED_BUILTIN_RX, OUTPUT); etc.

      If you want to know what pin the on-board LED is connected to on your Arduino model, check
      the Technical Specs of your board  at https://www.arduino.cc/en/Main/Products

      This example code is in the public domain.

      modified 8 May 2014
      by Scott Fitzgerald

      modified 2 Sep 2016
      by Arturo Guadalupi
    */

    // initialize digital LED_BUILTIN on pin 13 as an output.
    pinMode(13, OUTPUT);

    for (;;)  // A Task shall never return or exit.
    {
        digitalWrite(13, HIGH);                 // turn the LED on (HIGH is the voltage level)
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // wait for one second
        digitalWrite(13, LOW);                  // turn the LED off by making the voltage LOW
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // wait for one second
    }
}

void TaskAnalogRead(void* pvParameters)  // This is a task.
{
    (void)pvParameters;

    /*
      AnalogReadSerial
      Reads an analog input on pin 0, prints the result to the serial monitor.
      Graphical representation is available using serial plotter (Tools > Serial Plotter menu)
      Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.

      This example code is in the public domain.
    */

    for (;;) {
        // read the input on analog pin 0:
        int sensorValue = analogRead(A0);
        // print out the value you read:
        Serial.println(sensorValue);
        vTaskDelay(1);  // one tick delay (15ms) in between reads for stability
    }
}
