#include "Arduino.h"
#include <esp_log.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include "esp_wifi.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "mbedtls/base64.h"
#include "esp_https_ota.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#include "wifi.h"
#include "cam.h"
#include "motor.h"
#include "i2s.h"
#include "ota.h"
#include "server.h"

#define CONFIG_UDP_SERVER

static const char *TAG = "app";

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGI(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}

extern "C" void app_main()
{
    initArduino();
    // Serial.begin(115200);
    // while(!Serial){
        
    // }
    // GPIO_INIT();
    // SERVO_INIT();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    load_wifi_config_and_connect();
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    wifi_ap_record_t ap_info;
    while (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGI(TAG, "wait to get AP info");
        vTaskDelay(pdMS_TO_TICKS(3000)); 
    }
    ESP_LOGI(TAG, "wifi connected");
    start_web();
    // start_i2s();
    // start_i2sServer(server);
    // start_cam(server, 1000000, "JPEG", "VGA", 1, false);


#ifdef CONFIG_UDP_SERVER
    // xTaskCreate(&udp_broadcast_task, "udp_broadcast_task", 2048, NULL, 5, NULL);
#endif 
}

