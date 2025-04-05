
#include <Arduino.h>
#include <WiFi.h>
#include <utils.hpp>

#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define OTA_URL "http://192.168.1.109:8000/firmware.bin"
// put this in repo root, it's a symlink.
#include "secret.h"


#define OTA_BUF_SIZE 256
static const char* TAG = "esp_http_ota";


// https://github.com/henri98/esp32-e-paper-weatherdisplay/blob/master/components/esp_http_ota/src/esp_http_ota.c

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

esp_err_t esp_http_ota(const esp_http_client_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "esp_http_client config not found");
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_handle_t client = esp_http_client_init(config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return err;
    }
    esp_http_client_fetch_headers(client);

    esp_ota_handle_t update_handle = 0;
    const esp_partition_t* update_partition = NULL;
    ESP_LOGI(TAG, "Starting OTA...");
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        http_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
        update_partition->subtype, update_partition->address);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        http_cleanup(client);
        return err;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    ESP_LOGI(TAG, "Please Wait. This may take time");

    esp_err_t ota_write_err = ESP_OK;
    char* upgrade_data_buf = (char*)malloc(OTA_BUF_SIZE);
    if (!upgrade_data_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");
        return ESP_ERR_NO_MEM;
    }
    int binary_file_len = 0;
    while (1) {
        int data_read = esp_http_client_read(client, upgrade_data_buf, OTA_BUF_SIZE);
        if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
        }
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            break;
        }
        if (data_read > 0) {
            ota_write_err = esp_ota_write(update_handle, (const void*)upgrade_data_buf, data_read);
            if (ota_write_err != ESP_OK) {
                break;
            }
            binary_file_len += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_len);
        }
    }
    free(upgrade_data_buf);
    http_cleanup(client);
    ESP_LOGD(TAG, "Total binary data length writen: %d", binary_file_len);

    esp_err_t ota_end_err = esp_ota_end(update_handle);
    if (ota_write_err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%d", err);
        return ota_write_err;
    } else if (ota_end_err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_end failed! err=0x%d. Image is invalid", ota_end_err);
        return ota_end_err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);
        return err;
    }
    ESP_LOGI(TAG, "esp_ota_set_boot_partition succeeded");

    return ESP_OK;
}

void performOTAUpdate() {
    Serial.println("OTA Update");
    delay(500);
    WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .timeout_ms = 5000,
        .skip_cert_common_name_check = true,  // Only for HTTP or self-signed
    };


    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE("OTA", "Failed to init HTTP client");
        return;
    }

    // Make a HEAD request to check if file is present
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    esp_http_client_fetch_headers(client);

    int status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);

    if (status == 200) {
        ESP_LOGI("OTA", "Firmware found, starting OTA update...");
        config.skip_cert_common_name_check = true;  // Optional if using plain HTTP

        esp_err_t ota_err = esp_http_ota(&config);
        if (ota_err == ESP_OK) {
            ESP_LOGI("OTA", "Update successful. Rebooting...");
            esp_restart();
        } else {
            ESP_LOGE("OTA", "OTA failed: %s", esp_err_to_name(ota_err));
        }
    } else {
        ESP_LOGI("OTA", "No update available (url %s, HTTP %d)", config.url, status);
    }

    esp_http_client_cleanup(client);
}
