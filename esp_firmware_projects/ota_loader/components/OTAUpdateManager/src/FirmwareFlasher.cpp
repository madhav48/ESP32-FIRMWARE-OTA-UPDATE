#include "OTAUpdateManager/FirmwareFlasher.h"
#include "esp_log.h"
#include <cstdio>
#include "esp_ota_ops.h"

bool FlashUpdater::flashFirmwareFromFile(const std::string &firmwarePath)
{
    FILE *f = fopen(firmwarePath.c_str(), "rb");
    if (!f)
    {
        ESP_LOGE(TAG_OTA_FLASH_UPDATE, "Failed to open firmware file: %s", firmwarePath.c_str());
        return false;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        ESP_LOGE(TAG_OTA_FLASH_UPDATE, "No OTA update partition found.");
        fclose(f);
        return false;
    }

    ESP_LOGI(TAG_OTA_FLASH_UPDATE, "Writing to OTA partition: %s", update_partition->label);

    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_FLASH_UPDATE, "esp_ota_begin failed: 0x%x", err);
        fclose(f);
        return false;
    }

    const size_t buffer_size = 4096;
    uint8_t buffer[buffer_size];
    size_t total_written = 0;
    size_t read_bytes = 0;

    while ((read_bytes = fread(buffer, 1, buffer_size, f)) > 0)
    {
        err = esp_ota_write(update_handle, buffer, read_bytes);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG_OTA_FLASH_UPDATE, "esp_ota_write failed at byte %zu: 0x%x", total_written, err);
            esp_ota_end(update_handle);
            fclose(f);
            return false;
        }
        total_written += read_bytes;
    }

    fclose(f);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_FLASH_UPDATE, "esp_ota_end failed: 0x%x", err);
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_FLASH_UPDATE, "Failed to set new boot partition: 0x%x", err);
        return false;
    }

    ESP_LOGI(TAG_OTA_FLASH_UPDATE, "Firmware flashed successfully. Total bytes: %zu", total_written);
    return true;
}
