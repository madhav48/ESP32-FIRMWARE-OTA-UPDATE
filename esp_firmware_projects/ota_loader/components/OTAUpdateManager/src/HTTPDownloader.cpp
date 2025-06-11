#include "OTAUpdateManager/OTAUpdateManager.h"
#include "esp_http_client.h"
#include <cstring>
#include "Common/certificates.h"
#include <sys/stat.h>
#include "esp_log.h"
#include <unistd.h> 


// To change: Retrieve FIRMWARE_API_KEY from secure storage (e.g., NVS)...
#define FIRMWARE_API_KEY "......................."


HttpDownloader::HttpDownloader() {}

bool HttpDownloader::downloadToFile(const std::string &url, const std::string &path)
{
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = AWS_CA_CERT; // AWS Root CA for API Gateway
    config.disable_auto_redirect = true;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to init HTTP client");
        return false;
    }

    // Set the API Key header for authentication
    if (esp_http_client_set_header(client, "x-api-key", FIRMWARE_API_KEY) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to set x-api-key header");
        esp_http_client_cleanup(client);
        return false;
    }

    // Set Accept header to match what the API expects
    if (esp_http_client_set_header(client, "Accept", "application/octet-stream") != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to set Accept header");
        esp_http_client_cleanup(client);
        return false;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to open HTTP connection");
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to fetch HTTP headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Content-Length reported by server: %d", content_length);

    int status_code = esp_http_client_get_status_code(client);
    // Accept 200 OK or 206 Partial Content
    if (status_code != 200 && status_code != 206)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP status code unexpected: %d", status_code);
        // Authentication failure handling
        if (status_code == 401 || status_code == 403) {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Authentication failed. Check API Key or permissions.");
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    else if (status_code == 206)
    {
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Received 206 Partial Content.");
        char *content_range_value = nullptr; // Pointer to store the header value
        if (esp_http_client_get_header(client, "Content-Range", &content_range_value) == ESP_OK && content_range_value) {
            ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Content-Range: %s", content_range_value);
        } else {
            ESP_LOGW(TAG_OTA_HTTP_DOWNLOADER, "Content-Range header not found for 206 response.");
        }
    }

    FILE *f = fopen(path.c_str(), "wb");
    
    if (!f)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to open file for writing: %s", path.c_str());
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
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP read error");
            fclose(f);
            remove(path.c_str()); // Remove incomplete file
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        else if (read_bytes == 0)
        {
            break; // End of stream
        }

        // Check if there's enough space in the file system before writing
        size_t written_bytes = fwrite(tempBuffer, 1, read_bytes, f);
        if (written_bytes != read_bytes) {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "File write error, only wrote %d of %d bytes", written_bytes, read_bytes);
            fclose(f);
            remove(path.c_str()); // Remove incomplete file
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }

        total_written += read_bytes;
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Downloaded %d bytes, Total written: %d", read_bytes, total_written);
    }

    // ERROR CHECK: Crucial for ensuring all data is flushed to disk
    if (fclose(f) != 0) {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to close file or flush data: %s", path.c_str());
        remove(path.c_str());
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false; // Indicate failure
    }
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "File closed successfully and data flushed.");


    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Download complete. Total bytes written (from loop): %d", total_written);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Downloaded file size on disk (from stat): %ld bytes", st.st_size);
        if (st.st_size == 0)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Downloaded file is empty");
            remove(path.c_str());
            return false;
        }
        // Additional check: Does the stat size match the expected content_length (if available)?
        if (content_length > 0 && st.st_size != content_length) {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Downloaded file size mismatch! Expected: %d, Got: %ld", content_length, st.st_size);
            remove(path.c_str());
            return false;
        }
    }
    else
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "stat() failed, file not found after download: %s", path.c_str());
        return false;
    }

    return true;
}