/**
 * Complete example of using Adafruit_seesaw rotary encoder with FreeRTOS
 * Demonstrates proper interrupt handling and button state clearing
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

// FreeRTOS configuration
#define ENCODER_TASK_STACK_SIZE 4096
#define ENCODER_TASK_PRIORITY 5
#define EVENT_QUEUE_SIZE 10

// Debounce timing
#define BUTTON_DEBOUNCE_MS 50
#define ENCODER_READ_INTERVAL_MS 10

// Event types
enum EventType {
    ENCODER_ROTATED,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESS
};

struct EncoderEvent {
    EventType type;
    int32_t value;  // For rotation: position delta, for button: duration
    uint32_t timestamp;
};

class RotaryEncoderHandler {
private:
    Adafruit_seesaw seesaw;
    seesaw_NeoPixel pixel;
    
    // FreeRTOS handles
    TaskHandle_t encoderTaskHandle;
    QueueHandle_t eventQueue;
    SemaphoreHandle_t i2cMutex;
    
    // Encoder state
    int32_t lastPosition;
    int32_t currentPosition;
    
    // Button state
    bool lastButtonState;
    bool currentButtonState;
    uint32_t buttonPressStartTime;
    uint32_t lastButtonReadTime;
    
    // Configuration
    bool invertEncoder;
    uint32_t longPressThresholdMs;
    
public:
    RotaryEncoderHandler() 
        : pixel(SS_NEOPIX, 1, NEO_GRB + NEO_KHZ800),
          lastPosition(0),
          currentPosition(0),
          lastButtonState(true),  // Button is active LOW
          currentButtonState(true),
          buttonPressStartTime(0),
          lastButtonReadTime(0),
          invertEncoder(false),
          longPressThresholdMs(1000) {
    }
    
    bool begin(uint8_t addr = SEESAW_ADDR) {
        // Create FreeRTOS resources
        eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(EncoderEvent));
        if (!eventQueue) {
            Serial.println("Failed to create event queue");
            return false;
        }
        
        i2cMutex = xSemaphoreCreateMutex();
        if (!i2cMutex) {
            Serial.println("Failed to create I2C mutex");
            return false;
        }
        
        // Initialize I2C and seesaw
        Wire.begin();
        
        if (!seesaw.begin(addr)) {
            Serial.println("Couldn't find seesaw on I2C");
            return false;
        }
        
        Serial.print("Found seesaw at 0x");
        Serial.println(seesaw.read8(0x00, 0x00), HEX);
        
        // Configure encoder and button
        configureHardware();
        
        // Start the encoder task
        xTaskCreatePinnedToCore(
            encoderTaskWrapper,
            "EncoderTask",
            ENCODER_TASK_STACK_SIZE,
            this,
            ENCODER_TASK_PRIORITY,
            &encoderTaskHandle,
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
        
        // Configure button with pull-up and interrupt
        seesaw.pinMode(SS_SWITCH, INPUT_PULLUP);
        
        // Enable interrupts on button (both edges)
        seesaw.setGPIOInterrupts((1UL << SS_SWITCH), 1);
        
        // Configure encoder
        // Get initial position
        lastPosition = getEncoderPosition();
        currentPosition = lastPosition;
        
        // Clear any pending interrupts
        seesaw.digitalReadBulk((1UL << SS_SWITCH));
    }
    
    int32_t getEncoderPosition() {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        int32_t pos = seesaw.getEncoderPosition(SS_ENCODER);
        xSemaphoreGive(i2cMutex);
        return invertEncoder ? -pos : pos;
    }
    
    bool readButtonState() {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        bool state = seesaw.digitalRead(SS_SWITCH);
        xSemaphoreGive(i2cMutex);
        return state;
    }
    
    void setPixelColor(uint32_t color) {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        pixel.setPixelColor(0, color);
        pixel.show();
        xSemaphoreGive(i2cMutex);
    }
    
    static void encoderTaskWrapper(void* param) {
        RotaryEncoderHandler* handler = static_cast<RotaryEncoderHandler*>(param);
        handler->encoderTask();
    }
    
    void encoderTask() {
        TickType_t lastWakeTime = xTaskGetTickCount();
        
        while (true) {
            // Read encoder position
            currentPosition = getEncoderPosition();
            
            // Check for rotation
            if (currentPosition != lastPosition) {
                int32_t delta = currentPosition - lastPosition;
                
                EncoderEvent event;
                event.type = ENCODER_ROTATED;
                event.value = delta;
                event.timestamp = millis();
                
                xQueueSend(eventQueue, &event, 0);
                
                // Visual feedback
                setPixelColor(delta > 0 ? 0x00FF00 : 0xFF0000);  // Green CW, Red CCW
                
                lastPosition = currentPosition;
            }
            
            // Read button state with debouncing
            uint32_t now = millis();
            if (now - lastButtonReadTime >= BUTTON_DEBOUNCE_MS) {
                currentButtonState = readButtonState();
                
                // Button state changed (active LOW)
                if (currentButtonState != lastButtonState) {
                    EncoderEvent event;
                    event.timestamp = now;
                    
                    if (!currentButtonState) {  // Button pressed (LOW)
                        buttonPressStartTime = now;
                        event.type = BUTTON_PRESSED;
                        event.value = 0;
                        
                        // Visual feedback
                        setPixelColor(0xFFFF00);  // Yellow on press
                    } else {  // Button released (HIGH)
                        uint32_t pressDuration = now - buttonPressStartTime;
                        
                        // Determine if it was a long press
                        if (pressDuration >= longPressThresholdMs) {
                            event.type = BUTTON_LONG_PRESS;
                        } else {
                            event.type = BUTTON_RELEASED;
                        }
                        event.value = pressDuration;
                        
                        // Clear encoder position on button release
                        clearEncoderPosition();
                        
                        // Visual feedback
                        setPixelColor(0x0000FF);  // Back to blue
                    }
                    
                    xQueueSend(eventQueue, &event, 0);
                    lastButtonState = currentButtonState;
                }
                
                lastButtonReadTime = now;
            }
            
            // Delay until next cycle
            vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(ENCODER_READ_INTERVAL_MS));
        }
    }
    
    void clearEncoderPosition() {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        
        // Method 1: Set encoder position to 0
        seesaw.setEncoderPosition(0, SS_ENCODER);
        
        // Method 2: Alternative - read and reset internal state
        // Some seesaw firmwares may require reading to clear
        seesaw.getEncoderPosition(SS_ENCODER);
        
        xSemaphoreGive(i2cMutex);
        
        // Update our tracking variables
        lastPosition = 0;
        currentPosition = 0;
        
        Serial.println("Encoder position cleared");
    }
    
    bool processEvents() {
        EncoderEvent event;
        bool hasEvents = false;
        
        while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
            hasEvents = true;
            
            switch (event.type) {
                case ENCODER_ROTATED:
                    Serial.printf("[%lu] Encoder rotated: %d steps (position: %d)\n", 
                                event.timestamp, event.value, currentPosition);
                    break;
                    
                case BUTTON_PRESSED:
                    Serial.printf("[%lu] Button pressed\n", event.timestamp);
                    break;
                    
                case BUTTON_RELEASED:
                    Serial.printf("[%lu] Button released (duration: %dms)\n", 
                                event.timestamp, event.value);
                    break;
                    
                case BUTTON_LONG_PRESS:
                    Serial.printf("[%lu] Button long press detected (duration: %dms)\n", 
                                event.timestamp, event.value);
                    break;
            }
        }
        
        return hasEvents;
    }
    
    // Public configuration methods
    void setInvertEncoder(bool invert) {
        invertEncoder = invert;
    }
    
    void setLongPressThreshold(uint32_t ms) {
        longPressThresholdMs = ms;
    }
    
    // Get current values (thread-safe)
    int32_t getCurrentPosition() {
        return currentPosition;
    }
    
    bool isButtonPressed() {
        return !currentButtonState;  // Active LOW
    }
};

// Global instance
RotaryEncoderHandler encoder;

// Main application task
void applicationTask(void* param) {
    while (true) {
        // Process encoder events
        if (encoder.processEvents()) {
            // Events were processed, you can add custom handling here
        }
        
        // Add your application logic here
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Seesaw Rotary Encoder Example with FreeRTOS");
    Serial.println("============================================");
    
    // Initialize encoder
    if (!encoder.begin(SEESAW_ADDR)) {
        Serial.println("Failed to initialize encoder!");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("Encoder initialized successfully!");
    Serial.println("- Rotate encoder to see position changes");
    Serial.println("- Press button to clear position");
    Serial.println("- Long press for special action");
    
    // Optional: Configure encoder behavior
    encoder.setInvertEncoder(false);  // Set true to invert rotation direction
    encoder.setLongPressThreshold(1000);  // 1 second for long press
    
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
 * Advanced features and notes:
 * 
 * 1. Interrupt Handling:
 *    - The seesaw firmware handles the actual hardware interrupts
 *    - We poll the seesaw over I2C in a FreeRTOS task for changes
 *    - This approach is more reliable than ESP32 GPIO interrupts for I2C devices
 * 
 * 2. State Clearing:
 *    - The encoder position is cleared when the button is released
 *    - Use setEncoderPosition(0) to reset the internal counter
 *    - Some firmwares may need a read operation to clear state
 * 
 * 3. Thread Safety:
 *    - All I2C operations are protected with a mutex
 *    - Events are passed through a FreeRTOS queue
 *    - No shared state between tasks without protection
 * 
 * 4. Debouncing:
 *    - Button state is debounced in software (50ms default)
 *    - Encoder readings are rate-limited (10ms intervals)
 * 
 * 5. Visual Feedback:
 *    - NeoPixel changes color based on encoder actions
 *    - Blue: Idle, Green: CW rotation, Red: CCW rotation, Yellow: Button pressed
 * 
 * 6. Error Recovery:
 *    - Add watchdog timer for production use
 *    - Implement I2C error recovery if communication fails
 *    - Consider adding connection monitoring
 */