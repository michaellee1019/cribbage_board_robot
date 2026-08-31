#include "Coordinator.hpp"

#include "Event.hpp"
#include "ErrorHandler.hpp"
#include <MyBle.hpp>
#include <I2cBus.hpp>
#include <LeaderboardUiRules.hpp>
#include <LightColorRules.hpp>
#include <OtaTransferRules.hpp>
#include <VisualFeedbackRules.hpp>
#include <DeepSleep.hpp>
#include <SleepRules.hpp>
#include <utils.hpp>
#include <esp_sleep.h>

#include <array>
#include <cstring>

namespace {
constexpr uint8_t inputIndex(ButtonName buttonName) {
    return buttonName == ButtonName::GPIOButtons ? 0u : 1u;
}

constexpr uint8_t inputBit(ButtonName buttonName) {
    return static_cast<uint8_t>(1u << inputIndex(buttonName));
}

constexpr ButtonName inputName(uint8_t index) {
    return index == 0 ? ButtonName::GPIOButtons : ButtonName::RotaryEncoder;
}

constexpr uint32_t kResetPropagationMs = 150;
}  // namespace

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
            uint8_t input = 0xff;
            uint32_t inputEpoch = 0;
            if (e.type == EventType::ButtonPressed) {
                input = inputIndex(e.press.buttonName);
                // Consume exactly the interrupt generation observed before
                // the I2C read. An edge arriving during that read remains a
                // newer generation and is retried after this dispatch.
                inputEpoch = coordinator->inputInterruptEpoch[input].load(
                    std::memory_order_acquire);
            }
            xSemaphoreTake(coordinator->stateMutex, portMAX_DELAY);
            if (!coordinator->otaModeActive() &&
                !coordinator->restartRequested.load(std::memory_order_acquire)) {
                coordinator->state.handleEvent(e, coordinator);
            } else if (e.type == EventType::ButtonPressed) {
                // OTA owns the UI, but the level-latched I2C interrupt still
                // has to be consumed before its GPIO line can rise again.
                coordinator->discardInput(e.press.buttonName);
            }
            xSemaphoreGive(coordinator->stateMutex);

            if (input != 0xff) {
                coordinator->inputConsumedEpoch[input].store(
                    inputEpoch, std::memory_order_release);
                coordinator->queuedInputEvents.fetch_and(
                    static_cast<uint8_t>(~(1u << input)),
                    std::memory_order_release);
            }
            coordinator->outstandingEvents.fetch_sub(
                1, std::memory_order_acq_rel);
            coordinator->servicePendingInputEvents();
        }
    }
}

[[noreturn]]
void persistenceTask(void* param) {
    auto* coordinator = static_cast<Coordinator*>(param);
    DEBUG_PRINTF("Tasks: persistence core=%d\n", xPortGetCoreID());
    StatePersistenceRequest request{};
    StatePersistenceRequest newer{};
    while (true) {
        if (xQueueReceive(
                coordinator->statePersistenceQueue, &request,
                portMAX_DELAY) != pdPASS) {
            continue;
        }
        // Recovery only needs the latest immutable snapshot. Collapse a burst
        // before entering the comparatively slow flash transaction.
        while (xQueueReceive(
                   coordinator->statePersistenceQueue, &newer, 0) == pdPASS) {
            request = newer;
        }
        if (!GameState::persistEncoded(request.snapshot)) {
            FATAL_ERROR(
                ErrorCode::STATE_PERSIST_FAILED,
                "asynchronous state persistence");
        }
        coordinator->persistedGeneration.store(
            request.generation, std::memory_order_release);
    }
}


