/**
 * Complete example of using Adafruit_seesaw rotary encoder with hardware interrupts
 * and FreeRTOS. Demonstrates proper ISR handling and button state clearing.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Seesaw encoder module I2C address
#define SEESAW_ADDR 0x36

// Seesaw encoder registers
#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SS_ENCODER 0

// ESP32 interrupt pin (connect seesaw INT pin to this GPIO)
#define INTERRUPT_PIN 4  // Adjust based on your hardware

// FreeRTOS configuration
#define ENCODER_TASK_STACK_SIZE 4096
#define ENCODER_TASK_PRIORITY 5
#define ISR_TASK_PRIORITY 6
#define EVENT_QUEUE_SIZE 20

// Debounce timing
#define BUTTON_DEBOUNCE_MS 50
#define ENCODER_DEBOUNCE_US 1000  // Microseconds for encoder debounce

// Event types
enum EventType {
    ENCODER_ROTATED_CW,
    ENCODER_ROTATED_CCW,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESS,
    INTERRUPT_TRIGGERED
};

struct EncoderEvent {
    EventType type;
    int32_t value;
    uint32_t timestamp;
};

class RotaryEncoderHandler {
private:
    Adafruit_seesaw seesaw;
    seesaw_NeoPixel pixel;
    
    // FreeRTOS handles
    TaskHandle_t processingTaskHandle;
    TaskHandle_t isrTaskHandle;
    QueueHandle_t eventQueue;
    QueueHandle_t isrQueue;  // High priority queue for ISR events
    SemaphoreHandle_t i2cMutex;
    SemaphoreHandle_t isrSemaphore;
    
    // Encoder state (volatile for ISR access)
    volatile int32_t encoderPosition;
    volatile int32_t lastEncoderPosition;
    volatile uint32_t lastEncoderISRTime;
    
    // Button state (volatile for ISR access)
    volatile bool buttonState;
    volatile bool lastButtonState;
    volatile uint32_t buttonPressStartTime;
    volatile uint32_t lastButtonISRTime;
    
    // Configuration
    bool invertEncoder;
    uint32_t longPressThresholdMs;
    
    // ISR flag
    volatile bool interruptPending;
    
    // Static instance for ISR access
    static RotaryEncoderHandler* instance;
    
public:
    RotaryEncoderHandler() 
        : pixel(SS_NEOPIX, 1, NEO_GRB + NEO_KHZ800),
          encoderPosition(0),
          lastEncoderPosition(0),
          lastEncoderISRTime(0),
          buttonState(true),  // Button is active LOW
          lastButtonState(true),
          buttonPressStartTime(0),
          lastButtonISRTime(0),
          invertEncoder(false),
          longPressThresholdMs(1000),
          interruptPending(false) {
        instance = this;
    }
    
    bool begin(uint8_t addr = SEESAW_ADDR) {
        // Create FreeRTOS resources
        eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(EncoderEvent));
        if (!eventQueue) {
            Serial.println("Failed to create event queue");
            return false;
        }
        
        isrQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(EncoderEvent));
        if (!isrQueue) {
            Serial.println("Failed to create ISR queue");
            return false;
        }
        
        i2cMutex = xSemaphoreCreateMutex();
        if (!i2cMutex) {
            Serial.println("Failed to create I2C mutex");
            return false;
        }
        
        isrSemaphore = xSemaphoreCreateBinary();
        if (!isrSemaphore) {
            Serial.println("Failed to create ISR semaphore");
            return false;
        }
        
        // Initialize I2C and seesaw
        Wire.begin();
        
        if (!seesaw.begin(addr)) {
            Serial.println("Couldn't find seesaw on I2C");
            return false;
        }
        
        uint32_t version = ((seesaw.read8(0x00, 0x02) << 16) | 
                           (seesaw.read8(0x00, 0x03) << 8) | 
                           seesaw.read8(0x00, 0x04));
        Serial.printf("Seesaw version: 0x%06X\n", version);
        
        // Configure hardware and interrupts
        configureHardware();
        
        // Start the ISR handler task (high priority)
        xTaskCreatePinnedToCore(
            isrHandlerTaskWrapper,
            "ISRHandler",
            2048,
            this,
            ISR_TASK_PRIORITY,
            &isrTaskHandle,
            1  // Core 1
        );
        
        // Start the event processing task
        xTaskCreatePinnedToCore(
            processingTaskWrapper,
            "ProcessingTask",
            ENCODER_TASK_STACK_SIZE,
            this,
            ENCODER_TASK_PRIORITY,
            &processingTaskHandle,
            1  // Core 1
        );
        
        return true;
    }
    
    void configureHardware() {
        // Initialize NeoPixel
        if (!pixel.begin(SEESAW_ADDR, &Wire)) {
            Serial.println("Failed to initialize NeoPixel");
        }
        pixel.setBrightness(20);
        pixel.setPixelColor(0, 0x0000FF);  // Blue on startup
        pixel.show();
        
        // Configure encoder
        encoderPosition = seesaw.getEncoderPosition(SS_ENCODER);
        lastEncoderPosition = encoderPosition;
        
        // Configure button with pull-up
        seesaw.pinMode(SS_SWITCH, INPUT_PULLUP);
        buttonState = seesaw.digitalRead(SS_SWITCH);
        lastButtonState = buttonState;
        
        // Configure seesaw interrupts
        // Enable interrupts for button (both edges) and encoder changes
        uint32_t interruptMask = (1UL << SS_SWITCH);
        seesaw.setGPIOInterrupts(interruptMask, 1);
        
        // Enable encoder interrupt on the seesaw
        seesaw.enableEncoderInterrupt();
        
        // Configure ESP32 interrupt pin
        pinMode(INTERRUPT_PIN, INPUT_PULLUP);
        
        // Attach interrupt with FALLING edge (seesaw INT pin goes LOW on interrupt)
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), encoderISR, FALLING);
        
        Serial.println("Hardware configured with interrupts enabled");
    }
    
    // ISR - Must be static and IRAM_ATTR for ESP32
    static void IRAM_ATTR encoderISR() {
        if (instance) {
            instance->handleInterrupt();
        }
    }
    
    // Minimal ISR handler - just signals the handler task
    void IRAM_ATTR handleInterrupt() {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // Simple debounce check
        uint32_t now = micros();
        if (now - lastEncoderISRTime > ENCODER_DEBOUNCE_US) {
            lastEncoderISRTime = now;
            interruptPending = true;
            
            // Wake up the ISR handler task
            xSemaphoreGiveFromISR(isrSemaphore, &xHigherPriorityTaskWoken);
        }
        
        // Yield if we woke a higher priority task
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
    
    // High-priority task to handle ISR events
    static void isrHandlerTaskWrapper(void* param) {
        RotaryEncoderHandler* handler = static_cast<RotaryEncoderHandler*>(param);
        handler->isrHandlerTask();
    }
    
    void isrHandlerTask() {
        while (true) {
            // Wait for ISR signal
            if (xSemaphoreTake(isrSemaphore, portMAX_DELAY) == pdTRUE) {
                if (interruptPending) {
                    interruptPending = false;
                    
                    // Read interrupt status from seesaw
                    xSemaphoreTake(i2cMutex, portMAX_DELAY);
                    
                    // Get interrupt flags
                    uint32_t irqFlags = seesaw.getGPIOInterrupts();
                    
                    // Check encoder position
                    int32_t newPosition = seesaw.getEncoderPosition(SS_ENCODER);
                    if (invertEncoder) newPosition = -newPosition;
                    
                    // Check button state
                    bool newButtonState = seesaw.digitalRead(SS_SWITCH);
                    
                    xSemaphoreGive(i2cMutex);
                    
                    uint32_t timestamp = millis();
                    
                    // Process encoder change
                    if (newPosition != encoderPosition) {
                        int32_t delta = newPosition - encoderPosition;
                        encoderPosition = newPosition;
                        
                        EncoderEvent event;
                        event.type = (delta > 0) ? ENCODER_ROTATED_CW : ENCODER_ROTATED_CCW;
                        event.value = abs(delta);
                        event.timestamp = timestamp;
                        
                        xQueueSend(isrQueue, &event, 0);
                    }
                    
                    // Process button change
                    if (newButtonState != buttonState) {
                        buttonState = newButtonState;
                        
                        EncoderEvent event;
                        event.timestamp = timestamp;
                        
                        if (!buttonState) {  // Button pressed (LOW)
                            buttonPressStartTime = timestamp;
                            event.type = BUTTON_PRESSED;
                            event.value = 0;
                        } else {  // Button released (HIGH)
                            uint32_t pressDuration = timestamp - buttonPressStartTime;
                            event.type = (pressDuration >= longPressThresholdMs) ? 
                                        BUTTON_LONG_PRESS : BUTTON_RELEASED;
                            event.value = pressDuration;
                            
                            // Clear encoder position on button release
                            clearEncoderPositionFromISR();
                        }
                        
                        xQueueSend(isrQueue, &event, 0);
                    }
                }
            }
        }
    }
    
    void clearEncoderPositionFromISR() {
        // Schedule position clear in processing task to avoid I2C in ISR context
        EncoderEvent event;
        event.type = INTERRUPT_TRIGGERED;  // Special event to trigger position clear
        event.value = 0;
        event.timestamp = millis();
        xQueueSend(isrQueue, &event, 0);
    }
    
    void clearEncoderPosition() {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        
        // Set encoder position to 0
        seesaw.setEncoderPosition(0, SS_ENCODER);
        
        // Clear interrupt flags
        seesaw.getGPIOInterrupts();
        
        xSemaphoreGive(i2cMutex);
        
        // Update our tracking variables
        encoderPosition = 0;
        lastEncoderPosition = 0;
        
        Serial.println("Encoder position cleared");
    }
    
    // Main processing task
    static void processingTaskWrapper(void* param) {
        RotaryEncoderHandler* handler = static_cast<RotaryEncoderHandler*>(param);
        handler->processingTask();
    }
    
    void processingTask() {
        EncoderEvent event;
        
        while (true) {
            // Process ISR queue with higher priority
            while (xQueueReceive(isrQueue, &event, 0) == pdTRUE) {
                switch (event.type) {
                    case ENCODER_ROTATED_CW:
                        Serial.printf("[ISR][%lu] Encoder CW: %d steps (pos: %d)\n", 
                                    event.timestamp, event.value, encoderPosition);
                        setPixelColor(0x00FF00);  // Green for CW
                        break;
                        
                    case ENCODER_ROTATED_CCW:
                        Serial.printf("[ISR][%lu] Encoder CCW: %d steps (pos: %d)\n", 
                                    event.timestamp, event.value, encoderPosition);
                        setPixelColor(0xFF0000);  // Red for CCW
                        break;
                        
                    case BUTTON_PRESSED:
                        Serial.printf("[ISR][%lu] Button pressed\n", event.timestamp);
                        setPixelColor(0xFFFF00);  // Yellow on press
                        break;
                        
                    case BUTTON_RELEASED:
                        Serial.printf("[ISR][%lu] Button released (%dms)\n", 
                                    event.timestamp, event.value);
                        setPixelColor(0x0000FF);  // Blue when idle
                        break;
                        
                    case BUTTON_LONG_PRESS:
                        Serial.printf("[ISR][%lu] Long press detected (%dms)\n", 
                                    event.timestamp, event.value);
                        setPixelColor(0xFF00FF);  // Magenta for long press
                        break;
                        
                    case INTERRUPT_TRIGGERED:
                        // Special event to clear encoder position
                        clearEncoderPosition();
                        break;
                        
                    default:
                        break;
                }
                
                // Forward to main event queue for application processing
                if (event.type != INTERRUPT_TRIGGERED) {
                    xQueueSend(eventQueue, &event, 0);
                }
            }
            
            // Small delay to prevent task starvation
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    
    void setPixelColor(uint32_t color) {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        pixel.setPixelColor(0, color);
        pixel.show();
        xSemaphoreGive(i2cMutex);
    }
    
    // Public methods for application use
    bool processApplicationEvents(void (*callback)(EncoderEvent&)) {
        EncoderEvent event;
        bool hasEvents = false;
        
        while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
            hasEvents = true;
            if (callback) {
                callback(event);
            }
        }
        
        return hasEvents;
    }
    
    void setInvertEncoder(bool invert) {
        invertEncoder = invert;
    }
    
    void setLongPressThreshold(uint32_t ms) {
        longPressThresholdMs = ms;
    }
    
    int32_t getCurrentPosition() {
        return encoderPosition;
    }
    
    bool isButtonPressed() {
        return !buttonState;  // Active LOW
    }
};

// Static instance pointer for ISR access
RotaryEncoderHandler* RotaryEncoderHandler::instance = nullptr;

// Global instance
RotaryEncoderHandler encoder;

// Application callback for encoder events
void handleEncoderEvent(EncoderEvent& event) {
    // Add your application-specific handling here
    switch (event.type) {
        case ENCODER_ROTATED_CW:
            // Handle clockwise rotation
            break;
        case ENCODER_ROTATED_CCW:
            // Handle counter-clockwise rotation
            break;
        case BUTTON_PRESSED:
            // Handle button press
            break;
        case BUTTON_RELEASED:
            // Handle button release
            break;
        case BUTTON_LONG_PRESS:
            // Handle long press
            break;
        default:
            break;
    }
}

// Main application task
void applicationTask(void* param) {
    while (true) {
        // Process encoder events with callback
        encoder.processApplicationEvents(handleEncoderEvent);
        
        // Add other application logic here
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Seesaw Rotary Encoder with Hardware Interrupts Example");
    Serial.println("======================================================");
    Serial.printf("Interrupt pin: GPIO %d\n", INTERRUPT_PIN);
    Serial.println("Connect seesaw INT pin to this GPIO!");
    
    // Initialize encoder with interrupts
    if (!encoder.begin(SEESAW_ADDR)) {
        Serial.println("Failed to initialize encoder!");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("Encoder initialized with interrupt support!");
    Serial.println("- Rotate encoder to see position changes");
    Serial.println("- Press button to clear position");
    Serial.println("- Long press for special action");
    Serial.println("- All events handled via hardware interrupts");
    
    // Configure encoder behavior
    encoder.setInvertEncoder(false);
    encoder.setLongPressThreshold(1000);
    
    // Create application task
    xTaskCreatePinnedToCore(
        applicationTask,
        "AppTask",
        4096,
        NULL,
        3,
        NULL,
        1
    );
}

void loop() {
    // Empty - everything handled in tasks
    vTaskDelay(portMAX_DELAY);
}

/**
 * Hardware Interrupt Architecture:
 * 
 * 1. Interrupt Flow:
 *    - Seesaw detects encoder/button change
 *    - Seesaw INT pin goes LOW
 *    - ESP32 GPIO interrupt triggers ISR
 *    - ISR signals high-priority handler task
 *    - Handler task reads seesaw status via I2C
 *    - Events queued for processing
 * 
 * 2. ISR Design Principles:
 *    - ISR is minimal - just sets flag and wakes task
 *    - No I2C communication in ISR (would block)
 *    - Uses FreeRTOS semaphore for task signaling
 *    - Debouncing in ISR to prevent spurious interrupts
 * 
 * 3. Task Architecture:
 *    - ISR Handler Task (Priority 6): Reads I2C after interrupt
 *    - Processing Task (Priority 5): Handles events and updates
 *    - Application Task (Priority 3): User application logic
 * 
 * 4. Button State Clearing:
 *    - Position cleared on button release
 *    - Uses setEncoderPosition(0) to reset hardware counter
 *    - Clears interrupt flags to prevent spurious events
 * 
 * 5. Thread Safety:
 *    - Volatile variables for ISR-accessed data
 *    - Mutex protection for I2C operations
 *    - Separate queues for ISR and application events
 * 
 * 6. Hardware Setup:
 *    - Connect Seesaw INT pin to ESP32 GPIO (INTERRUPT_PIN)
 *    - Seesaw INT is open-drain, needs pull-up (internal enabled)
 *    - Interrupt triggers on FALLING edge (HIGH->LOW transition)
 * 
 * 7. Performance Considerations:
 *    - ISR executes in microseconds
 *    - I2C read takes ~1ms at 100kHz
 *    - Event processing latency < 10ms typical
 * 
 * 8. Error Recovery:
 *    - Add I2C timeout handling for production
 *    - Implement interrupt storm detection
 *    - Consider watchdog for critical applications
 */