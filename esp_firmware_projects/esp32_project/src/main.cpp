extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_ota_ops.h"
#include "esp_system.h" 
#include "esp_mac.h"     
#include "esp_wifi.h"
}

#include "Common/certificates.h"
#include "OTAUpdateManager/OTAUpdateManager.h"

inline const char *TAG = "Main App";

#define WIFI_SSID "..............."
#define WIFI_PASS "..........."
#define MQTT_BROKER_URI "mqtts://xxxxxxxxxxx.iot.eu-north-1.amazonaws.com"

int retry_num = 0;
static esp_mqtt_client_handle_t mqtt_client = nullptr;
char device_firmware_topic[64];

static void wifi_init();
static void mqtt_init();
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void generate_device_firmware_topic() {
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC: %s", esp_err_to_name(err));
        return;
    }

    snprintf(device_firmware_topic, sizeof(device_firmware_topic),
             "firmware_update/%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Device-specific topic: %s", device_firmware_topic);
}


static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_STA_START)
  {
    printf("WIFI CONNECTING....\n");
  }
  else if (event_id == WIFI_EVENT_STA_CONNECTED)
  {
    printf("WiFi CONNECTED\n");
  }
  else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    printf("WiFi lost connection\n");
    if (retry_num < 5)
    {
      esp_wifi_connect();
      retry_num++;
      printf("Retrying to Connect...\n");
    }
  }
  else if (event_id == IP_EVENT_STA_GOT_IP)
  {
    printf("Wifi got IP...\n\n");
  }
}

static void wifi_init()
{
  esp_netif_init();
  esp_event_loop_create_default();     // event loop
  esp_netif_create_default_wifi_sta(); // WiFi station
  wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_initiation);

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

  wifi_config_t wifi_configuration = {};
  strcpy((char *)wifi_configuration.sta.ssid, WIFI_SSID);
  strcpy((char *)wifi_configuration.sta.password, WIFI_PASS);

  strcpy((char *)wifi_configuration.sta.ssid, WIFI_SSID);
  strcpy((char *)wifi_configuration.sta.password, WIFI_PASS);

  esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration);

  esp_wifi_start();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_connect();

  printf("wifi_init_softap finished. SSID:%s  password:%s\n", WIFI_SSID, WIFI_PASS);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
  switch (event->event_id)
  {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT Connected");
    esp_mqtt_client_subscribe(mqtt_client, "firmware_update", 0);
    esp_mqtt_client_subscribe(mqtt_client, device_firmware_topic, 0);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT Disconnected");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "Subscribed to topic");
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "Received MQTT Message");
    printf("Topic: %.*s\n", event->topic_len, event->topic);
    printf("Data: %.*s\n", event->data_len, event->data);

    // Check for firmware Update..
    if ((event->topic_len == strlen("firmware_update") &&
         strncmp((const char *)event->topic, "firmware_update", event->topic_len) == 0) ||
        (event->topic_len == strlen(device_firmware_topic) &&
         strncmp((const char *)event->topic, device_firmware_topic, event->topic_len) == 0))
    {
      auto *params = new ota_task_params_t{std::string(event->data, event->data_len)};
      xTaskCreate(&ota_update_task, "ota_update_task", 8192, params, 5, NULL);
    }
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
}

static void mqtt_init()
{
  // Zero-initialize the configuration structure
  esp_mqtt_client_config_t mqtt_cfg = {};

  // Set broker URI
  mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;

  // Set TLS client certificate and private key
  mqtt_cfg.credentials.authentication.certificate = IoT_CLIENT_CERT;
  mqtt_cfg.credentials.authentication.key = IoT_PRIVATE_KEY;

  // CA certificate
  mqtt_cfg.broker.verification.certificate = AWS_CA_CERT;

  // Setup the MQTT client
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (mqtt_client == NULL)
  {
    ESP_LOGE("MQTT", "Failed to create MQTT client");
    return;
  }

  // Register event handler
  esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

  // Start the MQTT client
  esp_mqtt_client_start(mqtt_client);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "OTA-Boot Loader Started...");

    nvs_flash_init();
    wifi_init();

    // Delay to allow WiFi connection to establish before starting MQTT
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    generate_device_firmware_topic();
    mqtt_init();


    // --- Main APP Logic ----

    int i = 0;  // counter for logging

    while (true)
    {
        ESP_LOGI(TAG, "Hello World! %d", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
        i+=10;
    }
}
