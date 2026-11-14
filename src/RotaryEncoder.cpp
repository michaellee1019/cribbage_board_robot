#include <RotaryEncoder.hpp>
#include <Coordinator.hpp>
#include <ErrorHandler.hpp>
#include <I2cBus.hpp>
#include <PlayerBoard.hpp>
#include <utils.hpp>

#include <Adafruit_NeoPixel.h>

#include <cstring>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>
#include <soc/io_mux_reg.h>
#include <soc/spi_periph.h>

namespace {
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
Adafruit_NeoPixel sideLeds(
    scorebot::player_pins::kNeoPixelCount,
    scorebot::player_pins::kNeoPixel,
    NEO_GRB + NEO_KHZ800);

constexpr int8_t kQuadrature[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

uint8_t encoderBitsFromLevel(uint32_t level) {
    const uint8_t a = static_cast<uint8_t>(
        (level >> scorebot::player_pins::kEncoderA) & 1u);
    const uint8_t b = static_cast<uint8_t>(
        (level >> scorebot::player_pins::kEncoderB) & 1u);
    return static_cast<uint8_t>((a << 1) | b);
}

uint8_t encoderBits() {
    return encoderBitsFromLevel(GPIO.in);
}

// The ESP32-S3 RMT channel holds 48 bits, which is only two SK6812 pixels, and
// a CPU bit-bang is stretched by Bluetooth until the color is garbage. SPI
// shifts the whole frame on a hardware clock: 3.2 MHz, four SPI bits per LED
// bit. 1000 is a 0 (312 ns high) and 1100 is a 1 (625 ns high).
constexpr int kLedBitSpiBits = 4;
constexpr int kLedPixelBytes = scorebot::player_pins::kNeoPixelCount * 3;
constexpr int kLedEncodedBytes = kLedPixelBytes * kLedBitSpiBits;
constexpr int kLedLeadBytes = 16;
constexpr int kLedResetBytes = 64;
constexpr int kLedWireBytes = kLedLeadBytes + kLedEncodedBytes + kLedResetBytes;
constexpr spi_host_device_t kLedSpiHost = SPI2_HOST;
// Not bonded out of the XIAO module. The LEDs only see MOSI on GPIO48.
constexpr int kLedSpiClockPin = 14;

spi_device_handle_t ledSpi = nullptr;
alignas(4) uint8_t ledWire[kLedWireBytes];

void attachLedDataPin() {
    static_assert(scorebot::player_pins::kNeoPixel == 48, "side LED data pin");
    const gpio_num_t pin = GPIO_NUM_48;
    // GPIO48 boots as the differential SPI clock. Select the GPIO function,
    // then route SPI MOSI onto it. Boost enable calls pinMode() and undoes this.
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);
    esp_rom_gpio_connect_out_signal(
        pin, spi_periph_signal[kLedSpiHost].spid_out, false, false);
    PIN_FUNC_SELECT(IO_MUX_GPIO48_REG, PIN_FUNC_GPIO);
}

bool ensureLedSpi() {
    if (ledSpi != nullptr) {
        return true;
    }
    spi_bus_config_t bus = {};
    bus.mosi_io_num = scorebot::player_pins::kNeoPixel;
    bus.miso_io_num = -1;
    bus.sclk_io_num = kLedSpiClockPin;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = kLedWireBytes;
    const esp_err_t busError = spi_bus_initialize(kLedSpiHost, &bus, SPI_DMA_CH_AUTO);
    if (busError != ESP_OK) {
        ESP_LOGE("leds", "SPI bus init failed: %s", esp_err_to_name(busError));
        return false;
    }
    spi_device_interface_config_t device = {};
    device.clock_speed_hz = 3200000;
    device.mode = 0;
    device.spics_io_num = -1;
    device.queue_size = 1;
    const esp_err_t deviceError = spi_bus_add_device(kLedSpiHost, &device, &ledSpi);
    if (deviceError != ESP_OK) {
        ESP_LOGE("leds", "SPI device init failed: %s", esp_err_to_name(deviceError));
        ledSpi = nullptr;
        return false;
    }
    return true;
}

void encodeLedByte(uint8_t value, uint8_t* dest) {
    uint32_t packed = 0;
    for (int bit = 7; bit >= 0; --bit) {
        const uint32_t nibble = (value & (1u << bit)) != 0 ? 0xCu : 0x8u;
        packed = (packed << 4) | nibble;
    }
    dest[0] = static_cast<uint8_t>(packed >> 24);
    dest[1] = static_cast<uint8_t>(packed >> 16);
    dest[2] = static_cast<uint8_t>(packed >> 8);
    dest[3] = static_cast<uint8_t>(packed);
}

void paintSideLeds(uint32_t color) {
    if (!ensureLedSpi()) {
        return;
    }
    for (int pixel = 0; pixel < scorebot::player_pins::kNeoPixelCount; ++pixel) {
        sideLeds.setPixelColor(pixel, color);
    }
    const uint8_t* pixels = sideLeds.getPixels();
    const int pixelBytes = sideLeds.numPixels() * 3;
    memset(ledWire, 0, kLedWireBytes);
    uint8_t* dest = ledWire + kLedLeadBytes;
    for (int byteIndex = 0; byteIndex < pixelBytes && byteIndex < kLedPixelBytes; ++byteIndex) {
        encodeLedByte(pixels[byteIndex], dest);
        dest += kLedBitSpiBits;
    }
    // Boost enable takes the data pin back as a plain GPIO. Put SPI on it
    // again before shifting, or the frame never leaves the chip.
    attachLedDataPin();
    spi_transaction_t transaction = {};
    transaction.length = kLedWireBytes * 8;
    transaction.tx_buffer = ledWire;
    const esp_err_t error = spi_device_polling_transmit(ledSpi, &transaction);
    if (error != ESP_OK) {
        ESP_LOGE("leds", "SPI transmit failed: %s", esp_err_to_name(error));
    }
}
}  // namespace

