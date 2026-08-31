#include "Coordinator.hpp"

#include "Event.hpp"
#include "ErrorHandler.hpp"
#include <MyBle.hpp>
#include <I2cBus.hpp>
#include <LightColorRules.hpp>
#include <OtaTransferRules.hpp>
#include <VisualFeedbackRules.hpp>
#include <DeepSleep.hpp>
#include <SleepRules.hpp>
#include <utils.hpp>
#include <esp_sleep.h>

#if !CONFIG_FREERTOS_UNICORE
static_assert(ARDUINO_RUNNING_CORE == 1,
              "UI and application work must stay on the ESP32-S3 application core");
static_assert(CONFIG_BT_CTRL_PINNED_TO_CORE == 0,
              "Bluetooth controller must remain isolated on the radio core");
static_assert(CONFIG_BT_NIMBLE_PINNED_TO_CORE == 0,
              "NimBLE host must remain isolated on the radio core");
#endif

[[noreturn]]
void dispatcherTask(void* param) {
    auto* coordinator = static_cast<Coordinator*>(param);
    DEBUG_PRINTF("Tasks: dispatcher core=%d\n", xPortGetCoreID());
    Event e{};
    while (true) {
        if(xQueueReceive(coordinator->eventQueue, &e, portMAX_DELAY)) {
            xSemaphoreTake(coordinator->stateMutex, portMAX_DELAY);
            if (!coordinator->sleeping.load() && !coordinator->otaModeActive()) {
                coordinator->state.handleEvent(e, coordinator);
            }
            xSemaphoreGive(coordinator->stateMutex);
        }
    }
}


Coordinator::Coordinator() :
    eventQueue{xQueueCreate(32, sizeof(Event))},
    stateMutex{xSemaphoreCreateMutex()},
    pendingInputEvents(0),
    display1{},
    display2{},
    display3{},
    display4{},
    buttonGrid{this},
    rotaryEncoder{this},
    ble{this}
{
    CHECK_POINTER(eventQueue, ErrorCode::QUEUE_CREATE_FAILED, "Coordinator event queue");
    CHECK_POINTER(stateMutex, ErrorCode::SEMAPHORE_CREATE_FAILED, "Coordinator state mutex");
}

BoardRole Coordinator::myRole() {
    return myRoleConfig().role;
}

const BoardRoleConfig& Coordinator::myRoleConfig() {
    return getRoleConfig(ble.getMyPeerId());
}

void Coordinator::serviceStateHeartbeat() {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (!sleeping.load() && !otaModeActive()) {
        state.heartbeat(this);
    }
    xSemaphoreGive(stateMutex);
}

bool Coordinator::enqueueEvent(const Event& event) {
    return xQueueSend(eventQueue, &event, 0) == pdPASS;
}

void Coordinator::setup() {
    scorebot::deep_sleep::handleTimerWake(*this);
    // Enable serial and wait for 5s delay to allow serial monitor to connect

    Serial.begin(115200);
    usbConnection.begin();
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        delay(5000);  // Ensure see the serial messages from the beginning on cold boot.
    }
    // The `while(serial)` thing doesn't terminate unless connected to a serial (USB).

    if (!Serial.available()) {
        Serial.setTimeout(1);
    }
    // may need an i2c lock because Wire.h almost certainly buffers.
    Wire.begin(5, 6);
    CHECK_FREERTOS_RESULT(I2cBus::initialize() ? pdPASS : pdFAIL,
                          ErrorCode::SEMAPHORE_CREATE_FAILED, "I2C mutex");
    // print i2c devices for debugging hardware
    printI2CDevices();

    // BLE identity is the device's factory MAC-derived id, so it must be initialized first.
    ble.setup();
    state.restore();
    DEBUG_PRINTF("State: started=%d connected=0x%02x roster=0x%02x turn=%u\n",
                 state.gameStarted,
                 static_cast<unsigned>(state.connectedMask),
                 static_cast<unsigned>(state.rosterMask),
                 static_cast<unsigned>(state.whosTurn));
    if (myRole() == BoardRole::Leader) {
        // Connections are live transport state, not something that survives a
        // reboot. Frozen roster membership is persisted separately.
        state.connectedMask = 0;
        state.leaderId = ble.getMyPeerId();
        state.leaderless = false;
        ++state.term;
        if (state.gameStarted) {
            ble.freezeRoster(state.rosterMask);
        }
        if (!state.persist()) {
            FATAL_ERROR(ErrorCode::STATE_PERSIST_FAILED, "leader startup state");
        }
    } else {
        // A saved replica is useful for recovery, but is never authority while
        // this board is waiting for a live leaderboard connection.
        state.leaderless = !ble.hasLeader();
    }

    display1.setup(0x70);
    display1.print("----");
    if (myRole() == BoardRole::Leader) {
        display2.setup(0x71);
        display2.print("----");
        display3.setup(0x72);
        display3.print("----");
        display4.setup(0x73);
        display4.print("----");
    }
    
    buttonGrid.setup();
    rotaryEncoder.setup();
    state.refreshDisplays(this);
    updateDisplayBrightness();
    awakeSinceMs = millis();

    DEBUG_PRINTF("Tasks: Arduino loop core=%d configured=%d, BLE core=%d\n",
                 xPortGetCoreID(), ARDUINO_RUNNING_CORE,
                 CONFIG_BT_NIMBLE_PINNED_TO_CORE);
    BaseType_t taskResult = xTaskCreatePinnedToCore(
        dispatcherTask, "dispatcher", 4096, this, 2, nullptr,
        ARDUINO_RUNNING_CORE);
    CHECK_FREERTOS_RESULT(taskResult, ErrorCode::TASK_CREATE_FAILED, "Coordinator dispatcher task");
}

