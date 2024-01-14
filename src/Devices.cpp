#include "scorebot/Devices.hpp"
#include "scorebot/PlayerBoard.hpp"
#include "scorebot/ScoreBoard.hpp"

IODevice::IODevice() = default;



// PlayerBoard

PlayerBoard::~PlayerBoard() = default;
void PlayerBoard::setup() {
}

void PlayerBoard::loop() {
}



// ScoreBoard

ScoreBoard::~ScoreBoard() = default;
void ScoreBoard::setup() {
}

void ScoreBoard::loop() {
}


void scorebotSetup(IOConfig config) {
    Serial.begin(9600);
    std::cout << "hello!" << std::endl;

    pinMode(config.pinButton0, INPUT);
    pinMode(config.pinButton1, INPUT);
    pinMode(config.pinButton2, INPUT);
    pinMode(config.pinButton3, INPUT);

    digitalWrite(config.pinButton0, HIGH);
    digitalWrite(config.pinButton1, HIGH);
    digitalWrite(config.pinButton2, HIGH);
    digitalWrite(config.pinButton3, HIGH);
}