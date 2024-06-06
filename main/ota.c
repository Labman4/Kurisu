
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include "esp_crt_bundle.h"
#include "mbedtls/base64.h"
#include "esp_partition.h"
#if CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
#include "esp_efuse.h"
#endif
#include "esp_task_wdt.h"

#define HASH_LEN 32
#define OTA_URL_SIZE 256
// TaskHandle_t otaHandle = NULL;
static const char *TAG = "ota";

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    ESP_LOGI(TAG, "ota firmware version: %s", new_app_info -> version);
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        get_sha256_of_partitions();
        return ESP_FAIL;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
        ESP_LOGW(TAG, "New firmware security version is less than eFuse programmed, %"PRIu32" < %"PRIu32, new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

void check_task_stack() {
    TaskHandle_t xHandle = xTaskGetCurrentTaskHandle();
    UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(xHandle);
    ESP_LOGI(TAG, "Stack left for task %s: %d", pcTaskGetName(xHandle), stackLeft);
}

void erase_ota_partition() {
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (ota_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        return;
    }

    esp_err_t err = esp_partition_erase_range(ota_partition, 0, ota_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase OTA partition: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "OTA partition erased successfully");
    }
}

void check_partitions() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s", running->label);

    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Next OTA partition: %s", next->label);

    if (next != NULL) {
        ESP_LOGI(TAG, "Next OTA partition size: 0x%lu", next->size);
    } else {
        ESP_LOGE(TAG, "No OTA partition found");
    }
}

void print_hex(const char* input) {
    while (*input) {
        ESP_LOGI(TAG, "0x%02X ", (unsigned char)*input);
        input++;
    }
    ESP_LOGI(TAG, "\n");
}

int is_valid_base64(const char* input) {
    const char* valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    while (*input) {
        if (!strchr(valid_chars, *input)) {
            ESP_LOGW(TAG, "Invalid character: '%c' (0x%02X)\n", *input, (unsigned char)*input);
            return 0;
        }
        input++;
    }
    return 1;
}

char* base64_decode(const char* input, size_t* output_len) {
    if (input == NULL) {
        ESP_LOGW(TAG, "Invalid input: null\n");
        return NULL;
    }

    if (!is_valid_base64(input)) {
        ESP_LOGW(TAG, "Invalid input: contains non-base64 characters\n");
        return NULL;
    }

    size_t input_len = strlen(input);
    size_t decoded_len = 0;

    int ret = mbedtls_base64_decode(NULL, 0, &decoded_len, (const unsigned char*)input, input_len);
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ESP_LOGW(TAG, "Failed to calculate base64 decoded length, error code: %d\n", ret);
        return NULL;
    }

    char* output = (char*)malloc(decoded_len + 1);
    if (output == NULL) {
        ESP_LOGW(TAG, "Memory allocation failed\n");
        return NULL;
    }

    ret = mbedtls_base64_decode((unsigned char*)output, decoded_len, &decoded_len, (const unsigned char*)input, input_len);
    if (ret != 0) {
        ESP_LOGW(TAG, "Base64 decoding failed, error code: %d\n", ret);
        free(output);
        return NULL;
    }

    output[decoded_len] = '\0';
    if (output_len) {
        *output_len = decoded_len;
    }

    return output;
}

void ota_task(void *pvParameter)
{
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    char *taskParameter = (char *)pvParameter;
    if (taskParameter != NULL) {
        ESP_LOGI(TAG, "ota task param %s", taskParameter);
        config.url = strdup(taskParameter);        
    } else {
        config.url = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL;
    }

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb,
        // #ifdef CONFIG_EXAMPLE_ENABLE_PARTIAL_HTTP_DOWNLOAD
        //         .partial_http_download = true, // Register a callback to be invoked after esp_http_client is initialized
        //         .max_http_request_size = CONFIG_EXAMPLE_HTTP_REQUEST_SIZE,
        // #endif
    };
    // esp_task_wdt_deinit();
    esp_err_t ota_finish_err = ESP_OK;
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    // esp_task_wdt_config_t wdt_config = {
    //     .timeout_ms = 30000,         // 设置超时时间为 30 秒
    //     .trigger_panic = true,               // 超时触发 panic
    //     .idle_core_mask = (1 << portNUM_PROCESSORS) - 1 // 所有核心空闲时喂狗
    // };
    // esp_task_wdt_init(&wdt_config); 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        vTaskDelete(NULL);
    }
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(NULL);
        }
    }

    free(taskParameter);

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    free(taskParameter);
    vTaskDelete(NULL);
}

void start_ota(char* url) {
    #if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    /**
     * We are treating successful WiFi connection as a checkpoint to cancel rollback
     * process and mark newly updated firmware image as active. For production cases,
     * please tune the checkpoint behavior per end application requirement.
     */
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
                if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                    ESP_LOGI(TAG, "App is valid, rollback cancelled successfully");
                } else {
                    ESP_LOGE(TAG, "Failed to cancel rollback");
                }
            }
        }
    #endif
    if (url == NULL) {
        ESP_LOGI(TAG, "DEFAULT OTA URL %s", CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL);
        xTaskCreate(&ota_task, "ota_task", 8192, (void *)NULL, 5, NULL);
    } else {
        ESP_LOGI(TAG, "custom ota url");
        size_t decoded_len;
        char *ota_url = base64_decode(url, &decoded_len);
        xTaskCreate(&ota_task, "ota_task", 8192, (void *)ota_url, 5, NULL);
    }
}