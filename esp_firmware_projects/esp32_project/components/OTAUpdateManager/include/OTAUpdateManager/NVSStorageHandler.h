#pragma once
#include <string>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

inline const char *TAG_OTA_NVS_STORAGE = "[OTAUpdate:NVSStorageHandler]";


class NVSStorageHandler {
public:
    NVSStorageHandler(const std::string& partitionName, const std::string& namespaceName);

    // Initialize the NVS partition
    bool begin();

    // Get firmware version or return default
    std::string getFirmwareVersion(const std::string& defaultVersion = "0.0.0");

    // Store firmware version
    bool storeFirmwareVersion(const std::string& version);

private:
    std::string partition;
    std::string ns;
};