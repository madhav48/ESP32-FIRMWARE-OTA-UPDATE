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

static bool computeSHA256RawFromFile(const std::string &filepath, uint8_t hashOut[32])
{
    FILE *f = fopen(filepath.c_str(), "rb");
    if (!f)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to open file for SHA256: %s", filepath.c_str());
        return false;
    }

    const mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
    if (!md_info)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "mbedtls_md_info_from_type failed.");
        fclose(f);
        return false;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, md_info, 0) != 0)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "mbedtls_md_setup failed.");
        fclose(f);
        mbedtls_md_free(&ctx);
        return false;
    }

    mbedtls_md_starts(&ctx);

    uint8_t buffer[1024];
    size_t readBytes;
    while ((readBytes = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        mbedtls_md_update(&ctx, buffer, readBytes);
    }

    fclose(f);
    mbedtls_md_finish(&ctx, hashOut);
    mbedtls_md_free(&ctx);
    return true;
}

std::string SignatureVerifier::computeSHA256FromFile(const std::string &filepath)
{
    uint8_t hash[32];
    if (!computeSHA256RawFromFile(filepath, hash))
    {
        return "";
    }
    return hashToHex(hash);
}

bool SignatureVerifier::verify(const std::string &firmwarePath,
                               const std::string &signaturePath,
                               const std::string &expectedChecksum)
{

    // --- SHA256 Checksum Comparison ---
    uint8_t firmwareHash[32];
    if (!computeSHA256RawFromFile(firmwarePath, firmwareHash))
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Checksum computation failed.");
        return false;
    }

    std::string computedHex = hashToHex(firmwareHash);
    std::string lowercaseExpected = toLowerHex(expectedChecksum);

    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Expected SHA-256 : %s", lowercaseExpected.c_str());
    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Computed SHA-256 : %s", computedHex.c_str());

    if (computedHex != lowercaseExpected)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Checksum mismatch!");
        return false;
    }

    ESP_LOGI(TAG_SIGNATURE_VERIFIER, "Checksum matched successfully.");

    // --- Load Signature File ---
    FILE *sigFile = fopen(signaturePath.c_str(), "rb");
    if (!sigFile)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to open signature file: %s", signaturePath.c_str());
        return false;
    }
    fseek(sigFile, 0, SEEK_END);
    size_t sigSize = ftell(sigFile);
    fseek(sigFile, 0, SEEK_SET);

    std::vector<uint8_t> signature(sigSize);
    if (fread(signature.data(), 1, sigSize, sigFile) != sigSize)
    {
        ESP_LOGE(TAG_SIGNATURE_VERIFIER, "Failed to read complete signature.");
        fclose(sigFile);
        return false;
    }
    fclose(sigFile);

    // --- Load Public Key ---
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

    // --- Verify Signature ---
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
                            firmwareHash, sizeof(firmwareHash),
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
