#include "OTAUpdateManager/OTAUpdateManager.h"
#include "esp_http_client.h"
#include <cstring>
#include "Common/certificates.h"
#include <sys/stat.h>

HttpDownloader::HttpDownloader() {}

bool HttpDownloader::downloadToFile(const std::string &url, const std::string &path)
{
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = AWS_CA_CERT;
    config.disable_auto_redirect = true;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Failed to init HTTP client");
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
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Content length: %d", content_length);

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200)
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP status != 200: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
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
    int last_logged_percent = -1;

    while (true)
    {
        int read = esp_http_client_read(client, (char *)tempBuffer, sizeof(tempBuffer));
        if (read < 0)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "HTTP read error");
            fclose(f);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        else if (read == 0)
        {
            break;
        }

        fwrite(tempBuffer, 1, read, f);
        total_written += read;

        if (content_length > 0)
        {
            int percent = (total_written * 100) / content_length;
            if (percent != last_logged_percent && percent % 10 == 0) // Log every 10%
            {
                ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Download progress: %d%%", percent);
                last_logged_percent = percent;
            }
        }
    }

    fclose(f);
    ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Total bytes written: %d", total_written);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        ESP_LOGI(TAG_OTA_HTTP_DOWNLOADER, "Downloaded file size: %ld bytes", st.st_size);
        if (st.st_size == 0)
        {
            ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "Downloaded file is empty");
            return false;
        }
    }
    else
    {
        ESP_LOGE(TAG_OTA_HTTP_DOWNLOADER, "stat() failed, file not found");
        return false;
    }

    return true;
}
