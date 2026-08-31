#include <PrinterClient.hpp>

#include <PrinterConfig.hpp>
#include <PrinterWifiCredentials.hpp>
#include <utils.hpp>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

#include <array>
#include <algorithm>
#include <cstring>

namespace {
constexpr UBaseType_t kPrinterQueueDepth = 1;
constexpr uint32_t kWifiPollMs = 50;
constexpr uint32_t kWifiShutdownRetryMs = 100;
constexpr size_t kMaxPrinterResponseBodySize = 4096;

class FixedResponseStream final : public Stream {
public:
    size_t write(uint8_t value) override {
        return write(&value, 1);
    }

    size_t write(const uint8_t* values, size_t length) override {
        if (values == nullptr || length == 0) {
            return 0;
        }
        const size_t available = buffer.size() - used;
        const size_t copied = std::min(available, length);
        if (copied != 0) {
            std::memcpy(buffer.data() + used, values, copied);
            used += copied;
        }
        overflow = copied != length;
        return copied;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

    const char* data() const { return buffer.data(); }
    size_t size() const { return used; }
    bool overflowed() const { return overflow; }

private:
    std::array<char, kMaxPrinterResponseBodySize> buffer{};
    size_t used{0};
    bool overflow{false};
};

scorebot::PrinterError transportError(int status) {
    if (status == HTTPC_ERROR_READ_TIMEOUT) {
        return scorebot::PrinterError::PrintTimeout;
    }
    if (status == HTTPC_ERROR_STREAM_WRITE ||
        status == HTTPC_ERROR_TOO_LESS_RAM ||
        status == HTTPC_ERROR_ENCODING) {
        return scorebot::PrinterError::BadResponse;
    }
    return scorebot::PrinterError::NoRoute;
}

bool sha256Hex(
    const char* value, size_t length,
    std::array<char, scorebot::kPrinterSha256HexLength + 1>& out) {
    if (value == nullptr) {
        return false;
    }
    std::array<unsigned char, 32> digest{};
    if (mbedtls_sha256_ret(
            reinterpret_cast<const unsigned char*>(value), length,
            digest.data(), 0) != 0) {
        return false;
    }
    constexpr char hex[] = "0123456789abcdef";
    for (size_t index = 0; index < digest.size(); ++index) {
        out[index * 2] = hex[digest[index] >> 4];
        out[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    out[scorebot::kPrinterSha256HexLength] = '\0';
    return true;
}

bool successfulResponseMatchesKey(
    const char* response, size_t responseLength, const char* idempotencyKey,
    bool* replay) {
    JsonDocument document;
    if (deserializeJson(document, response, responseLength) !=
            DeserializationError::Ok ||
        document["status"] != "sent" ||
        !document["idempotency_key"].is<const char*>() ||
        !document["idempotent_replay"].is<bool>()) {
        return false;
    }
    const bool matches = std::strcmp(
                             document["idempotency_key"].as<const char*>(),
                             idempotencyKey) == 0;
    if (matches && replay != nullptr) {
        *replay = document["idempotent_replay"].as<bool>();
    }
    return matches;
}
}  // namespace

void printerWorkerTask(void* parameter) {
    static_cast<PrinterClient*>(parameter)->workerLoop();
}

PrinterClient::PrinterClient()
    : queue(nullptr),
      workerTask(nullptr),
      currentProgress(scorebot::PrinterProgress::Idle),
      currentError(scorebot::PrinterError::None),
      currentProgressStartedMs(0),
      wifiIsPowered(false),
      enabled(false) {}

PrinterClient::~PrinterClient() {
    if (workerTask != nullptr) {
        vTaskDelete(workerTask);
    }
    if (queue != nullptr) {
        vQueueDelete(queue);
    }
}

bool PrinterClient::setup(bool shouldEnable) {
    enabled = shouldEnable;
    if (!enabled) {
        return true;
    }

    // The smoke-test firmware may have left a persistent station profile.
    // Production credentials are compiled in, so clear that legacy profile
    // and prevent this client from ever reconnecting on its own.
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    if (!powerWifiOff(true)) {
        return false;
    }

    queue = xQueueCreate(kPrinterQueueDepth, sizeof(scorebot::PrinterSnapshot));
    if (queue == nullptr) {
        return false;
    }
    const BaseType_t result = xTaskCreatePinnedToCore(
        printerWorkerTask, "printer-worker", 10240, this, 1, &workerTask,
        ARDUINO_RUNNING_CORE);
    return result == pdPASS;
}

bool PrinterClient::enqueue(const scorebot::PrinterSnapshot& snapshot) {
    if (!enabled || queue == nullptr) {
        return false;
    }
    scorebot::PrinterProgress expected = scorebot::PrinterProgress::Idle;
    if (!currentProgress.compare_exchange_strong(
            expected, scorebot::PrinterProgress::Wifi,
            std::memory_order_acq_rel)) {
        return false;
    }
    currentError.store(scorebot::PrinterError::None, std::memory_order_release);
    currentProgressStartedMs.store(millis(), std::memory_order_release);
    if (xQueueSend(queue, &snapshot, 0) == pdPASS) {
        return true;
    }
    publish(
        scorebot::PrinterProgress::Error,
        scorebot::PrinterError::PrintBusy);
    return false;
}

scorebot::PrinterProgress PrinterClient::progress() const {
    return currentProgress.load(std::memory_order_acquire);
}

scorebot::PrinterError PrinterClient::error() const {
    return currentError.load(std::memory_order_acquire);
}

uint32_t PrinterClient::progressStartedMs() const {
    return currentProgressStartedMs.load(std::memory_order_acquire);
}

bool PrinterClient::uiActive() const {
    return progress() != scorebot::PrinterProgress::Idle;
}

bool PrinterClient::busy() const {
    const scorebot::PrinterProgress value = progress();
    return value == scorebot::PrinterProgress::Wifi ||
           value == scorebot::PrinterProgress::Send;
}

bool PrinterClient::wifiPowered() const {
    return wifiIsPowered.load(std::memory_order_acquire);
}

bool PrinterClient::dismissTerminal() {
    scorebot::PrinterProgress value = progress();
    while (scorebot::printerProgressIsTerminal(value)) {
        if (currentProgress.compare_exchange_weak(
                value, scorebot::PrinterProgress::Idle,
                std::memory_order_acq_rel)) {
            currentError.store(
                scorebot::PrinterError::None, std::memory_order_release);
            currentProgressStartedMs.store(millis(), std::memory_order_release);
            return true;
        }
    }
    return false;
}

void PrinterClient::workerLoop() {
    scorebot::PrinterSnapshot snapshot{};
    while (true) {
        if (xQueueReceive(queue, &snapshot, portMAX_DELAY) == pdPASS) {
            execute(snapshot);
        }
    }
}

void PrinterClient::execute(const scorebot::PrinterSnapshot& snapshot) {
    std::array<char, scorebot::kMaxPrinterRequestBodySize> body{};
    size_t bodyLength = 0;
    std::array<char, scorebot::kPrinterSha256HexLength + 1> digest{};
    std::array<char, scorebot::kMaxPrinterIdempotencyKeySize> key{};
    if (!scorebot::buildPrinterRequest(
            snapshot, body.data(), body.size(), &bodyLength) ||
        !sha256Hex(body.data(), bodyLength, digest) ||
        !scorebot::buildPrinterIdempotencyKey(
            snapshot, digest.data(), key.data(), key.size())) {
        publish(
            scorebot::PrinterProgress::Error,
            scorebot::PrinterError::BadResponse);
        return;
    }

    scorebot::PrinterError result = scorebot::PrinterError::WifiFail;
    bool success = false;
    bool httpStarted = false;
    HTTPClient http;

    wifiIsPowered.store(true, std::memory_order_release);
    if (WiFi.mode(WIFI_STA)) {
        WiFi.begin(
            scorebot::kPrinterWifiSsid, scorebot::kPrinterWifiPassword);
        const uint32_t wifiStartedMs = millis();
        while (WiFi.status() != WL_CONNECTED &&
               static_cast<uint32_t>(millis() - wifiStartedMs) <
                   scorebot::kPrinterWifiTimeoutMs) {
            if (WiFi.status() == WL_CONNECT_FAILED) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(kWifiPollMs));
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        publish(scorebot::PrinterProgress::Send);
        http.setConnectTimeout(scorebot::kPrinterTcpConnectTimeoutMs);
        http.setTimeout(scorebot::kPrinterHttpTimeoutMs);
        httpStarted = http.begin(scorebot::kPrinterEndpointUrl);
        if (httpStarted) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Idempotency-Key", key.data());
            const int status = http.POST(
                reinterpret_cast<uint8_t*>(body.data()), bodyLength);
            if (status > 0) {
                bool replay = false;
                bool valid = false;
                if (status >= 200 && status <= 299) {
                    const int declaredLength = http.getSize();
                    if (declaredLength <=
                        static_cast<int>(kMaxPrinterResponseBodySize)) {
                        FixedResponseStream response;
                        const int bodyStatus = http.writeToStream(&response);
                        if (bodyStatus >= 0 && !response.overflowed() &&
                            (declaredLength < 0 ||
                             bodyStatus == declaredLength)) {
                            valid = successfulResponseMatchesKey(
                                response.data(), response.size(), key.data(),
                                &replay);
                        } else if (bodyStatus < 0) {
                            result = transportError(bodyStatus);
                        }
                    }
                }
                if (result != scorebot::PrinterError::PrintTimeout &&
                    result != scorebot::PrinterError::NoRoute) {
                    result = scorebot::printerErrorForHttpResponse(
                        status, valid);
                }
                success = result == scorebot::PrinterError::None;
                DEBUG_PRINTF(
                    "Printer: HTTP status=%d key=%s replay=%d\n",
                    status, key.data(), valid && replay);
            } else {
                result = transportError(status);
                DEBUG_PRINTF(
                    "Printer: transport status=%d key=%s\n",
                    status, key.data());
            }
        } else {
            result = scorebot::PrinterError::NoRoute;
        }
    }

    if (httpStarted) {
        http.end();
    }
    while (!powerWifiOff(false)) {
        DEBUG_PRINTLN("Printer: retrying Wi-Fi shutdown");
        vTaskDelay(pdMS_TO_TICKS(kWifiShutdownRetryMs));
    }
    publish(
        success ? scorebot::PrinterProgress::Done
                : scorebot::PrinterProgress::Error,
        success ? scorebot::PrinterError::None : result);
}

void PrinterClient::publish(
    scorebot::PrinterProgress next, scorebot::PrinterError nextError) {
    currentError.store(nextError, std::memory_order_release);
    currentProgressStartedMs.store(millis(), std::memory_order_release);
    currentProgress.store(next, std::memory_order_release);
}

bool PrinterClient::powerWifiOff(bool eraseSavedAssociation) {
    WiFi.setAutoReconnect(false);
    bool erased = true;
    if (eraseSavedAssociation) {
        wifiIsPowered.store(true, std::memory_order_release);
        erased = WiFi.mode(WIFI_STA) && WiFi.eraseAP();
    }

    // disconnect(true) normally disables station mode. The explicit mode call
    // is both a fallback and the state we verify before allowing sleep or a
    // terminal result to claim the transient radio is gone.
    (void)WiFi.disconnect(true, false);
    (void)WiFi.mode(WIFI_OFF);
    const bool poweredOff = WiFi.getMode() == WIFI_MODE_NULL;
    wifiIsPowered.store(!poweredOff, std::memory_order_release);
    DEBUG_PRINTF(
        "Printer: Wi-Fi shutdown off=%d erased=%d\n", poweredOff, erased);
    return erased && poweredOff;
}
