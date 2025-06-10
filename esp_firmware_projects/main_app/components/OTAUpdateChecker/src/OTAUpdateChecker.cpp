#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "ArduinoJson.h"
#include "OTAUpdateChecker/OTAUpdateChecker.h"

static const char *TAG = "[OTAUpdate]";
static const char *NVS_PARTITION = "nvs";
static const char *NVS_NAMESPACE = "firmware";
static const char *NVS_KEY_VERSION = "fm_ver";

// Fetch stored firmware version
static std::string get_stored_version(const std::string &default_version = "1.0.0")
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(NVS_PARTITION, NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to open NVS for read: %s", esp_err_to_name(err));
        return default_version;
    }

    char version[32];
    size_t len = sizeof(version);
    err = nvs_get_str(handle, NVS_KEY_VERSION, version, &len);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        return std::string(version);
    }

    ESP_LOGW(TAG, "Version not found, defaulting to %s", default_version.c_str());
    return default_version;
}

static bool is_new_version(const std::string &current, const std::string &incoming)
{
    int curr_major = 0, curr_minor = 0, curr_patch = 0;
    int inc_major = 0, inc_minor = 0, inc_patch = 0;

    sscanf(current.c_str(), "%d.%d.%d", &curr_major, &curr_minor, &curr_patch);
    sscanf(incoming.c_str(), "%d.%d.%d", &inc_major, &inc_minor, &inc_patch);

    if (inc_major > curr_major)
        return true;
    if (inc_major < curr_major)
        return false;

    if (inc_minor > curr_minor)
        return true;
    if (inc_minor < curr_minor)
        return false;

    return inc_patch > curr_patch;
}

void check_and_update(const std::string &payload)
{

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, payload);

    if (err)
    {
        ESP_LOGE(TAG, "Failed to parse payload JSON: %s", err.c_str());
        return;
    }

    std::string new_version = doc["version"] | "";
    if (new_version.empty())
    {
        ESP_LOGW(TAG, "Version key not found in payload");
        return;
    }

    std::string current_version = get_stored_version();
    ESP_LOGI(TAG, "Current version: %s | Incoming version: %s", current_version.c_str(), new_version.c_str());

    if (!is_new_version(current_version, new_version))
    {
        ESP_LOGI(TAG, "Firmware is up to date. No update needed.");
        return;
    }

    ESP_LOGW(TAG, "New firmware available. Booting the Device...");

    const esp_partition_t *factory_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_FACTORY,
        NULL);

    if (!factory_partition)
    {
        ESP_LOGE(TAG, "No OTA partition available");
        return;
    }

    esp_err_t set_boot = esp_ota_set_boot_partition(factory_partition);
    if (set_boot != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(set_boot));
        return;
    }

    ESP_LOGI(TAG, "Restarting Device...");
    esp_restart();
}