Coordinator::Coordinator() :
    eventQueue{xQueueCreate(32, sizeof(Event))},
    statePersistenceQueue{xQueueCreate(1, sizeof(StatePersistenceRequest))},
    stateMutex{xSemaphoreCreateMutex()},
    display1{},
    display2{},
    display3{},
    display4{},
    buttonGrid{this},
    rotaryEncoder{this},
    ble{this}
{
    for (auto& epoch : inputInterruptEpoch) {
        epoch.store(0, std::memory_order_relaxed);
    }
    for (auto& epoch : inputConsumedEpoch) {
        epoch.store(0, std::memory_order_relaxed);
    }
    CHECK_POINTER(eventQueue, ErrorCode::QUEUE_CREATE_FAILED, "Coordinator event queue");
    CHECK_POINTER(
        statePersistenceQueue, ErrorCode::QUEUE_CREATE_FAILED,
        "Coordinator persistence queue");
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
    if (sleepTransition.load(std::memory_order_acquire) ==
            SleepTransition::Awake &&
        !otaModeActive() && !restartRequested.load()) {
        state.heartbeat(this);
    }
    xSemaphoreGive(stateMutex);
}

bool Coordinator::enqueueEvent(const Event& event) {
    outstandingEvents.fetch_add(1, std::memory_order_acq_rel);
    if (xQueueSend(eventQueue, &event, 0) != pdPASS) {
        outstandingEvents.fetch_sub(1, std::memory_order_acq_rel);
        return false;
    }
    cancelSleepIntent();
    return true;
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
        if (!state.gameStarted) {
            // Availability-driven defaults are live transport state. After a
            // reboot, untouched colors remain OFF until their boards rejoin;
            // explicitly selected LOCAL/PAIR/OFF modes remain as chosen.
            state.lobbyEnabledMask = static_cast<uint8_t>(
                state.lobbyEnabledMask & state.lobbyExplicitMask);
            state.localControlMask = static_cast<uint8_t>(
                state.localControlMask & state.lobbyExplicitMask);
        }
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

    CHECK_FREERTOS_RESULT(
        printer.setup(myRole() == BoardRole::Leader) ? pdPASS : pdFAIL,
        ErrorCode::TASK_CREATE_FAILED, "leaderboard printer worker");

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
    state.syncLeaderboardSelection(this);
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
    BaseType_t persistenceTaskResult = xTaskCreatePinnedToCore(
        persistenceTask, "persistence", 4096, this, 1, nullptr,
        ARDUINO_RUNNING_CORE);
    CHECK_FREERTOS_RESULT(
        persistenceTaskResult, ErrorCode::TASK_CREATE_FAILED,
        "Coordinator persistence task");
}

void Coordinator::loop() {
    usbConnection.poll();
    servicePendingInputEvents();
    this->ble.loop();
    if (servicePendingRestart()) {
        delay(1);
        return;
    }
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (otaUiActive.load() && !ble.otaActive()) {
        finishOtaUi();
    }
    if (!otaUiActive.load()) {
        this->state.heartbeat(this);
    }
    updateDisplayBrightness();
    const bool shouldSleep = sleepDue();
    if (shouldSleep) {
        SleepTransition expected = SleepTransition::Awake;
        (void)sleepTransition.compare_exchange_strong(
            expected, SleepTransition::Intent, std::memory_order_acq_rel);
    }
    xSemaphoreGive(stateMutex);
    if (restartRequested.load()) {
        delay(1);
        return;
    }
    if (shouldSleep) {
        // USB can be connected just as the idle timeout expires. Give the
        // native controller one final, short enumeration window before any
        // peripherals are shut down. This runs only at an actual sleep edge.
        if (usbConnection.connectionAppearsWithin(
                scorebot::usb::kFinalSleepProbeMs)) {
            sleepTransition.store(
                SleepTransition::Awake, std::memory_order_release);
        } else {
            SleepTransition expected = SleepTransition::Intent;
            if (sleepTransition.compare_exchange_strong(
                    expected, SleepTransition::Quiescing,
                    std::memory_order_acq_rel)) {
                // Stop every producer before the final decision. BLE callbacks
                // reject new game writes, while detached active-low GPIO lines
                // retain their peripheral latches for either abort recovery or
                // an immediate EXT1 wake after commit.
                ble.beginSleepQuiesce();
                buttonGrid.prepareForSleep();
                rotaryEncoder.prepareForSleep();

                xSemaphoreTake(stateMutex, portMAX_DELAY);
                expected = SleepTransition::Quiescing;
                const bool inputAsserted = buttonGrid.interruptAsserted() ||
                                           rotaryEncoder.interruptAsserted();
                if (!inputAsserted && sleepDue() &&
                    sleepTransition.compare_exchange_strong(
                        expected, SleepTransition::Committed,
                        std::memory_order_acq_rel)) {
                    scorebot::deep_sleep::enter(*this);
                }
                xSemaphoreGive(stateMutex);

                sleepTransition.store(
                    SleepTransition::Awake, std::memory_order_release);
                ble.cancelSleepQuiesce();
                buttonGrid.resumeAfterSleepAbort();
                rotaryEncoder.resumeAfterSleepAbort();
                if (buttonGrid.interruptAsserted()) {
                    recordLatchedInput(ButtonName::GPIOButtons);
                }
                if (rotaryEncoder.interruptAsserted()) {
                    recordLatchedInput(ButtonName::RotaryEncoder);
                }
            }
        }
    }
    delay(5);  // let the idle task run instead of continuously spinning a CPU core
}

