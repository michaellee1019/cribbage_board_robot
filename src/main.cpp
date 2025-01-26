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




#include <WiFi.h>
#include <painlessMesh.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// #include <Message.hpp>

#define LED_PIN         33
#define BUTTON_PIN      27

class RTButton {
public:
    RTButton(int pin, int debounceDelay, int doubleClickThreshold)
        : buttonPin(pin), debounceDelayMs(debounceDelay), doubleClickThresholdMs(doubleClickThreshold), lastInterruptTime(0) {
        buttonSemaphore = xSemaphoreCreateBinary();

        debounceTimer = xTimerCreate(
            "Debounce Timer",
            pdMS_TO_TICKS(debounceDelayMs),
            pdFALSE, // One-shot timer
            this,
            [](TimerHandle_t xTimer) {
                auto* button = static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                if (button) button->debounceCallback();
            }
        );

        doubleClickTimer = xTimerCreate(
            "Double-Click Timer",
            pdMS_TO_TICKS(doubleClickThresholdMs),
            pdFALSE, // One-shot timer
            this,
            [](TimerHandle_t xTimer) {
                auto* button = static_cast<RTButton*>(pvTimerGetTimerID(xTimer));
                if (button) button->doubleClickCallback();
            }
        );

        if (debounceTimer == nullptr || doubleClickTimer == nullptr) {
            Serial.println("Failed to create timers");
            // TODO: have a failure mode
        }
    }


    virtual ~RTButton() {
        if (debounceTimer != nullptr) {
            xTimerDelete(debounceTimer, portMAX_DELAY);
        }
        if (doubleClickTimer != nullptr) {
            xTimerDelete(doubleClickTimer, portMAX_DELAY);
        }
        if (buttonSemaphore != nullptr) {
            vSemaphoreDelete(buttonSemaphore);
        }
    }

    void init() {
        pinMode(buttonPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(buttonPin), ISRHandler, this, CHANGE);

        BaseType_t taskCreated = xTaskCreate(
            [](void* param) {
                auto* button = static_cast<RTButton*>(param);
                if (button) button->buttonTask();
            },
            "Button Task", 2048, this, 1, nullptr);

        if (taskCreated != pdPASS) {
            Serial.println("Failed to create button task");
            // TODO: handle failure modes
        }
    }

protected:
    virtual void onPress() {
        Serial.println("Button Pressed");
    }

    virtual void onRelease() {
        Serial.println("Button Released");
    }

    virtual void onSingleClick() {
        Serial.println("Single Click Detected");
    }

    virtual void onDoubleClick() {
        Serial.println("Double Click Detected");
        // TODO: send
    }

private:
    const unsigned short buttonPin;
    const unsigned short debounceDelayMs;
    const unsigned short doubleClickThresholdMs;

    SemaphoreHandle_t buttonSemaphore;
    TimerHandle_t debounceTimer;
    TimerHandle_t doubleClickTimer;

    volatile bool buttonPressed = false;
    volatile bool isDoubleClick = false;
    volatile size_t clickCount = 0;
    volatile TickType_t lastInterruptTime = 0;

    static void IRAM_ATTR ISRHandler(void* arg) {
        auto* button = static_cast<RTButton*>(arg);
        if (button) button->handleInterrupt();
    }

    void handleInterrupt() {
        TickType_t interruptTime = xTaskGetTickCountFromISR();

        if ((interruptTime - lastInterruptTime) * portTICK_PERIOD_MS > debounceDelayMs) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xTimerResetFromISR(debounceTimer, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        lastInterruptTime = interruptTime;
    }

    void debounceCallback() {
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            xSemaphoreGive(buttonSemaphore);
        } else {
            buttonPressed = false;
            xSemaphoreGive(buttonSemaphore);
        }
    }

    void doubleClickCallback() {
        if (clickCount == 1) {
            onSingleClick();
        } else if (clickCount == 2) {
            isDoubleClick = true;
            onDoubleClick();
        }
        clickCount = 0;
    }

    [[noreturn]]
    void buttonTask() {
        while (true) {
            if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
                if (buttonPressed) {
                    onPress();
                    clickCount++;
                    xTimerStart(doubleClickTimer, 0);
                } else {
                    onRelease();
                }
            }
        }
    }
};

// RTButton button(BUTTON_PIN, 50, 500);


#define MESH_PREFIX     "mesh_network"
#define MESH_PASSWORD   "mesh_password"
#define MESH_PORT       5555

painlessMesh mesh;

// “Global” turn data
// For simplicity, we’ll keep track of the turn number
// and which node has the current turn in a broadcast structure.
int  turnNumber = 0;       // e.g., how many times the button has been pressed
uint32_t currentTurnNode = 0;  // nodeId of the device whose turn it currently is

// We’ll track the order of node IDs discovered in a vector,
// and we’ll figure out “who’s next” by index in this list.
std::vector<uint32_t> knownNodes;

/*************************************************************
   Forward Declarations
*************************************************************/
void sendTurnUpdate();
void setDisplay(int number);
bool isOurTurn();
void nextTurn();

