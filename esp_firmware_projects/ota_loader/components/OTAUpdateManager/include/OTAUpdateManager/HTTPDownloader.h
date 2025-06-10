#pragma once

#include <string>
#include <vector>
#include <functional>
#include "esp_log.h"


inline const char *TAG_OTA_HTTP_DOWNLOADER = "[OTAUpdate:HTTPDownloader]";


class HttpDownloader {
public:
    using LogCallback = std::function<void(const std::string&)>;

    HttpDownloader();

    bool downloadToFile(const std::string& url, const std::string& path);

};
