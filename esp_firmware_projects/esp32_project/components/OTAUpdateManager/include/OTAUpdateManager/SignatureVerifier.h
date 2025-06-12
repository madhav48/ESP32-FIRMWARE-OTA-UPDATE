#pragma once

#include <string>
#include <functional>
#include "esp_log.h"
#include "esp_ota_ops.h"

using LogCallback = std::function<void(const std::string &)>;
inline const char *TAG_SIGNATURE_VERIFIER = "[OTAUpdate:SignatureVerifier]";

class SignatureVerifier
{
public:
    SignatureVerifier();
    bool verify(const esp_partition_t *partition,
                uint32_t firmwareSize,
                const std::vector<uint8_t> &signature,
                const std::string &expectedChecksum);
};