void Coordinator::loop() {
    usbConnection.poll();
    flushInputEvents();
    this->ble.loop();
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (otaUiActive.load() && !ble.otaActive()) {
        finishOtaUi();
    }
    if (!otaUiActive.load()) {
        this->state.heartbeat(this);
    }
    updateDisplayBrightness();
    const bool shouldSleep = scorebot::sleep::isDue(
        millis(), awakeSinceMs, lastInteractionMs.load(), sleepBlocked(),
        scorebot::sleep::idleTimeoutMs(
            myRole() == BoardRole::Leader, state.myScore != 0));
    if (shouldSleep) {
        sleeping.store(true);
    }
    xSemaphoreGive(stateMutex);
    if (shouldSleep) {
        // USB can be connected just as the idle timeout expires. Give the
        // native controller one final, short enumeration window before any
        // peripherals are shut down. This runs only at an actual sleep edge.
        if (usbConnection.connectionAppearsWithin(
                scorebot::usb::kFinalSleepProbeMs)) {
            sleeping.store(false);
        } else {
            scorebot::deep_sleep::enter(*this);
        }
    }
    delay(5);  // let the idle task run instead of continuously spinning a CPU core
}

bool Coordinator::sleepBlocked() const {
    return state.pendingOperation != 0 ||
           state.rotaryPressStartedMs.load() != 0 ||
           pendingInputEvents.load(std::memory_order_acquire) != 0 ||
           usbConnection.connected() ||
           !ble.sleepAllowed();
}

void Coordinator::noteInteraction() {
    lastInteractionMs.store(millis());
}

void Coordinator::armOta() {
    // Publish exclusive UI intent before asking the BLE-owner loop to arm.
    // That closes the dispatcher gate immediately after the hold event.
    otaUiActive.store(true);
    pendingInputEvents.store(0);
    ble.armOta();
    showOtaUi();
}

bool Coordinator::otaModeActive() const {
    return otaUiActive.load();
}

void Coordinator::setPlayerTurnAnimation(bool enabled) {
    const bool wasEnabled = playerTurnAnimationActive.load();
    if (enabled && !wasEnabled) {
        playerTurnAnimationStartedMs.store(millis());
    }
    playerTurnAnimationActive.store(enabled);
}

void Coordinator::setLeaderboardTurnColor(uint32_t color) {
    leaderboardTurnColor.store(color);
}

void Coordinator::enqueueInputFromISR(ButtonName buttonName) {
    static_assert(std::atomic<uint8_t>::is_always_lock_free,
                  "input-event coalescing must remain ISR-safe");
    if (otaUiActive.load(std::memory_order_relaxed)) {
        return;
    }
    const uint8_t bit = buttonName == ButtonName::GPIOButtons ? 1u : 2u;
    pendingInputEvents.fetch_or(bit, std::memory_order_relaxed);
}

void Coordinator::flushInputEvents() {
    uint8_t pending = pendingInputEvents.exchange(0, std::memory_order_acq_rel);
    if (otaUiActive.load()) {
        return;
    }
    while (pending != 0) {
        const ButtonName buttonName = (pending & 1u) != 0
                                          ? ButtonName::GPIOButtons
                                          : ButtonName::RotaryEncoder;
        const uint8_t bit = buttonName == ButtonName::GPIOButtons ? 1u : 2u;
        Event event{};
        event.type = EventType::ButtonPressed;
        event.press.buttonName = buttonName;
        if (xQueueSend(eventQueue, &event, 0) != pdPASS) {
            pendingInputEvents.fetch_or(pending, std::memory_order_release);
            return;
        }
        pending &= ~bit;
    }
}

