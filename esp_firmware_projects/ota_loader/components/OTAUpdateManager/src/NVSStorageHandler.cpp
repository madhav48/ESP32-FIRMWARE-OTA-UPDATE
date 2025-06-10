#include "OTAUpdateManager/NVSStorageHandler.h"

#define NVS_KEY_VERSION "fm_ver"

NVSStorageHandler::NVSStorageHandler(const std::string &partitionName, const std::string &namespaceName)
    : partition(partitionName), ns(namespaceName) {}

bool NVSStorageHandler::begin()
{
    esp_err_t err = nvs_flash_init_partition(partition.c_str());
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_NVS_STORAGE, "Failed to init NVS partition '%s': %s", partition.c_str(), esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG_OTA_NVS_STORAGE, "Initialized NVS partition '%s'", partition.c_str());
    return true;
}

std::string NVSStorageHandler::getFirmwareVersion(const std::string &defaultVersion)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(partition.c_str(), ns.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_OTA_NVS_STORAGE, "Failed to open NVS partition (%s), using default version", esp_err_to_name(err));
        return defaultVersion;
    }

    char version[32] = {0};
    size_t required_size = sizeof(version);
    err = nvs_get_str(handle, NVS_KEY_VERSION, version, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG_OTA_NVS_STORAGE, "No firmware version stored. Saving default: %s", defaultVersion.c_str());
        err = nvs_set_str(handle, NVS_KEY_VERSION, defaultVersion.c_str());
        if (err == ESP_OK)
        {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG_OTA_NVS_STORAGE, "Failed to commit default version: %s", esp_err_to_name(err));
        }
        return defaultVersion;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_NVS_STORAGE, "Error reading version: %s", esp_err_to_name(err));
        nvs_close(handle);
        return defaultVersion;
    }

    nvs_close(handle);
    ESP_LOGI(TAG_OTA_NVS_STORAGE, "Loaded firmware version from NVS: %s", version);
    return std::string(version);
}

bool NVSStorageHandler::storeFirmwareVersion(const std::string &version)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(partition.c_str(), ns.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_NVS_STORAGE, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, NVS_KEY_VERSION, version.c_str());
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_NVS_STORAGE, "Failed to write version to NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_NVS_STORAGE, "Failed to commit firmware version: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG_OTA_NVS_STORAGE, "Firmware version stored successfully: %s", version.c_str());
    return true;
}