bool Coordinator::sleepBlocked() const {
    return state.pendingOperation != 0 ||
           state.rotaryPressStartedMs.load() != 0 ||
           maintenanceModeActive() ||
           printer.uiActive() ||
           printer.wifiPowered() ||
           inputPending() ||
           outstandingEvents.load(std::memory_order_acquire) != 0 ||
           statePersistencePending() ||
           restartRequested.load(std::memory_order_acquire) ||
           ble.transmissionsPending() ||
           usbConnection.connected() ||
           !ble.sleepAllowed();
}

bool Coordinator::sleepDue() {
    return scorebot::sleep::isDue(
        millis(), awakeSinceMs, lastInteractionMs.load(), sleepBlocked(),
        scorebot::sleep::idleTimeoutMs(
            myRole() == BoardRole::Leader, state.hasPendingLocalScore()));
}

bool Coordinator::inputPending() const {
    for (size_t index = 0; index < inputInterruptEpoch.size(); ++index) {
        if (inputInterruptEpoch[index].load(std::memory_order_acquire) !=
            inputConsumedEpoch[index].load(std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

void Coordinator::cancelSleepIntent() {
    SleepTransition expected = sleepTransition.load(std::memory_order_acquire);
    while (expected == SleepTransition::Intent ||
           expected == SleepTransition::Quiescing) {
        if (sleepTransition.compare_exchange_weak(
                expected, SleepTransition::Awake,
                std::memory_order_acq_rel)) {
            return;
        }
    }
}

void Coordinator::servicePendingInputEvents() {
    for (uint8_t index = 0; index < inputInterruptEpoch.size(); ++index) {
        if (inputInterruptEpoch[index].load(std::memory_order_acquire) ==
            inputConsumedEpoch[index].load(std::memory_order_acquire)) {
            continue;
        }
        const uint8_t bit = static_cast<uint8_t>(1u << index);
        if ((queuedInputEvents.fetch_or(bit, std::memory_order_acq_rel) & bit) != 0) {
            continue;
        }
        Event event{};
        event.type = EventType::ButtonPressed;
        event.press.buttonName = inputName(index);
        if (!enqueueEvent(event)) {
            queuedInputEvents.fetch_and(
                static_cast<uint8_t>(~bit), std::memory_order_release);
        }
    }
}

void Coordinator::recordLatchedInput(ButtonName buttonName) {
    inputInterruptEpoch[inputIndex(buttonName)].fetch_add(
        1, std::memory_order_release);
    cancelSleepIntent();
    servicePendingInputEvents();
}

uint32_t Coordinator::enqueueStatePersistence(const String& snapshot) {
    if (statePersistenceQueue == nullptr ||
        snapshot.length() > scorebot::kMaxWireMessageSize) {
        return 0;
    }
    StatePersistenceRequest request{};
    request.generation = requestedPersistenceGeneration.fetch_add(
                             1, std::memory_order_acq_rel) +
                         1;
    memcpy(request.snapshot, snapshot.c_str(), snapshot.length());
    request.snapshot[snapshot.length()] = '\0';
    return xQueueOverwrite(statePersistenceQueue, &request) == pdPASS
               ? request.generation
               : 0;
}

bool Coordinator::statePersistencePending() const {
    return requestedPersistenceGeneration.load(std::memory_order_acquire) !=
           persistedGeneration.load(std::memory_order_acquire);
}

bool Coordinator::waitForStatePersistence(uint32_t timeoutMs) const {
    const uint32_t startedMs = millis();
    while (statePersistencePending()) {
        if (millis() - startedMs >= timeoutMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

void Coordinator::requestRestart(bool waitForBleDrain) {
    restartReadyAtMs = 0;
    restartPrepared.store(false, std::memory_order_release);
    restartTransmissionGeneration.store(0, std::memory_order_release);
    restartWaitsForBle.store(waitForBleDrain, std::memory_order_release);
    restartRequested.store(true, std::memory_order_release);
    sleepTransition.store(SleepTransition::Awake, std::memory_order_release);
    ble.beginRestartQuiesce();
}

void Coordinator::setRestartTransmission(uint32_t generation) {
    restartTransmissionGeneration.store(generation, std::memory_order_release);
}

void Coordinator::finishRestartPreparation() {
    restartPrepared.store(true, std::memory_order_release);
}

bool Coordinator::servicePendingRestart() {
    if (!restartRequested.load(std::memory_order_acquire)) {
        return false;
    }
    sleepTransition.store(SleepTransition::Awake, std::memory_order_release);
    if (!restartPrepared.load(std::memory_order_acquire)) {
        return true;
    }
    if (restartWaitsForBle.load(std::memory_order_acquire)) {
        const uint32_t generation = restartTransmissionGeneration.load(
            std::memory_order_acquire);
        if (generation == 0) {
            return true;
        }
        const MyBle::TransmissionCompletion completion =
            ble.transmissionCompletion(generation);
        if (completion == MyBle::TransmissionCompletion::Pending) {
            return true;
        }
        const uint32_t completedAtMs =
            ble.transmissionCompletedAtMs(generation);
        if (completedAtMs != 0) {
            restartReadyAtMs = completedAtMs + kResetPropagationMs;
            if (static_cast<int32_t>(millis() - restartReadyAtMs) < 0) {
                return true;
            }
        }
        if (completion == MyBle::TransmissionCompletion::Failed) {
            DEBUG_PRINTF(
                "Reset: BLE propagation timed out generation=%lu\n",
                static_cast<unsigned long>(generation));
            ESP.restart();
            return true;
        }
    }
    ESP.restart();
    return true;
}

void Coordinator::noteInteraction() {
    lastInteractionMs.store(millis());
}

void Coordinator::armOta() {
    // Publish exclusive UI intent before asking the BLE-owner loop to arm.
    // That closes the dispatcher gate immediately after the hold event.
    otaUiActive.store(true);
    maintenanceUiAction.store(scorebot::MaintenanceAction::None);
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
    if (myRole() != BoardRole::Leader || otaUiActive.load() ||
        maintenanceModeActive() || printer.uiActive()) {
        return;
    }

    // A state transition can still be saving to flash while the normal UI
    // animation loop is blocked on the state mutex. Paint the newly-known turn
    // color here so the light, display, and input target change together.
    const int8_t selected = leaderboardSelectedDisplay.load();
    const uint8_t referenceBrightness =
        selected == 0 ? display2.currentBrightness() : display1.currentBrightness();
    const uint32_t immediateColor = color == 0
                                        ? 0
                                        : scorebot::scaleRgb(
                                              color,
                                              scorebot::leaderboardLightLevel(
                                                  referenceBrightness));
    if (immediateColor != lastLeaderboardLightColor) {
        rotaryEncoder.setColor(immediateColor);
        lastLeaderboardLightColor = immediateColor;
    }
}

void Coordinator::setLeaderboardSelectedDisplay(
    int8_t index, bool lobbySelection, bool localPrompt) {
    const bool allowNormalWrites = normalUiWritesAllowed();
    const int8_t previous = leaderboardSelectedDisplay.exchange(index);
    const bool previousLobby = leaderboardLobbySelection.exchange(lobbySelection);
    const bool previousPrompt = leaderboardLocalPrompt.exchange(localPrompt);
    if (previous != index || previousLobby != lobbySelection ||
        previousPrompt != localPrompt) {
        leaderboardSelectionStartedMs.store(millis());
        lastSelectedSegmentBrightness = 0xff;
        lastLeaderboardPromptFrame = 0xffffffff;
        // Force the normal brightness targets to be restored on the display
        // that has just stopped owning the selection animation.
        displaysAreActive = !display_brightness::isInteractionActive(
            millis(), lastInteractionMs.load());
        if (allowNormalWrites && !lobbySelection && index >= 0 && index < 4) {
            const std::array<HT16Display*, 4> displays = {
                &display1, &display2, &display3, &display4};
            displays[static_cast<size_t>(index)]->setBrightnessNow(
                scorebot::turnPulseAt(0).segmentBrightness);
            lastSelectedSegmentBrightness =
                scorebot::turnPulseAt(0).segmentBrightness;
        }
    }

    if (!allowNormalWrites) {
        return;
    }

    // The display controller provides a true on/off blink, which is much more
    // legible in the lobby than varying LED brightness. Reapply it on every
    // state refresh so cancelling maintenance or OTA restores the selection.
    const std::array<HT16Display*, 4> displays = {
        &display1, &display2, &display3, &display4};
    for (HT16Display* display : displays) {
        display->setBlinkRate(0.0f);
    }
    if (lobbySelection && index >= 0 && index < 4) {
        displays[static_cast<size_t>(index)]->setBlinkRate(1.0f);
    }
}

void Coordinator::startMaintenanceUi(
    scorebot::MaintenanceAction action, uint32_t startedMs) {
    setPlayerTurnAnimation(false);
    maintenanceUiStartedMs.store(startedMs);
    maintenanceUiAction.store(action);
    lastMaintenanceFrame = 0xffffffff;
    lastMaintenanceLightColor = 0xffffffff;
    display1.setBrightnessNow(display_brightness::kActiveBrightness);
    if (myRole() == BoardRole::Leader) {
        display1.setBlinkRate(0.0f);
        display2.setBlinkRate(0.0f);
        display3.setBlinkRate(0.0f);
        display4.setBlinkRate(0.0f);
        display2.setBrightnessNow(display_brightness::kActiveBrightness);
        display3.setBrightnessNow(display_brightness::kActiveBrightness);
        display4.setBrightnessNow(display_brightness::kActiveBrightness);
    }
}

void Coordinator::cancelMaintenanceUi() {
    maintenanceUiAction.store(scorebot::MaintenanceAction::None);
    lastMaintenanceFrame = 0xffffffff;
    lastMaintenanceLightColor = 0xffffffff;
    rotaryEncoder.setColor(0x000000);
    state.refreshDisplays(this);
    displaysAreActive = !display_brightness::isInteractionActive(
        millis(), lastInteractionMs.load());
}

bool Coordinator::maintenanceModeActive() const {
    return maintenanceUiAction.load() != scorebot::MaintenanceAction::None;
}

bool Coordinator::startPrint(const scorebot::PrinterSnapshot& snapshot) {
    maintenanceUiAction.store(scorebot::MaintenanceAction::None);
    lastMaintenanceFrame = 0xffffffff;
    lastMaintenanceLightColor = 0xffffffff;
    preparePrinterUi();
    const bool queued = printer.enqueue(snapshot);
    if (!queued && !printer.uiActive()) {
        finishPrinterUi();
    }
    return queued;
}

bool Coordinator::printerUiActive() const {
    return printer.uiActive();
}

bool Coordinator::consumePrinterUiInput() {
    if (!printer.uiActive()) {
        return false;
    }
    if (printer.dismissTerminal()) {
        finishPrinterUi();
    }
    return true;
}

bool Coordinator::normalUiWritesAllowed() const {
    return !otaUiActive.load(std::memory_order_acquire) &&
           !restartRequested.load(std::memory_order_acquire) &&
           !maintenanceModeActive() && !printer.uiActive();
}

void Coordinator::enqueueInputFromISR(ButtonName buttonName) {
    static_assert(std::atomic<uint8_t>::is_always_lock_free,
                  "input-event coalescing must remain ISR-safe");
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "input interrupt epochs must remain ISR-safe");
    const uint8_t index = inputIndex(buttonName);
    const uint8_t bit = inputBit(buttonName);
    inputInterruptEpoch[index].fetch_add(1, std::memory_order_release);
    cancelSleepIntent();
    const uint8_t previous = queuedInputEvents.fetch_or(
        bit, std::memory_order_acq_rel);
    if ((previous & bit) != 0) {
        // One event for this device is queued or being consumed. The epoch
        // increment above guarantees a follow-up dispatch if this edge arrived
        // after that event began its I2C transaction.
        return;
    }

    Event event{};
    event.type = EventType::ButtonPressed;
    event.press.buttonName = buttonName;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    outstandingEvents.fetch_add(1, std::memory_order_acq_rel);
    if (xQueueSendFromISR(
            eventQueue, &event, &higherPriorityTaskWoken) != pdPASS) {
        outstandingEvents.fetch_sub(1, std::memory_order_acq_rel);
        queuedInputEvents.fetch_and(
            static_cast<uint8_t>(~bit), std::memory_order_release);
        // Keep the unmatched interrupt epoch. The application loop retries it
        // in task context even though the hardware line remains latched low.
        return;
    }
    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void Coordinator::discardInput(ButtonName buttonName) {
    if (buttonName == ButtonName::GPIOButtons) {
        (void)buttonGrid.consumeInterrupt();
        return;
    }
    (void)rotaryEncoder.pressed();
    (void)rotaryEncoder.delta();
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
    const scorebot::MaintenanceAction maintenance = maintenanceUiAction.load();
    if (maintenance != scorebot::MaintenanceAction::None) {
        const uint32_t elapsed = now - maintenanceUiStartedMs.load();
        const uint32_t frameIndex = maintenance == scorebot::MaintenanceAction::Ota
                                        ? 0
                                        : elapsed / scorebot::kMaintenanceScrollStepMs;
        if (frameIndex != lastMaintenanceFrame) {
            const auto frame = scorebot::maintenanceDisplayFrame(maintenance, elapsed);
            display1.print(frame.data());
            if (myRole() == BoardRole::Leader) {
                display2.print(frame.data());
                display3.print(frame.data());
                display4.print(frame.data());
            }
            lastMaintenanceFrame = frameIndex;
        }
        const uint32_t baseColor =
            maintenance == scorebot::MaintenanceAction::Reset
                ? 0xff0000
                : maintenance == scorebot::MaintenanceAction::Print
                      ? scorebot::kPrintMaintenanceColor
                      : scorebot::kOtaPurple;
        const uint32_t color = scorebot::maintenanceLightOn(elapsed) ? baseColor : 0;
        if (color != lastMaintenanceLightColor) {
            rotaryEncoder.setColor(color);
            lastMaintenanceLightColor = color;
        }
        return;
    }
    if (updatePrinterUi(now)) {
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
        const int8_t selected = leaderboardSelectedDisplay.load();
        if (selected >= 0 && selected < 4) {
            const std::array<HT16Display*, 4> displays = {
                &display1, &display2, &display3, &display4};
            if (leaderboardLobbySelection.load()) {
                if (leaderboardLocalPrompt.load()) {
                    const uint32_t elapsed =
                        now - leaderboardSelectionStartedMs.load();
                    const uint32_t frame =
                        elapsed / scorebot::kLocalPromptScrollStepMs;
                    if (frame != lastLeaderboardPromptFrame) {
                        const auto text =
                            scorebot::leaderboardLocalPromptFrame(elapsed);
                        displays[static_cast<size_t>(selected)]->print(text.data());
                        lastLeaderboardPromptFrame = frame;
                    }
                }
            } else {
                const scorebot::TurnPulse pulse = scorebot::turnPulseAt(
                    now - leaderboardSelectionStartedMs.load());
                if (pulse.segmentBrightness != lastSelectedSegmentBrightness) {
                    displays[static_cast<size_t>(selected)]->setBrightnessNow(
                        pulse.segmentBrightness);
                    lastSelectedSegmentBrightness = pulse.segmentBrightness;
                }
            }
        }
        const uint32_t baseColor = leaderboardTurnColor.load();
        const uint8_t referenceBrightness =
            selected == 0 ? display2.currentBrightness() : display1.currentBrightness();
        const uint32_t color = baseColor == 0
                                   ? 0
                                   : scorebot::scaleRgb(
                                         baseColor,
                                         scorebot::leaderboardLightLevel(
                                             referenceBrightness));
        if (color != lastLeaderboardLightColor) {
            rotaryEncoder.setColor(color);
            lastLeaderboardLightColor = color;
        }
    }
}

bool Coordinator::updatePrinterUi(uint32_t now) {
    const scorebot::PrinterProgress progress = printer.progress();
    if (progress == scorebot::PrinterProgress::Idle) {
        return false;
    }
    const uint32_t startedMs = printer.progressStartedMs();
    if (scorebot::printerTerminalExpired(progress, startedMs, now) &&
        !inputPending() &&
        outstandingEvents.load(std::memory_order_acquire) == 0) {
        (void)printer.dismissTerminal();
        finishPrinterUi();
        return false;
    }

    const scorebot::PrinterError error = printer.error();
    const uint32_t elapsed = scorebot::printerElapsedMs(now, startedMs);
    const uint32_t frame = progress == scorebot::PrinterProgress::Error
                               ? elapsed / scorebot::kPrinterScrollStepMs
                               : 0;
    const uint8_t progressValue = static_cast<uint8_t>(progress);
    const uint8_t errorValue = static_cast<uint8_t>(error);
    if (progressValue != lastPrinterProgress ||
        errorValue != lastPrinterError || frame != lastPrinterFrame) {
        const auto text = progress == scorebot::PrinterProgress::Error
                              ? scorebot::printerErrorFrame(error, elapsed)
                              : scorebot::printerProgressFrame(progress, elapsed);
        display1.print(text.data());
        display2.print(text.data());
        display3.print(text.data());
        display4.print(text.data());
        lastPrinterProgress = progressValue;
        lastPrinterError = errorValue;
        lastPrinterFrame = frame;
    }

    const uint32_t color = progress == scorebot::PrinterProgress::Done
                               ? 0x00ff00
                               : progress == scorebot::PrinterProgress::Error
                                     ? 0xff2000
                                     : scorebot::kPrintMaintenanceColor;
    if (color != lastPrinterLightColor) {
        rotaryEncoder.setColor(color);
        lastPrinterLightColor = color;
    }
    return true;
}

void Coordinator::preparePrinterUi() {
    setPlayerTurnAnimation(false);
    lastPrinterProgress = 0xff;
    lastPrinterError = 0xff;
    lastPrinterFrame = 0xffffffff;
    lastPrinterLightColor = 0xffffffff;
    display1.setBrightnessNow(display_brightness::kActiveBrightness);
    display1.setBlinkRate(0.0f);
    if (myRole() == BoardRole::Leader) {
        display2.setBrightnessNow(display_brightness::kActiveBrightness);
        display3.setBrightnessNow(display_brightness::kActiveBrightness);
        display4.setBrightnessNow(display_brightness::kActiveBrightness);
        display2.setBlinkRate(0.0f);
        display3.setBlinkRate(0.0f);
        display4.setBlinkRate(0.0f);
    }
}

void Coordinator::finishPrinterUi() {
    lastPrinterProgress = 0xff;
    lastPrinterError = 0xff;
    lastPrinterFrame = 0xffffffff;
    lastPrinterLightColor = 0xffffffff;
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

void Coordinator::showOtaUi() {
    setPlayerTurnAnimation(false);
    setLeaderboardTurnColor(0);
    display1.setBrightnessNow(display_brightness::kActiveBrightness);
    display1.print("OTA ");
    if (myRole() == BoardRole::Leader) {
        display1.setBlinkRate(0.0f);
        display2.setBlinkRate(0.0f);
        display3.setBlinkRate(0.0f);
        display4.setBlinkRate(0.0f);
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
