#include "Arduino.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <stdbool.h>
#include <string.h>
#include "ESP32_OV5640_AF.h"

#define WROOM 
// #define OV_7670 
// #define OV 

#ifdef OV_7670
#define CAM_PIN_PWDN 8
#define CAM_PIN_XCLK 3
#define CAM_PIN_SDA 1
#define CAM_PIN_HREF 2
#define CAM_PIN_SCL 46

#define CAM_PIN_VSYNC 45
#define CAM_PIN_PCLK 15
#define CAM_PIN_D7 16
#define CAM_PIN_D5 17
#define CAM_PIN_D3 18                                                                                                                                                              
#define CAM_PIN_D1 21
#define CAM_PIN_RESET 38 //software reset will be performed

#define CAM_PIN_D6 4
#define CAM_PIN_D4 5                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
#define CAM_PIN_D2 6
#define CAM_PIN_D0 7
#endif

#ifdef OV
#define CAM_PIN_PWDN 38
#define CAM_PIN_XCLK 3
#define CAM_PIN_SDA 1
#define CAM_PIN_HREF 45
#define CAM_PIN_SCL 46

#define CAM_PIN_VSYNC 2
#define CAM_PIN_PCLK 3
#define CAM_PIN_D7 7
#define CAM_PIN_D5 6
#define CAM_PIN_D3 5                                                                                                                                                              
#define CAM_PIN_D1 4
#define CAM_PIN_RESET 3 //software reset will be performed

#define CAM_PIN_D6 18
#define CAM_PIN_D4 17                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
#define CAM_PIN_D2 16
#define CAM_PIN_D0 15
#endif

#ifdef WROOM
#define CAM_PIN_PWDN -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SDA 4
#define CAM_PIN_HREF 7
#define CAM_PIN_SCL 5

#define CAM_PIN_VSYNC 6
#define CAM_PIN_PCLK 13
#define CAM_PIN_D7 16
#define CAM_PIN_D5 18
#define CAM_PIN_D3 10                                                                                                                                                              
#define CAM_PIN_D1 9
#define CAM_PIN_RESET -1 //software reset will be performed

#define CAM_PIN_D6 17
#define CAM_PIN_D4 12                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
#define CAM_PIN_D2 8
#define CAM_PIN_D0 11
#endif
#define PART_BOUNDARY "123456789000000000000987654321"

static const char *TAG = "cam";

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
static QueueHandle_t xQueueFrameI = NULL;

static bool gReturnFB = true;
static bool cam_init = false;
TaskHandle_t streamHandle = NULL;

const char *pixformat_str[] = {
    "RGB565",
    "YUV422",
    "YUV420",
    "GRAYSCALE",
    "JPEG",
    "RGB888",
    "RAW",
    "RGB444",
    "RGB555"
};

const char *framesize_str[] = {
    "96X96",
    "QQVGA",
    "QCIF",
    "HQVGA",
    "240X240",
    "QVGA",
    "CIF",
    "HVGA",
    "VGA",
    "SVGA",
    "XGA",
    "HD",
    "SXGA",
    "UXGA",
    "FHD",
    "P_HD",
    "P_3MP",
    "QXGA",
    "QHD",
    "WQXGA",
    "P_FHD",
    "QSXGA"
};

pixformat_t str_to_pixformat(char* str) {
    for (int i = 0; i < sizeof(pixformat_str) / sizeof(pixformat_str[0]); ++i) {
        if (strcmp(str, pixformat_str[i]) == 0) {
            return (pixformat_t)i;
        }
    }
    return PIXFORMAT_JPEG;
}

framesize_t str_to_framesize(char* str) {
    for (int i = 0; i < sizeof(framesize_str) / sizeof(framesize_str[0]); ++i) {
        if (strcmp(str, framesize_str[i]) == 0) {
            return (framesize_t)i;
        }
    }
    return FRAMESIZE_INVALID;
}

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)
#define TEST_ASSERT_NOT_NULL(ret) assert(ret != NULL)

esp_err_t jpg_stream_httpd_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    static int64_t con_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            if(!con_frame) {
                con_frame = esp_timer_get_time();
            }   
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
            int64_t con_end = esp_timer_get_time();
            int64_t con_time = con_end - con_frame;
            con_frame = con_end;
            con_time /= 1000;
            ESP_LOGI(TAG, "->JPG: %lums", (uint32_t)con_time);
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %luKB %lums (%.1ffps)",
            (uint32_t)(_jpg_buf_len/1024),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

OV5640 ov5640 = OV5640();

