#include <Arduino.h>
#include <Wire.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <cmath>
#include <cstring>

namespace {
constexpr uint8_t kBatteryPin = 1;
constexpr uint8_t kUsbPin = 2;
constexpr uint8_t kBoostPin = 47;
constexpr uint8_t kPixelPin = 48;
constexpr uint8_t kDisplayAddress = 0x70;
constexpr unsigned kSampleCount = 64;
esp_adc_cal_characteristics_t calibration{};
rmt_obj_t* pixels = nullptr;
const char* boostMode = "float";
const char* loadMode = "off";
bool streaming = true;
bool displayFound = false;
int calibrationSource = ESP_ADC_CAL_VAL_NOT_SUPPORTED;

struct Reading {
    double raw = 0;
    double mv = 0;
    double stddev = 0;
    unsigned minimum = 4095;
    unsigned maximum = 0;
    unsigned saturated = 0;
};

Reading readAdc(adc1_channel_t channel) {
    Reading result;
    double sumSquared = 0;
    // The dividers have 50/60 kohm Thevenin resistance. Discard conversions
    // after switching channels; BAT_SENSE also has a 100 nF reservoir capacitor.
    for (unsigned i = 0; i < 8; ++i) {
        adc1_get_raw(channel);
        delayMicroseconds(250);
    }
    for (unsigned i = 0; i < kSampleCount; ++i) {
        const unsigned raw = adc1_get_raw(channel);
        result.raw += raw;
        result.mv += esp_adc_cal_raw_to_voltage(raw, &calibration);
        sumSquared += static_cast<double>(raw) * raw;
        result.minimum = min(result.minimum, raw);
        result.maximum = max(result.maximum, raw);
        result.saturated += raw >= 4080;
        delayMicroseconds(500);
    }
    result.raw /= kSampleCount;
    result.mv /= kSampleCount;
    result.stddev = std::sqrt(max(0.0, sumSquared / kSampleCount - result.raw * result.raw));
    return result;
}

bool displayCommand(uint8_t command) {
    Wire.beginTransmission(kDisplayAddress);
    Wire.write(command);
    return Wire.endTransmission() == 0;
}

bool displayAck() {
    Wire.beginTransmission(kDisplayAddress);
    return Wire.endTransmission() == 0;
}

bool setPixels(uint8_t white) {
    if (pixels == nullptr) return false;
    // Eight SK6812SIDE RGB LEDs, GRB order, with an 80+ us reset interval.
    rmt_data_t data[8 * 24]{};
    for (unsigned i = 0; i < 8 * 24; ++i) {
        const bool one = (white & (0x80 >> (i % 8))) != 0;
        data[i].level0 = 1;
        data[i].duration0 = one ? 6 : 3;
        data[i].level1 = 0;
        data[i].duration1 = one ? 6 : 9;
    }
    const bool ok = rmtWrite(pixels, data, 8 * 24);
    delay(2);
    return ok;
}

void setLoad(const char* mode) {
    loadMode = mode;
    if (std::strcmp(mode, "off") == 0) {
        setPixels(0);
        displayCommand(0x80);
        displayCommand(0x20);
        return;
    }
    const bool bright = std::strcmp(mode, "bright") == 0;
    setPixels(bright ? 32 : 4);  // Deliberately avoid full-current RGB white.
    displayCommand(0x21);
    displayCommand(bright ? 0xef : 0xe1);
    Wire.beginTransmission(kDisplayAddress);
    Wire.write(0x00);
    for (unsigned i = 0; i < 16; ++i) Wire.write(0xff);
    Wire.endTransmission();
    displayCommand(0x81);
}

void setBoost(const char* mode) {
    boostMode = mode;
    if (std::strcmp(mode, "float") == 0) {
        pinMode(kBoostPin, INPUT);
    } else {
        // MT3608 EN is ACTIVE HIGH despite the schematic's 5V_ENN name.
        digitalWrite(kBoostPin, std::strcmp(mode, "high") == 0 ? HIGH : LOW);
        pinMode(kBoostPin, OUTPUT);
    }
}

void sample(const char* stage) {
    const Reading battery = readAdc(ADC1_CHANNEL_0);
    const Reading usb = readAdc(ADC1_CHANNEL_1);
    const bool ack = displayAck();
    Serial.printf(
        "{\"type\":\"sample\",\"stage\":\"%s\",\"ms\":%lu,"
        "\"boost\":\"%s\",\"enable_read\":%d,\"load\":\"%s\",\"display_ack\":%s,"
        "\"bat_raw\":%.2f,\"bat_raw_std\":%.2f,\"bat_raw_min\":%u,\"bat_raw_max\":%u,"
        "\"bat_adc_mv\":%.2f,\"bat_mv\":%.2f,\"bat_saturated\":%u,"
        "\"usb_raw\":%.2f,\"usb_raw_std\":%.2f,\"usb_raw_min\":%u,\"usb_raw_max\":%u,"
        "\"usb_adc_mv\":%.2f,\"vbus_mv\":%.2f,\"usb_saturated\":%u}\n",
        stage, static_cast<unsigned long>(millis()), boostMode,
        digitalRead(kBoostPin), loadMode, ack ? "true" : "false",
        battery.raw, battery.stddev, battery.minimum, battery.maximum,
        battery.mv, battery.mv * 2.0, battery.saturated,
        usb.raw, usb.stddev, usb.minimum, usb.maximum,
        usb.mv, usb.mv * (5.0 / 3.0), usb.saturated);
}

void stage(const char* label, const char* enable, const char* load) {
    setBoost(enable);
    delay(200);
    setLoad(load);
    delay(500);
    for (unsigned i = 0; i < 4; ++i) {
        sample(label);
        delay(250);
    }
}

void runSequence() {
    Serial.println("{\"type\":\"sequence_begin\"}");
    stage("float_off", "float", "off");
    stage("high_off", "high", "off");
    stage("low_off", "low", "off");
    stage("high_dim", "high", "dim");
    stage("high_bright", "high", "bright");
    stage("low_bright", "low", "bright");
    stage("high_recovery", "high", "bright");
    for (unsigned i = 0; i < 5; ++i) {
        stage("cycle_low", "low", "dim");
        stage("cycle_high", "high", "dim");
    }
    stage("final_float_off", "float", "off");
    Serial.println("{\"type\":\"sequence_end\"}");
}

void printInfo() {
    const uint64_t mac = ESP.getEfuseMac();
    Serial.printf(
        "{\"type\":\"ready\",\"probe\":\"power-v1\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\","
        "\"reset_reason\":%d,\"flash_bytes\":%u,\"adc_cal_source\":%d,"
        "\"battery_pin\":1,\"usb_pin\":2,\"boost_pin\":47,\"sda\":17,\"scl\":18,"
        "\"display_found\":%s,\"pixel_driver\":%s}\n",
        static_cast<unsigned>(mac & 0xff), static_cast<unsigned>((mac >> 8) & 0xff),
        static_cast<unsigned>((mac >> 16) & 0xff), static_cast<unsigned>((mac >> 24) & 0xff),
        static_cast<unsigned>((mac >> 32) & 0xff), static_cast<unsigned>((mac >> 40) & 0xff),
        static_cast<int>(esp_reset_reason()), ESP.getFlashChipSize(), calibrationSource,
        displayFound ? "true" : "false", pixels != nullptr ? "true" : "false");
}

void command(const char* input) {
    if (std::strcmp(input, "run") == 0) runSequence();
    else if (std::strcmp(input, "info") == 0) printInfo();
    else if (std::strcmp(input, "status") == 0) sample("manual");
    else if (std::strcmp(input, "quiet") == 0) streaming = false;
    else if (std::strcmp(input, "stream") == 0) streaming = true;
    else if (std::strcmp(input, "high") == 0) setBoost("high");
    else if (std::strcmp(input, "low") == 0) setBoost("low");
    else if (std::strcmp(input, "float") == 0) setBoost("float");
    else if (std::strcmp(input, "off") == 0) setLoad("off");
    else if (std::strcmp(input, "dim") == 0) setLoad("dim");
    else if (std::strcmp(input, "bright") == 0) setLoad("bright");
    else Serial.println("{\"type\":\"error\",\"message\":\"unknown command\"}");
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);
    pinMode(kBatteryPin, INPUT);
    pinMode(kUsbPin, INPUT);
    pinMode(kBoostPin, INPUT);  // Preserve the external 10 kohm EN pulldown.
    for (uint8_t pin = 4; pin <= 11; ++pin) pinMode(pin, INPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_11);
    calibrationSource = static_cast<int>(esp_adc_cal_characterize(
        ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &calibration));
    Wire.begin(17, 18, 100000);
    Wire.setTimeOut(30);
    displayFound = displayAck();
    pixels = rmtInit(kPixelPin, RMT_TX_MODE, RMT_MEM_64);
    if (pixels != nullptr) rmtSetTick(pixels, 100);
    setLoad("off");
    printInfo();
}

void loop() {
    static char buffer[40]{};
    static unsigned length = 0;
    static uint32_t lastSample = 0;
    while (Serial.available()) {
        const char character = Serial.read();
        if (character == '\n') {
            buffer[length] = '\0';
            command(buffer);
            length = 0;
        } else if (character != '\r' && length < sizeof(buffer) - 1) {
            buffer[length++] = character;
        }
    }
    if (streaming && millis() - lastSample >= 1000) {
        lastSample = millis();
        sample("idle");
    }
    delay(5);
}
