#include <esp_log.h>
#include "cJSON.h"
#include "esp_http_server.h"
#include "string.h"
#include "mbedtls/base64.h"
#include <cstring>
#include <iostream> 

#include "cam.h"
#include "motor.h"
#include "i2s.h"
#include "ota.h"
static const char *TAG = "web";

static httpd_handle_t server = NULL;

esp_err_t cam_handler(httpd_req_t *req){
    int ret = req->content_len;
    char *content = new char[req->content_len + 1];
    if (!content) {                                                        
        return ESP_FAIL;
    }

    // Read the request body
    ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {  // 0 return value indicates connection closed
        delete[] content;
        return ESP_FAIL;
    }
    content[ret] = '\0';  // Null-terminate the string

    ESP_LOGD(TAG, "Received JSON data: %s", content);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        delete[] content;
        return ESP_FAIL;
    }

    const cJSON *format = cJSON_GetObjectItemCaseSensitive(json, "format");
    const cJSON *hz = cJSON_GetObjectItemCaseSensitive(json, "hz");
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(json, "size");
    const cJSON *test = cJSON_GetObjectItemCaseSensitive(json, "test");
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(json, "count");
    const cJSON *testNum = cJSON_GetObjectItemCaseSensitive(json, "testNum");
    const cJSON *handler = cJSON_GetObjectItemCaseSensitive(json, "handler");

    char* default_handler = "stream";
    char* default_format = "RGB565";
    char* default_size = "VGA";
    int default_hz = 10000000;
    int default_test = 1;
    int default_count = 1;
    int default_testNum = 360;

    if (cJSON_IsNumber(hz)) {
        ESP_LOGD(TAG, "hz: %d", hz->valueint);
        default_hz = hz->valueint;
    } 

    if (cJSON_IsString(handler)) {
        ESP_LOGD(TAG, "handler: %s", handler->valuestring);
        default_handler = handler->valuestring;
    }

    if (cJSON_IsString(format)) {
        ESP_LOGD(TAG, "format: %s", format->valuestring);
        default_format = format->valuestring;
    }

    if (cJSON_IsString(size)) {
        ESP_LOGD(TAG, "size: %s", size->valuestring);
        default_size = size->valuestring;
    }

    if (cJSON_IsNumber(test)) {
        ESP_LOGD(TAG, "test: %d", test->valueint);
        default_test = test->valueint;
        if (cJSON_IsNumber(testNum)) {
        ESP_LOGD(TAG, "testNum: %d", testNum->valueint);
        default_testNum = testNum->valueint;
        } 
    }

    if (cJSON_IsNumber(count)) {
        ESP_LOGD(TAG, "count: %d", count -> valueint);
        default_count = count -> valueint;
    }
    if (default_test == 1) {
        start_cam(server, default_hz, default_format, default_size, default_count, default_handler);
    } else {
        camera_performance_test_with_format(server, default_hz, default_testNum, default_format, default_size, default_count);
    }

    cJSON_Delete(json);
    delete[] content;
    const char resp[] = "1";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t motor_handler(httpd_req_t *req){
    int ret = req->content_len;
    char *content = new char[req->content_len + 1];
    if (!content) {                                                        
        return ESP_FAIL;
    }

    // Read the request body
    ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {  // 0 return value indicates connection closed
        delete[] content;;
        return ESP_FAIL;
    }
    content[ret] = '\0';  // Null-terminate the string

    ESP_LOGD(TAG, "Received JSON data: %s", content);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        delete[] content;
        return ESP_FAIL;
    }

    // Extract values from the JSON object
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(json, "index");

    if (cJSON_IsNumber(index)) {
        ESP_LOGD(TAG, "index: %d", index->valueint);
    } else {
        return ESP_FAIL;
    }

    if (cJSON_IsNumber(state)) {
        ESP_LOGD(TAG, "state: %d", state->valueint);
    } else {
        return ESP_FAIL;
    }
    motor_control(index->valueint, state->valueint);

    // Clean up
    cJSON_Delete(json);
    delete[] content;

    const char resp[] = "1";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t servo_handler(httpd_req_t *req){
    int ret = req->content_len;
    char *content = new char[req->content_len + 1];
    if (!content) {                                                        
        return ESP_FAIL;
    }

    // Read the request body
    ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {  // 0 return value indicates connection closed
        delete[] content;
        return ESP_FAIL;
    }
    content[ret] = '\0';  // Null-terminate the string

    ESP_LOGD(TAG, "Received JSON data: %s", content);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        delete[] content;
        return ESP_FAIL;
    }

    // Extract values from the JSON object
    const cJSON *angle = cJSON_GetObjectItemCaseSensitive(json, "angle");
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(json, "index");

    if (cJSON_IsNumber(index)) {
        ESP_LOGD(TAG, "index: %d", index->valueint);
    } else {
        return ESP_FAIL;
    }

    if (cJSON_IsNumber(angle)) {
        ESP_LOGD(TAG, "angle: %d", angle->valueint);
    } else {
        return ESP_FAIL;
    }
    set_servo_angle(index->valueint, angle->valueint);

    // Clean up
    cJSON_Delete(json);
    delete[] content;

    const char resp[] = "1";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

