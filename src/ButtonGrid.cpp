#include <ButtonGrid.hpp>
#include <Coordinator.hpp>
#include <Event.hpp>
#include "hardware/MCP23017Hardware.hpp"
#include "EventPublisher.hpp"

void IRAM_ATTR buttonISR(void* arg) {
    const auto* self = static_cast<ButtonGrid*>(arg);
    Event event{};
    event.type = EventType::ButtonPressed;
    self->getEventPublisher()->publishEventFromISR(event);
}

ButtonGrid::ButtonGrid(IButtonHardware* hardware, IEventPublisher* eventPublisher, uint8_t interruptPin)
    : hardware(hardware), eventPublisher(eventPublisher), interruptPin(interruptPin) {}

// Legacy constructor for backward compatibility
ButtonGrid::ButtonGrid(Coordinator* coordinator) 
    : hardware(new MCP23017Hardware(&Wire, 0x20)), 
      eventPublisher(new EventPublisher(coordinator->eventQueue)),
      interruptPin(static_cast<uint8_t>(Pins::Interrupt)) {}

void ButtonGrid::setup() {
    hardware->begin();
    hardware->setupInterrupts(true, false, LOW);
    
    for (auto&& pin : AllPins) {
        hardware->setPinMode(hardwarePin(pin), INPUT_PULLUP);
        hardware->setupInterruptPin(hardwarePin(pin), CHANGE);
    }

    pinMode(interruptPin, INPUT_PULLUP);
    attachInterruptArg(
        digitalPinToInterrupt(interruptPin),
        buttonISR,
        this,
        FALLING
    );
    hardware->clearInterrupts();
}
