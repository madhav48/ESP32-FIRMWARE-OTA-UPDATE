#pragma once

#include <string>
#include <functional>
#include "esp_log.h"


using LogCallback = std::function<void(const std::string&)>;
inline const char *TAG_SIGNATURE_VERIFIER = "[OTAUpdate:SignatureVerifier]";


class SignatureVerifier {
public:
    SignatureVerifier();

    bool verify(const std::string& firmwarePath,
                const std::string& signaturePath,
                const std::string& expectedChecksum);

private:
    std::string computeSHA256FromFile(const std::string& filepath);
};
