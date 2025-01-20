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
void setup() {
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

void loop() {
  // Let painlessMesh handle its background tasks
  mesh.update();

  // Other background tasks or logic can go here if needed
}