extern "C" esp_err_t init_camera(int xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count, int focus)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SDA,
        .pin_sccb_scl = CAM_PIN_SCL,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,    //QQVGA-UXGA, sizes above QVGA are not been recommended when not JPEG format.

        .jpeg_quality = 10, //0-63, for OV series camera sensors, lower number means higher quality
        .fb_count = fb_count,       // For ESP32/ESP32-S2, if more than one, i2s runs in continuous mode. Use only with JPEG.
        .fb_location = CAMERA_FB_IN_PSRAM,
        // .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
        // CAMERA_GRAB_WHEN_EMPTY
        //CAMERA_GRAB_LATEST
    };
    esp_err_t ret = esp_camera_init(&camera_config);

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);//flip it back
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_saturation(s, -2);//lower the saturation
    }

    if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID) {
        s->set_vflip(s, 1); //flip it back
    } else if (s->id.PID == GC0308_PID) {
        s->set_hmirror(s, 0);
    } else if (s->id.PID == GC032A_PID) {
        s->set_vflip(s, 1);
    }
    if (s->id.PID == OV5640_PID && focus == 1) {
        ov5640.start(s);
        if (ov5640.focusInit() == 0) {
            ESP_LOGW(TAG, "OV5640_Focus_Init Successful!");
        }

        if (ov5640.autoFocusMode() == 0) {
            ESP_LOGW(TAG, "OV5640_Auto_Focus Successful!");
        }
    }

    return ret;
}

esp_err_t raw_stream_handler(httpd_req_t *req)
{
    camera_fb_t *frame = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "15");

    while (true) {
        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY)) {
            _timestamp.tv_sec = frame->timestamp.tv_sec;
            _timestamp.tv_usec = frame->timestamp.tv_usec;
        } else {
            res = ESP_FAIL;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            if (res == ESP_OK) {
                size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
        }

        if (gReturnFB) {
            esp_camera_fb_return(frame);
        } else {
            free(frame->buf);
        }

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }

    return res;
}

esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *frame = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "15");

    while (true) {
        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY)) {
            _timestamp.tv_sec = frame->timestamp.tv_sec;
            _timestamp.tv_usec = frame->timestamp.tv_usec;
            uint8_t rc = ov5640.getFWStatus();
            // ESP_LOGW(TAG, "FW_STATUS = 0x%x\n", rc);

            // if (rc == -1) {
            //     ESP_LOGW(TAG, "Check your OV5640");
            // } else if (rc == FW_STATUS_S_FOCUSED) {
            //     ESP_LOGW(TAG, "Focused!");
            // } else if (rc == FW_STATUS_S_FOCUSING) {
            //     ESP_LOGW(TAG, "Focusing!");
            // }

            if (frame->format == PIXFORMAT_JPEG) {
                _jpg_buf = frame->buf;
                _jpg_buf_len = frame->len;
            } else if (!frame2jpg(frame, 60, &_jpg_buf, &_jpg_buf_len)) {
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }
        } else {
            res = ESP_FAIL;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            if (res == ESP_OK) {
                size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }

            if (frame->format != PIXFORMAT_JPEG) {
                free(_jpg_buf);
                _jpg_buf = NULL;
            }
        }

        if (gReturnFB) {
            esp_camera_fb_return(frame);
        } else {
            free(frame->buf);
        }

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }

    return res;
}

esp_err_t focus_stream_handler(httpd_req_t *req)
{
    camera_fb_t *frame = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "15");

    while (true) {
        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY)) {
            _timestamp.tv_sec = frame->timestamp.tv_sec;
            _timestamp.tv_usec = frame->timestamp.tv_usec;
            uint8_t rc = ov5640.getFWStatus();

            ESP_LOGD(TAG, "FW_STATUS = 0x%x\n", rc);
            if (rc == -1) {
                ESP_LOGW(TAG, "Check your OV5640");
            } else if (rc == FW_STATUS_S_FOCUSED) {
                ESP_LOGW(TAG, "Focused!");
            } else if (rc == FW_STATUS_S_FOCUSING) {
                ESP_LOGW(TAG, "Focusing!");
            }

            if (frame->format == PIXFORMAT_JPEG) {
                _jpg_buf = frame->buf;
                _jpg_buf_len = frame->len;
            } else if (!frame2jpg(frame, 60, &_jpg_buf, &_jpg_buf_len)) {
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }
        } else {
            res = ESP_FAIL;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            if (res == ESP_OK) {
                size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }

            if (frame->format != PIXFORMAT_JPEG) {
                free(_jpg_buf);
                _jpg_buf = NULL;
            }
        }

        if (gReturnFB) {
            esp_camera_fb_return(frame);
        } else {
            free(frame->buf);
        }

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }

    return res;
}

