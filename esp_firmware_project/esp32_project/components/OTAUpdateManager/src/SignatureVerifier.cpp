#include "OTAUpdateManager/SignatureVerifier.h"
#include "Common/certificates.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>

#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "esp_log.h"


SignatureVerifier::SignatureVerifier() {}

static std::string toLowerHex(const std::string &hexIn)
{
    std::string out = hexIn;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return out;
}

static std::string hashToHex(const uint8_t *hash, size_t len = 32)
{
    if (len > 32)
        len = 32;
    char hex[65] = {0};
    for (size_t i = 0; i < len; ++i)
        sprintf(hex + (i * 2), "%02x", hash[i]);
    return std::string(hex);
}


bool SignatureVerifier::verify(const esp_partition_t *partition,
                               uint32_t firmwareSize,
                               const std::vector<uint8_t> &signature,
                               const std::string &expectedChecksum)
{
    if (!partition || firmwareSize == 0 || signature.empty())
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Invalid input to verifier.");
        return false;
    }

    // --- Read firmware from flash and compute SHA256 ---
    const mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
    if (!md_info)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to get SHA256 info");
        return false;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, md_info, 0) != 0)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "mbedtls_md_setup failed.");
        mbedtls_md_free(&ctx);
        return false;
    }

    mbedtls_md_starts(&ctx);

    const size_t chunkSize = 1024;
    uint8_t buffer[chunkSize];
    size_t totalRead = 0;

    while (totalRead < firmwareSize)
    {
        size_t toRead = std::min((uint32_t)chunkSize, firmwareSize - totalRead);
        if (esp_partition_read(partition, totalRead, buffer, toRead) != ESP_OK)
        {
            ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to read from flash at offset %d", (int)totalRead);
            mbedtls_md_free(&ctx);
            return false;
        }
        mbedtls_md_update(&ctx, buffer, toRead);
        totalRead += toRead;
    }

    // --- Checksum Verification ---
    uint8_t hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    std::string computedHex = hashToHex(hash);
    std::string lowercaseExpected = toLowerHex(expectedChecksum);

    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Expected SHA-256 : %s", lowercaseExpected.c_str());
    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Computed SHA-256 : %s", computedHex.c_str());

    if (computedHex != lowercaseExpected)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Checksum mismatch!");
        return false;
    }

    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Checksum matched successfully.");

    // --- Signature Verification ---
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(
        &pk,
        reinterpret_cast<const uint8_t *>(FIRMWARE_SIGN_KEY),
        strlen(FIRMWARE_SIGN_KEY) + 1);
    if (ret != 0)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to parse public key. mbedtls_pk_parse_public_key returned %d", ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
                            hash, sizeof(hash),
                            signature.data(), signature.size());

    mbedtls_pk_free(&pk);

    if (ret != 0)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Digital signature verification failed. mbedtls_pk_verify returned %d", ret);
        return false;
    }

    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Digital signature verified successfully.");
    return true;
}
