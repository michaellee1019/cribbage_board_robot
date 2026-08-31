#ifndef COORDINATOR_H
#define COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <array>
#include <cstdint>

#include <GameState.hpp>
#include <DisplayBrightness.hpp>
#include <MyBle.hpp>
#include <MaintenanceRules.hpp>
#include <ButtonGrid.hpp>
#include <RotaryEncoder.hpp>
#include <HT16Display.hpp>
#include <BoardRole.hpp>
#include <UsbConnection.hpp>
#include <Protocol.hpp>
#include <PrinterClient.hpp>

struct StatePersistenceRequest {
    uint32_t generation;
    char snapshot[scorebot::kMaxWireMessageSize + 1];
};

class Coordinator {
private:
    QueueHandle_t eventQueue;
    QueueHandle_t statePersistenceQueue;
    SemaphoreHandle_t stateMutex;
    GameState state;

public:
    HT16Display display1;
    HT16Display display2;
    HT16Display display3;
    HT16Display display4;
    ButtonGrid buttonGrid;
    RotaryEncoder rotaryEncoder;
    MyBle ble;
    UsbConnectionMonitor usbConnection;

    Coordinator();
    void setup();
    void loop();
    void enqueueInputFromISR(ButtonName buttonName);
    void noteInteraction();
    void armOta();
    bool otaModeActive() const;
    void setPlayerTurnAnimation(bool enabled);
    void setLeaderboardTurnColor(uint32_t color);
    void setLeaderboardSelectedDisplay(
        int8_t index, bool lobbySelection, bool localPrompt);
    void startMaintenanceUi(scorebot::MaintenanceAction action, uint32_t startedMs);
    void cancelMaintenanceUi();
    bool maintenanceModeActive() const;
    BoardRole myRole();
    const BoardRoleConfig& myRoleConfig();
    void serviceStateHeartbeat();
    bool enqueueEvent(const Event& event);
    uint32_t enqueueStatePersistence(const String& snapshot);
    bool waitForStatePersistence(uint32_t timeoutMs) const;
    void requestRestart(bool waitForBleDrain);
    void setRestartTransmission(uint32_t generation);
    void finishRestartPreparation();
    bool startPrint(const scorebot::PrinterSnapshot& snapshot);
    bool printerUiActive() const;
    bool consumePrinterUiInput();
    bool normalUiWritesAllowed() const;

    friend void dispatcherTask(void*);
    friend void persistenceTask(void*);

private:
    uint32_t awakeSinceMs{0};
    std::atomic<uint32_t> lastInteractionMs{0};
    std::atomic<bool> playerTurnAnimationActive{false};
    std::atomic<uint32_t> playerTurnAnimationStartedMs{0};
    std::atomic<uint32_t> leaderboardTurnColor{0};
    std::atomic<int8_t> leaderboardSelectedDisplay{-1};
    std::atomic<uint32_t> leaderboardSelectionStartedMs{0};
    std::atomic<bool> leaderboardLobbySelection{false};
    std::atomic<bool> leaderboardLocalPrompt{false};
    std::atomic<scorebot::MaintenanceAction> maintenanceUiAction{
        scorebot::MaintenanceAction::None};
    std::atomic<uint32_t> maintenanceUiStartedMs{0};
    uint8_t lastTurnSegmentBrightness{0xff};
    uint32_t lastTurnLightColor{0xffffffff};
    uint32_t lastLeaderboardLightColor{0xffffffff};
    uint8_t lastSelectedSegmentBrightness{0xff};
    uint32_t lastLeaderboardPromptFrame{0xffffffff};
    uint32_t lastMaintenanceFrame{0xffffffff};
    uint32_t lastMaintenanceLightColor{0xffffffff};
    bool turnAnimationWasActive{false};
    // Start true so the first update applies the idle target after display setup.
    bool displaysAreActive{true};
    enum class SleepTransition : uint8_t {
        Awake,
        Intent,
        Quiescing,
        Committed,
    };
    std::atomic<SleepTransition> sleepTransition{SleepTransition::Awake};
    std::array<std::atomic<uint32_t>, 2> inputInterruptEpoch{};
    std::array<std::atomic<uint32_t>, 2> inputConsumedEpoch{};
    std::atomic<uint8_t> queuedInputEvents{0};
    std::atomic<uint32_t> outstandingEvents{0};
    std::atomic<bool> otaUiActive{false};
    uint32_t lastOtaLightColor{0xffffffff};
    std::atomic<uint32_t> requestedPersistenceGeneration{0};
    std::atomic<uint32_t> persistedGeneration{0};
    std::atomic<bool> restartRequested{false};
    std::atomic<bool> restartPrepared{false};
    std::atomic<bool> restartWaitsForBle{false};
    std::atomic<uint32_t> restartTransmissionGeneration{0};
    uint32_t restartReadyAtMs{0};
    PrinterClient printer;
    uint8_t lastPrinterProgress{0xff};
    uint8_t lastPrinterError{0xff};
    uint32_t lastPrinterFrame{0xffffffff};
    uint32_t lastPrinterLightColor{0xffffffff};

    void updateDisplayBrightness();
    bool updatePrinterUi(uint32_t now);
    void preparePrinterUi();
    void finishPrinterUi();
    void showOtaUi();
    void finishOtaUi();
    bool sleepBlocked() const;
    bool sleepDue();
    bool inputPending() const;
    void cancelSleepIntent();
    void servicePendingInputEvents();
    void recordLatchedInput(ButtonName buttonName);
    bool servicePendingRestart();
    void discardInput(ButtonName buttonName);
    bool statePersistencePending() const;
};

#endif // COORDINATOR_H
