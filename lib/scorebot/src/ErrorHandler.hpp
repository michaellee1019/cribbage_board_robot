#pragma once

#include "platform.hpp"

static const char* ERROR_TAG = "SCOREBOT";

enum class ErrorCode {
    QUEUE_CREATE_FAILED,
    TASK_CREATE_FAILED,
    HARDWARE_INIT_FAILED,
    WIFI_INIT_FAILED,
    MEMORY_ALLOCATION_FAILED
};

class ErrorHandler {
public:
    static void handleFatal(ErrorCode code, const char* context = nullptr) {
        const char* message = getErrorMessage(code);
        
        ESP_LOGE(ERROR_TAG, "FATAL ERROR: %s", message);
        if (context) {
            ESP_LOGE(ERROR_TAG, "Context: %s", context);
        }
        
        ESP_LOGE(ERROR_TAG, "System will restart in 5 seconds...");
        
        delay(5000);
        esp_restart();
    }
    
    static bool checkFreeRTOSResult(BaseType_t result, ErrorCode errorCode, const char* context = nullptr) {
        if (result != pdPASS) {
#ifdef NATIVE_BUILD
            // For testing, just log and return false instead of restarting
            const char* message = getErrorMessage(errorCode);
            ESP_LOGE(ERROR_TAG, "FATAL ERROR: %s", message);
            if (context) {
                ESP_LOGE(ERROR_TAG, "Context: %s", context);
            }
#else
            handleFatal(errorCode, context);
#endif
            return false;
        }
        return true;
    }
    
    static bool checkPointer(void* ptr, ErrorCode errorCode, const char* context = nullptr) {
        if (ptr == nullptr) {
#ifdef NATIVE_BUILD
            // For testing, just log and return false instead of restarting
            const char* message = getErrorMessage(errorCode);
            ESP_LOGE(ERROR_TAG, "FATAL ERROR: %s", message);
            if (context) {
                ESP_LOGE(ERROR_TAG, "Context: %s", context);
            }
#else
            handleFatal(errorCode, context);
#endif
            return false;
        }
        return true;
    }

    static const char* getErrorMessage(ErrorCode code) {
        switch (code) {
            case ErrorCode::QUEUE_CREATE_FAILED:
                return "Failed to create FreeRTOS queue";
            case ErrorCode::TASK_CREATE_FAILED:
                return "Failed to create FreeRTOS task";
            case ErrorCode::HARDWARE_INIT_FAILED:
                return "Hardware initialization failed";
            case ErrorCode::WIFI_INIT_FAILED:
                return "WiFi initialization failed";
            case ErrorCode::MEMORY_ALLOCATION_FAILED:
                return "Memory allocation failed";
            default:
                return "Unknown error";
        }
    }
};

#define CHECK_FREERTOS_RESULT(result, errorCode, context) \
    ErrorHandler::checkFreeRTOSResult(result, errorCode, context)

#define CHECK_POINTER(ptr, errorCode, context) \
    ErrorHandler::checkPointer(ptr, errorCode, context)

#define FATAL_ERROR(errorCode, context) \
    ErrorHandler::handleFatal(errorCode, context)