/*************************************************************
   FreeRTOS Tasks
*************************************************************/
// Task to periodically check button state.
void buttonTask(void *parameter) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  bool lastButtonState = HIGH;
  while (true) {
    bool buttonState = digitalRead(BUTTON_PIN);

    // Simple falling-edge check:
    if (lastButtonState == HIGH && buttonState == LOW) {
      // If it’s our turn, pressing the button increments the turn
      // and passes to next node
        Serial.println("Button Pressed");
      if (isOurTurn()) {
        turnNumber++;
        nextTurn();
        sendTurnUpdate();
      }
    }
    lastButtonState = buttonState;

    // Debounce-ish delay
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// Task to update the LED/display based on the shared turn data.
void updateTask(void *parameter) {
  pinMode(LED_PIN, OUTPUT);

  while (true) {
    // LED on if it’s our turn, off otherwise
    if (isOurTurn()) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    // Update the numeric display with the current turn count
    setDisplay(turnNumber);

    // Let’s not hammer the CPU
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/*************************************************************
   Mesh Callbacks
*************************************************************/

// Called whenever a new node connects
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Mesh] New Connection, nodeId = %u\n", nodeId);

  // Keep track of known nodes
  // (avoid duplicates; just for demonstration)
  bool found = false;
  for (auto n : knownNodes) {
    if (n == nodeId) {
      found = true;
      break;
    }
  }
  if (!found) {
    knownNodes.push_back(nodeId);
  }

  // Optionally broadcast the current state so the new node
  // can sync immediately
  sendTurnUpdate();
}

// Called whenever a node goes offline
void lostConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Mesh] Lost Connection, nodeId = %u\n", nodeId);

  // Remove from the knownNodes list
  for (auto it = knownNodes.begin(); it != knownNodes.end(); ) {
    if (*it == nodeId) {
      it = knownNodes.erase(it);
    } else {
      ++it;
    }
  }
}

// Called when a message arrives
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[Mesh] Received from %u: %s\n", from, msg.c_str());
  // For simplicity, let’s assume the message is formatted like: "TURN:<turnNumber>:<currentTurnNode>"
  // We’ll parse it out.
  // In real code, you might want JSON or a more robust approach.

  if (msg.startsWith("TURN:")) {
    int firstColon = msg.indexOf(':', 5);
    int secondColon = msg.indexOf(':', firstColon + 1);
    if (firstColon > 0 && secondColon > 0) {
      String turnStr = msg.substring(5, firstColon);
      String nodeStr = msg.substring(firstColon + 1, secondColon);

      turnNumber = turnStr.toInt();
      currentTurnNode = (uint32_t) nodeStr.toInt();
    }
  }
}

/*************************************************************
   Helper Functions
*************************************************************/

bool isOurTurn() {
  // If the currentTurnNode matches our nodeId, it’s our turn
  return (currentTurnNode == mesh.getNodeId());
}

void nextTurn() {
  // We have a vector of knownNodes plus ourselves. Let’s unify them:
  // If the list doesn’t contain ourselves, we add it.
  bool haveSelf = false;
  for (auto n : knownNodes) {
    if (n == mesh.getNodeId()) {
      haveSelf = true;
      break;
    }
  }
  if (!haveSelf) {
    knownNodes.push_back(mesh.getNodeId());
  }

  // Make sure the node vector is sorted, so that the "turn order" is consistent
  // Or some other stable ordering
  std::sort(knownNodes.begin(), knownNodes.end());

  // Find our position in the sorted list
  int idx = -1;
  for (int i = 0; i < (int)knownNodes.size(); i++) {
    if (knownNodes[i] == mesh.getNodeId()) {
      idx = i;
      break;
    }
  }
  if (idx >= 0) {
    // Next index
    int nextIdx = (idx + 1) % knownNodes.size();
    currentTurnNode = knownNodes[nextIdx];
  }
}

// Broadcast the current turn data to the entire mesh
void sendTurnUpdate() {
  // Example format: "TURN:<turnNumber>:<currentTurnNode>:"
  String msg = "TURN:" + String(turnNumber) + ":" + String(currentTurnNode) + ":";
  mesh.sendBroadcast(msg);
}

int lastDisplay = -1;
void setDisplay(int number) {
  // Stub for your actual display library code, e.g.:
  // display.setNumber(number);
  // or something similar
    if (number != lastDisplay) {
        Serial.printf("[Display] %d\n", number);
        lastDisplay = number;
    }

}

/*************************************************************
   setup() and loop()
*************************************************************/
void setupOld() {
  Serial.begin(115200);
  delay(1000);

  // Initialize the mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);  // set before init()
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&lostConnectionCallback);
  mesh.onReceive(&receivedCallback);

  // For demonstration, we’ll say the first device to power up
  // decides it’s the first turn holder:
  currentTurnNode = mesh.getNodeId();

  // Create FreeRTOS tasks
  xTaskCreate(buttonTask, "ButtonTask", 2048, NULL, 1, NULL);
  xTaskCreate(updateTask, "UpdateTask", 2048, NULL, 1, NULL);
}

void loopOld() {
  // Let painlessMesh handle its background tasks
  mesh.update();

  // Other background tasks or logic can go here if needed
}


