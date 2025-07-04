#include "OTAUpdateManager/OTAUpdateManager.h"
#include "esp_log.h"
#include <ArduinoJson.h>
#include <sys/stat.h>
#include <string.h>
#include <sstream>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include <inttypes.h>

void ota_update_task(void *param)
{
    ota_task_params_t *taskParams = static_cast<ota_task_params_t *>(param);
    ESP_LOGI(TAG_OTA_UPDATE, "Starting OTA update task...");

    OtaUpdateManager otaUpdateManager;
    bool update_successful = otaUpdateManager.handleUpdateRequest(taskParams->payload);

    delete taskParams;

    if (update_successful)
    {
        ESP_LOGI(TAG_OTA_UPDATE, "OTA update successful.");

        ESP_LOGI(TAG_OTA_UPDATE, " Restarting device... 3");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG_OTA_UPDATE, " Restarting device... 2");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG_OTA_UPDATE, " Restarting device... 1");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    vTaskDelete(NULL);
}

OtaUpdateManager::OtaUpdateManager()
    : nvsStorageHandler("nvs", "firmware")
{

    nvsStorageHandler.begin();
    currentVersion = nvsStorageHandler.getFirmwareVersion("1.0.0");
    ESP_LOGI(TAG_OTA_UPDATE, "Current firmware version: %s", currentVersion.c_str());
}

const std::string &OtaUpdateManager::getCurrentVersion() const
{
    return currentVersion;
}

bool OtaUpdateManager::handleUpdateRequest(const std::string &payload)
{
    FirmwareMetadata metadata;

    if (!parsePayload(payload, metadata))
    {
        ESP_LOGE(TAG_OTA_UPDATE, "Failed to parse update payload.");
        return false;
    }

    ESP_LOGI(TAG_OTA_UPDATE, "Parsed firmware metadata. Version: %s", metadata.version.c_str());

    if (!isNewVersion(metadata.version))
    {
        ESP_LOGI(TAG_OTA_UPDATE, "No new firmware update available.");
        return false;
    }

    if (performUpdate(metadata))
    {
        ESP_LOGI(TAG_OTA_UPDATE, "Firmware updated successfully to version: %s", metadata.version.c_str());
        return true;
    }
    else
    {
        ESP_LOGE(TAG_OTA_UPDATE, "Firmware update failed.");
        return false;
    }
}

bool OtaUpdateManager::parsePayload(const std::string &jsonStr, FirmwareMetadata &outMeta)
{
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error)
    {
        ESP_LOGE(TAG_OTA_UPDATE, "JSON deserialization failed");
        return false;
    }

    if (!doc["version"].is<const char *>() || !doc["firmware_url"].is<const char *>() ||
        !doc["signature_url"].is<const char *>() || !doc["checksum"].is<const char *>())
    {
        ESP_LOGE(TAG_OTA_UPDATE, "JSON missing required keys");
        return false;
    }

    outMeta.version = doc["version"].as<std::string>();
    outMeta.firmwareUrl = doc["firmware_url"].as<std::string>();
    outMeta.signatureUrl = doc["signature_url"].as<std::string>();
    outMeta.expectedChecksum = doc["checksum"].as<std::string>();

    return true;
}

bool OtaUpdateManager::isNewVersion(const std::string &newVersion)
{
    auto parseVersion = [](const std::string &ver) -> std::vector<int>
    {
        std::vector<int> parts;
        std::stringstream ss(ver);
        std::string item;
        while (std::getline(ss, item, '.'))
        {
            parts.push_back(std::stoi(item));
        }
        return parts;
    };

    std::vector<int> currentParts = parseVersion(currentVersion);
    std::vector<int> newParts = parseVersion(newVersion);

    while (currentParts.size() < 3)
        currentParts.push_back(0);
    while (newParts.size() < 3)
        newParts.push_back(0);

    for (size_t i = 0; i < 3; ++i)
    {
        if (newParts[i] > currentParts[i])
        {
            return true;
        }
        else if (newParts[i] < currentParts[i])
        {
            return false;
        }
    }
    return false;
}

bool OtaUpdateManager::performUpdate(const FirmwareMetadata &meta)
{
    ESP_LOGI(TAG_OTA_UPDATE, "Starting firmware update...");

    HttpDownloader downloader;
    SignatureVerifier verifier;

    const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
    if (!next_partition)
    {
        ESP_LOGE(TAG_OTA_UPDATE, "No OTA partition available");
        return false;
    }

    esp_ota_handle_t ota_handle = 0;
    uint32_t firmwareSize = 0;
    std::vector<uint8_t> signature;

    if (!downloader.downloadToPartition(meta.firmwareUrl,
                                        meta.signatureUrl,
                                        next_partition,
                                        &ota_handle,
                                        &firmwareSize,
                                        signature))
    {
        ESP_LOGE(TAG_OTA_UPDATE, "Download to partition failed");
        return false;
    }

    ESP_LOGI(TAG_OTA_UPDATE, "Download complete: firmware size=%" PRIu32 ", signature size=%zu",
             firmwareSize, signature.size());

    if (esp_ota_end(ota_handle) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_UPDATE, "esp_ota_end failed");
        return false;
    }

    if (!verifier.verify(next_partition, firmwareSize, signature, meta.expectedChecksum))
    {
        ESP_LOGE(TAG_OTA_UPDATE, "Firmware verification failed");
        return false;
    }
    ESP_LOGI(TAG_OTA_UPDATE, "Firmware verified successfully");

    if (esp_ota_set_boot_partition(next_partition) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_UPDATE, "Failed to set boot partition");
        return false;
    }
    ESP_LOGI(TAG_OTA_UPDATE, "Boot partition set to new OTA image");

    nvsStorageHandler.storeFirmwareVersion(meta.version);
    ESP_LOGI(TAG_OTA_UPDATE, "Stored new firmware version: %s", meta.version.c_str());

    return true;
}