void IRAM_ATTR rotaryEncoderISR(void* arg) {
    auto* self = static_cast<RotaryEncoder*>(arg);
    if (self->directInputs) {
        const uint8_t previous = static_cast<uint8_t>(self->encoderState & 0x03);
        const uint8_t next = encoderBitsFromLevel(GPIO.in);
        self->encoderState = static_cast<uint8_t>((self->encoderState << 2) | next);
        int8_t movement = kQuadrature[self->encoderState & 0x0f];
        // A missed intermediate edge changes both contacts at once. Keep the
        // current direction instead of dropping that part of the detent.
        if (movement == 0 && next != previous && self->lastDirection != 0) {
            movement = static_cast<int8_t>(self->lastDirection * 2);
        }
        const int64_t now = esp_timer_get_time();
        portENTER_CRITICAL_ISR(&encoderMux);
        if (movement != 0) {
            const int8_t direction = movement > 0 ? 1 : -1;
            // Contact bounce is a fast reversal. Ignore it, but keep a real
            // click that simply has uneven edges.
            if (!(direction == -self->lastDirection &&
                  (now - self->lastTransitionUs) < 500)) {
                self->quarterCount =
                    static_cast<int8_t>(self->quarterCount + movement);
                self->lastDirection = direction;
            }
            self->lastTransitionUs = now;
        }
        if (previous == 0x03 && next != 0x03) {
            self->leftRestUs = now;
        }
        // The PEC11R rests with both contacts open. Count one tick per click
        // once it returns there, even if one of the four edges was missed.
        if (next == 0x03 && previous != 0x03 && self->quarterCount != 0) {
            const int8_t quarters = self->quarterCount;
            const bool realClick = (now - self->leftRestUs) >= 1200 ||
                                   quarters >= 2 || quarters <= -2;
            if (realClick) {
                self->detentCount += quarters > 0 ? 1 : -1;
            }
            self->quarterCount = 0;
        }
        portEXIT_CRITICAL_ISR(&encoderMux);
    }
    self->coordinator->enqueueInputFromISR(ButtonName::RotaryEncoder);
}

RotaryEncoder::RotaryEncoder(Coordinator *coordinator)
    : coordinator{coordinator} {}

int32_t RotaryEncoder::delta() {
    if (directInputs) {
        portENTER_CRITICAL(&encoderMux);
        const int32_t value = detentCount;
        detentCount = 0;
        portEXIT_CRITICAL(&encoderMux);
        return value;
    }
    I2cBus::Guard guard;
    return ss.getEncoderDelta();
}