void Coordinator::updateDisplayBrightness() {
    const uint32_t now = millis();
    if (otaUiActive.load()) {
        const uint32_t color = scorebot::otaIndicatorColor(ble.otaWriting(), now);
        if (color != lastOtaLightColor) {
            rotaryEncoder.setColor(color);
            lastOtaLightColor = color;
        }
        return;
    }
    if (myRole() != BoardRole::Leader && playerTurnAnimationActive.load()) {
        if (!turnAnimationWasActive) {
            lastTurnSegmentBrightness = 0xff;
            lastTurnLightColor = 0xffffffff;
            turnAnimationWasActive = true;
        }
        const scorebot::TurnPulse pulse = scorebot::turnPulseAt(
            now - playerTurnAnimationStartedMs.load());
        if (pulse.segmentBrightness != lastTurnSegmentBrightness) {
            display1.setBrightnessNow(pulse.segmentBrightness);
            lastTurnSegmentBrightness = pulse.segmentBrightness;
        }
        const uint32_t color = scorebot::scaleRgb(
            myRoleConfig().color, pulse.lightLevel);
        if (color != lastTurnLightColor) {
            rotaryEncoder.setColor(color);
            lastTurnLightColor = color;
        }
        return;
    }

    if (turnAnimationWasActive) {
        turnAnimationWasActive = false;
        lastTurnSegmentBrightness = 0xff;
        lastTurnLightColor = 0xffffffff;
        rotaryEncoder.setColor(0x000000);
        // setBrightnessNow() keeps the pulse value as the transition target.
        // Force the normal path to choose a fresh target after leaving GO.
        displaysAreActive = !display_brightness::isInteractionActive(
            now, lastInteractionMs.load());
    }

    const bool active = display_brightness::isInteractionActive(
        now, lastInteractionMs.load());
    if (active != displaysAreActive) {
        const uint8_t targetBrightness = display_brightness::targetFor(active);
        display1.setTargetBrightness(targetBrightness);
        display2.setTargetBrightness(targetBrightness);
        display3.setTargetBrightness(targetBrightness);
        display4.setTargetBrightness(targetBrightness);
        displaysAreActive = active;
    }

    display1.updateBrightness(now);
    display2.updateBrightness(now);
    display3.updateBrightness(now);
    display4.updateBrightness(now);

    if (myRole() == BoardRole::Leader) {
        const uint32_t baseColor = leaderboardTurnColor.load();
        const uint32_t color = baseColor == 0
                                   ? 0
                                   : scorebot::scaleRgb(
                                         baseColor,
                                         scorebot::leaderboardLightLevel(
                                             display1.currentBrightness()));
        if (color != lastLeaderboardLightColor) {
            rotaryEncoder.setColor(color);
            lastLeaderboardLightColor = color;
        }
    }
}

void Coordinator::showOtaUi() {
    setPlayerTurnAnimation(false);
    setLeaderboardTurnColor(0);
    display1.setBrightnessNow(display_brightness::kActiveBrightness);
    display1.print("OTA ");
    if (myRole() == BoardRole::Leader) {
        display2.setBrightnessNow(display_brightness::kActiveBrightness);
        display3.setBrightnessNow(display_brightness::kActiveBrightness);
        display4.setBrightnessNow(display_brightness::kActiveBrightness);
        display2.print("OTA ");
        display3.print("OTA ");
        display4.print("OTA ");
    }
    rotaryEncoder.setColor(scorebot::kOtaPurple);
    lastOtaLightColor = scorebot::kOtaPurple;
    turnAnimationWasActive = false;
    displaysAreActive = true;
}

void Coordinator::finishOtaUi() {
    otaUiActive.store(false);
    lastOtaLightColor = 0xffffffff;
    lastTurnLightColor = 0xffffffff;
    lastLeaderboardLightColor = 0xffffffff;
    rotaryEncoder.setColor(0x000000);
    state.refreshDisplays(this);

    const bool active = display_brightness::isInteractionActive(
        millis(), lastInteractionMs.load());
    const uint8_t target = display_brightness::targetFor(active);
    display1.setTargetBrightness(target);
    display2.setTargetBrightness(target);
    display3.setTargetBrightness(target);
    display4.setTargetBrightness(target);
    displaysAreActive = active;
}
