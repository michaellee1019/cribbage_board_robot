#include <HT16Display.hpp>
#include <ErrorHandler.hpp>

HT16Display::HT16Display() = default;

void HT16Display::setup(uint8_t address) {
    const int maxRetries = 10;
    const int delayMs = 500;
    
    for (int retry = 0; retry < maxRetries; retry++) {
        if (driver.begin(address)) {
            return;
        }
        delay(delayMs);
    }
    
    FATAL_ERROR(ErrorCode::DISPLAY_INIT_FAILED, "HT16Display initialization failed after retries");
}
void HT16Display::clear() {
    driver.clear();
}