void RotaryEncoder::setup() {
    directInputs = scorebot::usesPlayerBoardHardware();
    if (directInputs) {
        pinMode(scorebot::player_pins::kEncoderA, INPUT_PULLUP);
        pinMode(scorebot::player_pins::kEncoderB, INPUT_PULLUP);
        pinMode(scorebot::player_pins::kEncoderSwitch, INPUT_PULLUP);
        encoderState = encoderBits();
        quarterCount = 0;
        detentCount = 0;
        lastDirection = 0;
        sideLeds.begin();
        paintSideLeds(0);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderA),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderB),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch),
            rotaryEncoderISR, this, CHANGE);
        return;
    }

    {
        I2cBus::Guard guard;
        if (!ss.begin(SEESAW_ADDR)) {
            FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder seesaw initialization failed");
        }
        if (!sspixel.begin(SEESAW_ADDR)) {
            FATAL_ERROR(ErrorCode::ENCODER_INIT_FAILED, "RotaryEncoder pixel initialization failed");
        }

        // https://github.com/adafruit/Adafruit_seesaw/blob/master/examples/digital/gpio_interrupts/gpio_interrupts.ino
        ss.pinMode(SS_SWITCH, INPUT_PULLUP);

        static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;

        pinMode(SEESAW_INTERRUPT, INPUT_PULLUP);
        ss.pinModeBulk(mask, INPUT_PULLUP);  // Probably don't need this with the ss.pinMode above
        ss.setGPIOInterrupts(mask, true);
        ss.enableEncoderInterrupt();

        attachInterruptArg(digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, FALLING);
    }

    // setColor takes the I2C mutex itself. It must run after the setup guard is
    // released or startup deadlocks before the dispatcher and main loop exist.
    this->setColor(0x000000);
}

void RotaryEncoder::setupStatusPixelOnly() {
    if (scorebot::usesPlayerBoardHardware()) {
        directInputs = true;
        sideLeds.begin();
        return;
    }
    I2cBus::Guard guard;
    // Do not reset the seesaw during a timer-only wake: its GPIO/encoder
    // interrupt configuration must survive so physical input can wake us from
    // the next deep-sleep interval.
    if (!sspixel.Adafruit_seesaw::begin(SEESAW_ADDR, -1, false)) {
        return;
    }
    sspixel.updateType(NEO_GRB + NEO_KHZ800);
    sspixel.updateLength(1);
    sspixel.setPin(SS_NEOPIX);
}

void RotaryEncoder::prepareForSleep() {
    if (directInputs) {
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderA));
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderB));
        detachInterrupt(digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch));
        return;
    }
    detachInterrupt(digitalPinToInterrupt(SEESAW_INTERRUPT));
    // Preserve any interrupt latched during the sleep handoff. Its active-low
    // output then causes an immediate EXT1 wake; normal setup clears/reset it.
}

void RotaryEncoder::resumeAfterSleepAbort() {
    if (directInputs) {
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderA),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderB),
            rotaryEncoderISR, this, CHANGE);
        attachInterruptArg(
            digitalPinToInterrupt(scorebot::player_pins::kEncoderSwitch),
            rotaryEncoderISR, this, CHANGE);
        return;
    }
    attachInterruptArg(
        digitalPinToInterrupt(SEESAW_INTERRUPT), rotaryEncoderISR, this, FALLING);
}

bool RotaryEncoder::interruptAsserted() const {
    if (directInputs) {
        return digitalRead(scorebot::player_pins::kEncoderA) == LOW ||
               digitalRead(scorebot::player_pins::kEncoderB) == LOW ||
               digitalRead(scorebot::player_pins::kEncoderSwitch) == LOW;
    }
    return digitalRead(SEESAW_INTERRUPT) == LOW;
}

void RotaryEncoder::setColor(uint32_t color) {
    if (directInputs || scorebot::usesPlayerBoardHardware()) {
        paintSideLeds(color);
        return;
    }
    I2cBus::Guard guard;
    sspixel.setPixelColor(0, color);
    sspixel.show();
}

void RotaryEncoder::reset() {
    if (directInputs) {
        portENTER_CRITICAL(&encoderMux);
        detentCount = 0;
        quarterCount = 0;
        portEXIT_CRITICAL(&encoderMux);
        return;
    }
    I2cBus::Guard guard;
    ss.setEncoderPosition(0);
}

bool RotaryEncoder::pressed() {
    if (directInputs) {
        return digitalRead(scorebot::player_pins::kEncoderSwitch) == LOW;
    }
    I2cBus::Guard guard;
    // Clear the GPIO interrupt flags on the seesaw chip
    static constexpr uint32_t mask = static_cast<uint32_t>(0b1) << SS_SWITCH;
    uint32_t data = ss.digitalReadBulk(mask);  // Reading clears the interrupt flags
    return data == 0; // is 0 or 16777216. 0 is press down, 16777216 is everything else (press up and rotate)
}
