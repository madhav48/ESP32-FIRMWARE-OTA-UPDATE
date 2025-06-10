#pragma once

#include <string>

inline const char *TAG_OTA_FLASH_UPDATE = "[OTAUpdate:FlashUpdater]";


class FlashUpdater {
public:
    FlashUpdater() = default;

    // Flash firmware from a file path (e.g., /spiffs/firmware.bin)
    bool flashFirmwareFromFile(const std::string& firmwarePath);
};