char* base64_encode(const char* input, size_t input_len) {
    if (input == nullptr || input_len == 0) {
        std::cerr << "Invalid input: null or zero length\n";
        return nullptr; // 输入无效
    }

    // 计算输出缓冲区的大小
    size_t output_len = 4 * ((input_len + 2) / 3) + 1; // Base64 编码后的大小，包括 null 终止符
    char* output = new (std::nothrow) char[output_len]; // 分配输出缓冲区
    if (output == nullptr) {
        std::cerr << "Memory allocation failed\n";
        return nullptr; // 分配失败
    }

    // 执行 base64 编码
    int ret = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(output), output_len, &output_len, reinterpret_cast<const unsigned char*>(input), input_len);
    if (ret != 0) {
        std::cerr << "Base64 encoding failed, error code: " << ret << "\n";
        delete[] output; // 编码失败，释放内存并返回 nullptr
        return nullptr;
    }

    return output;
}

void print_hex(const char* input) {
    while (*input) {
        ESP_LOGI(TAG, "0x%02X ", (unsigned char)*input);
        input++;
    }
    ESP_LOGI(TAG, "\n");
}

esp_err_t ota_handler(httpd_req_t *req){
    int ret = req->content_len;
    char *content = new char[req->content_len + 1];

    if (!content) {                                                        
        return ESP_FAIL;
    }

    // Read the request body
    ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {  // 0 return value indicates connection closed
        delete[] content;
        return ESP_FAIL;
    }
    content[ret] = '\0';  // Null-terminate the string
    ESP_LOGI(TAG, "Received JSON data: %s", content);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        delete[] content;
        return ESP_FAIL;
    }

    // Extract values from the JSON object
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(json, "url");
    if (cJSON_IsString(url)) {
        char *encoded = base64_encode(url->valuestring, std::strlen(url->valuestring));
        if (encoded) {
            // std::cout << "Base64 Encoded: " << encoded << std::endl;
            start_ota(encoded);
            delete[] encoded;
        } else {
            // std::cerr << "Base64 Encoding failed." << std::endl;
        }
    }

    // Clean up
    cJSON_Delete(json);
    delete[] content;

    const char resp[] = "1";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_uri_t ota = {
    .uri      = "/ota",
    .method   = HTTP_POST,
    .handler  = ota_handler,
    .user_ctx = NULL
};


httpd_uri_t motor = {
    .uri      = "/motor",
    .method   = HTTP_POST,
    .handler  = motor_handler,
    .user_ctx = NULL
};

httpd_uri_t pwm = {
    .uri      = "/servo",
    .method   = HTTP_POST,
    .handler  = servo_handler,
    .user_ctx = NULL
};

httpd_uri_t cam = {
    .uri      = "/cam",
    .method   = HTTP_POST,
    .handler  = cam_handler,
    .user_ctx = NULL
};


extern "C" void start_web()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 5120;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &motor);
        httpd_register_uri_handler(server, &pwm);
        httpd_register_uri_handler(server, &cam);
        httpd_register_uri_handler(server, &ota);
    }
}

extern "C" void stop_web()
{
    if (server) {
        httpd_stop(server);
    }
}