httpd_uri_t raw_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = raw_stream_handler,
    .user_ctx = NULL
};

httpd_uri_t focus_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = focus_stream_handler,
    .user_ctx = NULL
};

httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
};

void streamTask(void *pvParameters) {
    camera_fb_t *frame;
    while (true) {
        frame = esp_camera_fb_get();
        if (frame) {
            xQueueSend(xQueueFrameI, &frame, portMAX_DELAY);
        }
    }
}

extern "C" void start_cam(httpd_handle_t server, uint32_t xclk_freq_hz, char* pixel_format, char* frame_size, uint8_t fb_count, char* handler, int focus) {
    ESP_LOGW(TAG, "hz %lu, format %s, size %s reinit %d", xclk_freq_hz, pixel_format, frame_size, cam_init);
    if (cam_init) {
        ESP_LOGW(TAG, "reninit camera");
        TEST_ESP_OK(esp_camera_deinit());
        gReturnFB = false;
        cam_init = false;
        vTaskDelay(500);
        httpd_unregister_uri_handler(server, "/stream", HTTP_GET);
        if (streamHandle != NULL) {
            vTaskDelete(streamHandle);
            streamHandle = NULL;
        }
    }
    ESP_ERROR_CHECK(init_camera(xclk_freq_hz, str_to_pixformat(pixel_format), str_to_framesize(frame_size), fb_count, focus));
    cam_init = true;
    xQueueFrameI = xQueueCreate(fb_count, sizeof(camera_fb_t *));
    gReturnFB = true;
    if (focus == 1) {
        httpd_register_uri_handler(server, &focus_uri);
    } else if (strcmp(handler, "raw") == 0) {
        httpd_register_uri_handler(server, &raw_uri);
    } else {
        httpd_register_uri_handler(server, &stream_uri);
    }
    xTaskCreate(&streamTask, "streamTask", 4096, NULL, 5, &streamHandle);
}

bool camera_test_fps(uint16_t times, float *fps, size_t *size)
{
    *fps = 0.0f;
    *size = 0;
    uint32_t s = 0;
    uint32_t num = 0;
    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            return 0;
        } else {
            s += pic->len;
            num++;
        }
        esp_camera_fb_return(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    if (num) {
        *fps = num * 1000000.0f / total_time;
        *size = s / num;
    }
    return 1;
}

void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

extern "C" void camera_performance_test_with_format(httpd_handle_t server, uint32_t xclk_freq, uint32_t pic_num, char* pixel_format, char* frame_size, uint32_t count)
{
    if (cam_init) {
        ESP_LOGW(TAG, "reninit camera");
        TEST_ESP_OK(esp_camera_deinit());
        gReturnFB = false;
        cam_init = false;
        vTaskDelay(500);
        httpd_unregister_uri_handler(server, "/stream", HTTP_GET);
        if (streamHandle != NULL) {
            vTaskDelete(streamHandle);
            streamHandle = NULL;
        }
    }
    ESP_LOGW(TAG, "hz %lu, format %s, size %s", xclk_freq, pixel_format, frame_size);
    esp_err_t ret = ESP_OK;
    uint64_t t1 = 0;
    pixformat_t format = str_to_pixformat(pixel_format);
    framesize_t size = str_to_framesize(frame_size);
    // detect sensor information
    t1 = esp_timer_get_time();
    TEST_ESP_OK(init_camera(20000000, format, size, count, 0));
    printf("Cam prob %llu\n", (esp_timer_get_time() - t1) / 1000);
    // get sensor info
    sensor_t *s = esp_camera_sensor_get();
    camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
    TEST_ASSERT_NOT_NULL(info);
    // deinit sensor
    TEST_ESP_OK(esp_camera_deinit());
    vTaskDelay(500);

    struct fps_result {
        float fps;
        size_t size;
    };
    struct fps_result results = {0};

    ret = init_camera(xclk_freq, format, size, count, 0);
    vTaskDelay(100);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Testing init failed :-(, skip this item");
        task_fatal_error();
    }
    camera_test_fps(pic_num, &results.fps, &results.size);
    TEST_ESP_OK(esp_camera_deinit());

    printf("FPS Result\n");
    printf("fps, size \n");

    printf("%5.2f,     %7u \n",
           results.fps, results.size);

    printf("----------------------------------------------------------------------------------------\n");
}
