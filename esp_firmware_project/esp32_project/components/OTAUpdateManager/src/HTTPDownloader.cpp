#include "OTAUpdateManager/OTAUpdateManager.h"
#include "esp_http_client.h"
#include <cstring>
#include "Common/certificates.h"
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <unistd.h>
#include <inttypes.h>

#define FIRMWARE_API_KEY "......................................."

HttpDownloader::HttpDownloader() {}

bool HttpDownloader::downloadToPartition(const std::string &firmwareUrl,
                                         const std::string &signatureUrl,
                                         const esp_partition_t *partition,
                                         esp_ota_handle_t *otaHandleOut,
                                         uint32_t *firmwareSizeOut,
                                         std::vector<uint8_t> &signatureOut)
{
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Starting firmware download from URL: %s", firmwareUrl.c_str());

    esp_http_client_config_t fw_config = {};
    fw_config.url = firmwareUrl.c_str();
    fw_config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    fw_config.cert_pem = AWS_CA_CERT;
    fw_config.disable_auto_redirect = true;

    esp_http_client_handle_t client = esp_http_client_init(&fw_config);
    if (!client)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to init HTTP client for firmware");
        return false;
    }

    if (esp_http_client_set_header(client, "x-api-key", FIRMWARE_API_KEY) != ESP_OK ||
        esp_http_client_set_header(client, "Accept", "application/octet-stream") != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to set firmware request headers");
        esp_http_client_cleanup(client);
        return false;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to open firmware HTTP connection");
        esp_http_client_cleanup(client);
        return false;
    }

    // ---- Firmware Download ----
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Invalid firmware content length: %d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200 && status_code != 206)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Unexpected firmware HTTP status code: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Firmware HTTP response OK, size: %d bytes", content_length);

    esp_err_t err = esp_ota_begin(partition, content_length, otaHandleOut);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    uint8_t tempBuffer[1024];
    int total_written = 0;
    while (true)
    {
        int read_bytes = esp_http_client_read(client, (char *)tempBuffer, sizeof(tempBuffer));
        if (read_bytes < 0)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP read error during firmware download");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        else if (read_bytes == 0)
        {
            break; // End of stream
        }
        err = esp_ota_write(*otaHandleOut, tempBuffer, read_bytes);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "esp_ota_write failed at offset %d: %s", total_written, esp_err_to_name(err));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        total_written += read_bytes;
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Firmware chunk written: %d bytes, total: %d", read_bytes, total_written);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_written != content_length)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Firmware size mismatch: expected %d, got %d", content_length, total_written);
        return false;
    }

    *firmwareSizeOut = static_cast<uint32_t>(total_written);
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Firmware download complete, total bytes: %" PRIu32, *firmwareSizeOut);

    // ---- Signature Download ----
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Starting signature download from URL: %s", signatureUrl.c_str());

    esp_http_client_config_t sig_config = {};
    sig_config.url = signatureUrl.c_str();
    sig_config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    sig_config.cert_pem = AWS_CA_CERT;
    sig_config.disable_auto_redirect = true;

    esp_http_client_handle_t sigClient = esp_http_client_init(&sig_config);
    if (!sigClient)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to init HTTP client for signature");
        return false;
    }

    if (esp_http_client_open(sigClient, 0) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to open signature HTTP connection");
        esp_http_client_cleanup(sigClient);
        return false;
    }

    int sig_length = esp_http_client_fetch_headers(sigClient);
    if (sig_length <= 0)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Invalid signature content length: %d", sig_length);
        esp_http_client_close(sigClient);
        esp_http_client_cleanup(sigClient);
        return false;
    }

    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Signature HTTP response OK, size: %d bytes", sig_length);

    signatureOut.clear();
    signatureOut.resize(sig_length);

    int read_total = 0;
    while (read_total < sig_length)
    {
        int to_read = std::min<int>(sizeof(tempBuffer), sig_length - read_total);
        int r = esp_http_client_read(sigClient, (char *)tempBuffer, to_read);
        if (r < 0)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP read error during signature download");
            esp_http_client_close(sigClient);
            esp_http_client_cleanup(sigClient);
            return false;
        }
        else if (r == 0)
        {
            break;
        }

        memcpy(signatureOut.data() + read_total, tempBuffer, r);
        read_total += r;
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Signature chunk read: %d bytes, total: %d/%d", r, read_total, sig_length);
    }

    esp_http_client_close(sigClient);
    esp_http_client_cleanup(sigClient);

    if (read_total != sig_length)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Signature size mismatch: expected %d, got %d", sig_length, read_total);
        return false;
    }

    signatureOut.shrink_to_fit();
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Signature download complete, total bytes: %d", read_total);

    return true;
}
