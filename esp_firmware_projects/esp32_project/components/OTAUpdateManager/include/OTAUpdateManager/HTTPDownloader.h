#pragma once

#include <string>
#include <vector>
#include <functional>
#include "esp_log.h"
#include "esp_ota_ops.h"

inline const char *TAG_OTA_HTTP_DOWNLOADER = "[OTAUpdate:HTTPDownloader]";

class HttpDownloader
{
public:
    using LogCallback = std::function<void(const std::string &)>;

    HttpDownloader();

    bool downloadToPartition(const std::string &firmwareUrl,
                             const std::string &signatureUrl,
                             const esp_partition_t *partition,
                             esp_ota_handle_t *otaHandleOut,
                             uint32_t *firmwareSizeOut,
                             std::vector<uint8_t> &signatureOut);
};
