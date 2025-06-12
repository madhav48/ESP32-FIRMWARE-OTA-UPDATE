#pragma once

#include <string>
#include <functional>
#include "HttpDownloader.h"
#include "SignatureVerifier.h"
#include "NVSStorageHandler.h"
#include "esp_log.h"

inline const char *TAG_OTA_UPDATE = "[OTAUpdate]";

typedef struct {
    std::string payload;
} ota_task_params_t;

void ota_update_task(void *param);

class OtaUpdateManager
{
public:
    struct FirmwareMetadata
    {
        std::string version;
        std::string firmwareUrl;
        std::string signatureUrl;
        std::string expectedChecksum;
    };

    using LogCallback = std::function<void(const std::string &)>;

    OtaUpdateManager();

    bool handleUpdateRequest(const std::string &payload);
    const std::string &getCurrentVersion() const;

private:
    std::string currentVersion;
    NVSStorageHandler nvsStorageHandler;

    bool isNewVersion(const std::string &newVersion);
    bool parsePayload(const std::string &json, FirmwareMetadata &outMeta);

    bool performUpdate(const FirmwareMetadata &metadata);
};
