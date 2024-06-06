#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
#include "string.h"
#include "esp_http_server.h"

// #include "driver/i2s_std.h"
// #include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#include "driver/i2s.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
// I2S configuration

#define I2S_DO_IO       4
#define I2S_BCK_IO      3
#define I2S_WS_IO       2
#define I2S_DI_IO       1
static const char *TAG = "i2s";

#define GPIO_MUTE_LEVEL 1
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ADC_I2S_CHANNEL 2
static int s_play_channel_format = 2;
static int s_bits_per_chan = 16;
static int s_play_sample_rate = 16000;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// static i2s_chan_handle_t                rx_handle = NULL;        // I2S rx channel handler
#endif

static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    esp_err_t ret_val = ESP_OK;

// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
//     i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);

//     ret_val |= i2s_new_channel(&chan_cfg, NULL, &rx_handle);
//     i2s_std_config_t std_cfg = {
//         .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
//         .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
//         .gpio_cfg = {
//             .mclk = I2S_GPIO_UNUSED,
//             .bclk = I2S_BCK_IO,
//             .ws = I2S_WS_IO,
//             .dout = I2S_GPIO_UNUSED,
//             .din = I2S_DI_IO,
//             .invert_flags = {
//                 .mclk_inv = false,
//                 .bclk_inv = false,
//                 .ws_inv = false,
//             },
//         },

//     };
//     std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
//     // std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;   //The default is I2S_MCLK_MULTIPLE_256. If not using 24-bit data width, 256 should be enough
//     ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
//     ret_val |= i2s_channel_enable(rx_handle);
// #else
//     // i2s_config_t i2s_config = I2S_CONFIG_DEFAULT(16000, I2S_CHANNEL_FMT_ONLY_LEFT, 32);
   
// #endif
    i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX |I2S_MODE_RX),
    .sample_rate = s_play_sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
    .dma_buf_count = 2,
    .dma_buf_len = 1024,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO,
        .mck_io_num = -1,
    };

    ret_val |= i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    ret_val |= i2s_set_pin(i2s_num, &pin_config);
    return ret_val;
}

void audio_task(void *arg)
{
    size_t bytes_read, bytes_written;
    char *i2s_read_buffer = (char *) calloc(1024, sizeof(char));
    char *i2s_write_buffer = (char *) calloc(1024, sizeof(char));
    float volume = 1; // 设置音量，范围从 0.0 到 1.0

    while (1) {
        // Read audio data from INMP441
        i2s_read(I2S_NUM_0, i2s_read_buffer, 1024, &bytes_read, portMAX_DELAY);

  // Adjust the volume
        for (int i = 0; i < bytes_read / sizeof(int16_t); i++) {
            i2s_write_buffer[i] = (int16_t)(i2s_read_buffer[i] * volume);
        }

        // Write audio data to MAX98537
        i2s_write(I2S_NUM_0, i2s_read_buffer, bytes_read, &bytes_written, portMAX_DELAY);

        // Optional: Add some processing to the audio data here
    }

    free(i2s_read_buffer);
    free(i2s_write_buffer);
    vTaskDelete(NULL);
}

// static esp_err_t bsp_i2s_deinit(i2s_port_t i2s_num)
// {
//     esp_err_t ret_val = ESP_OK;

// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
//     ret_val |= i2s_channel_disable(rx_handle);
//     ret_val |= i2s_del_channel(rx_handle);
//     rx_handle = NULL;
// #else
//     ret_val |= i2s_stop(i2s_num);
//     ret_val |= i2s_driver_uninstall(i2s_num);
// #endif

//     return ret_val;
// }

// esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
// {
//     esp_err_t ret = ESP_OK;
//     size_t bytes_read;
//     int audio_chunksize = buffer_len / (sizeof(int32_t));
// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
//     ret = i2s_channel_read(rx_handle, buffer, buffer_len, &bytes_read, portMAX_DELAY);
// #else
//     ret = i2s_read(I2S_NUM_1, buffer, buffer_len, &bytes_read, portMAX_DELAY);
// #endif

//     int32_t *tmp_buff = buffer;
//     for (int i = 0; i < audio_chunksize; i++) {
//         tmp_buff[i] = tmp_buff[i] >> 14; // 32:8为有效位， 8:0为低8位， 全为0， AFE的输入为16位语音数据，拿29：13位是为了对语音信号放大。
//     }

//     return ret;
// }

int bsp_get_feed_channel(void)
{
    return ADC_I2S_CHANNEL;
}

void create_wav_header(uint8_t* header, int wav_size, int sample_rate, int bits_per_sample, int channels) {
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    int block_align = channels * bits_per_sample / 8;

    // RIFF header
    memcpy(header, "RIFF", 4);
    int chunk_size = wav_size + 36;
    memcpy(header + 4, &chunk_size, 4);
    memcpy(header + 8, "WAVE", 4);

    // fmt subchunk
    memcpy(header + 12, "fmt ", 4);
    int subchunk1_size = 16;
    memcpy(header + 16, &subchunk1_size, 4);
    short audio_format = 1;
    memcpy(header + 20, &audio_format, 2);
    memcpy(header + 22, &channels, 2);
    memcpy(header + 24, &sample_rate, 4);
    memcpy(header + 28, &byte_rate, 4);
    memcpy(header + 32, &block_align, 2);
    memcpy(header + 34, &bits_per_sample, 2);

    // data subchunk
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &wav_size, 4);
}


esp_err_t audio_stream_handler(httpd_req_t *req) {
    size_t bytes_read;
    char buffer[1024];
    uint8_t wav_header[44];
    create_wav_header(wav_header, 0x7FFFFFFF, s_play_sample_rate, 16, 1); // 
    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_send_chunk(req, (const char *)wav_header, sizeof(wav_header));
    while (true) {
        i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        if (bytes_read > 0) {
            if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
                break;
            }
        }
    }
    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}
httpd_handle_t server = NULL;


void start_i2sServer(httpd_handle_t server) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t audio_uri = {
        .uri       = "/audio",
        .method    = HTTP_GET,
        .handler   = audio_stream_handler,
        .user_ctx  = NULL
    };
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register the URI handler
        httpd_register_uri_handler(server, &audio_uri);
    }
}
void i2s_init(void)
{
    bsp_i2s_init(I2S_NUM_0, s_play_sample_rate, s_play_channel_format, s_bits_per_chan);

    // i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);

    // ret_val |= i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    // i2s_std_config_t std_cfg = I2S_CONFIG_DEFAULT(16000, I2S_SLOT_MODE_MONO, 32);
    // std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    // // std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;   //The default is I2S_MCLK_MULTIPLE_256. If not using 24-bit data width, 256 should be enough
    // ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
    // ret_val |= i2s_channel_enable(rx_handle);
    // I2S configuration

    // i2s_config_t i2s_config = {
    //     .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    //     .sample_rate = 44100,
    //     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    //     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    //     .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    //     .intr_alloc_flags = 0,
    //     .dma_buf_count = 8,
    //     .dma_buf_len = 64,
    //     .use_apll = false
    // };

    // // I2S pin configuration
    // i2s_pin_config_t pin_config = {
    //     .bck_io_num = I2S_BCK_IO,
    //     .ws_io_num = I2S_WS_IO,
    //     .data_out_num = -1,
    //     .data_in_num = I2S_DI_IO
    // };

    // Install and start I2S driver
    // i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    // i2s_set_pin(I2S_NUM, &pin_config);
    // i2s_start(I2S_NUM);
    // i2s_zero_dma_buffer(I2S_NUM);
}

// int detect_flag = 0;
// static esp_afe_sr_iface_t *afe_handle = NULL;
// static esp_afe_sr_data_t *afe_data = NULL;
// static volatile int task_flag = 0;

// void feed_Task(void *arg)
// {
//     esp_afe_sr_data_t *afe_data = arg;
//     int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);

//     ESP_LOGI(TAG, "audio_chunksize %d", audio_chunksize);

//     int nch = afe_handle->get_channel_num(afe_data);

//     ESP_LOGI(TAG, "nch %d", nch);

//     int feed_channel = bsp_get_feed_channel();

//     ESP_LOGI(TAG, "feed_channel %d", feed_channel);

//     assert(nch<feed_channel);
//     int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
//     assert(i2s_buff);

//     while (task_flag) {
//         bsp_get_feed_data(false, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

//         afe_handle->feed(afe_data, i2s_buff);
//     }
//     if (i2s_buff) {
//         free(i2s_buff);
//         i2s_buff = NULL;
//     }
//     vTaskDelete(NULL);
// }

// void detect_Task(void *arg)
// {
//     esp_afe_sr_data_t *afe_data = arg;
//     int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
//     int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));
//     assert(buff);
//     printf("------------detect start------------\n");

//     while (task_flag) {
//         afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
//         if (!res || res->ret_value == ESP_FAIL) {
//             printf("fetch error!\n");
//             break;
//         }
//         // ESP_LOGI(TAG, "hear %d", res ->data_size );
//         if (res->wakeup_state == WAKENET_DETECTED) {
//             printf("wakeword detected\n");
// 	        printf("model index:%d, word index:%d\n", res->wakenet_model_index, res->wake_word_index);
//             printf("-----------LISTENING-----------\n");
//         }
//     }
//     if (buff) {
//         free(buff);
//         buff = NULL;
//     }
//     vTaskDelete(NULL);
// }

void start_i2s()
{
    // Initialize I2S
    i2s_init();
    // start_i2sServer(server);
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);


//     srmodel_list_t *models = esp_srmodel_init("model");
//     char *wn_name = NULL;
//     char *wn_name_2 = NULL;

//     if (models!=NULL) {
//         for (int i=0; i<models->num; i++) {
//             if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
//                 if (wn_name == NULL) {
//                     wn_name = models->model_name[i];
//                     printf("The first wakenet model: %s\n", wn_name);
//                 } else if (wn_name_2 == NULL) {
//                     wn_name_2 = models->model_name[i];
//                     printf("The second wakenet model: %s\n", wn_name_2);
//                 }
//             }
//         }
//     } else {
//         printf("Please enable wakenet model and select wake word by menuconfig!\n");
//         return ;
//     }

//     afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
//     afe_config_t afe_config = AFE_CONFIG_DEFAULT();
//     afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
//     afe_config.wakenet_init = true;
//     afe_config.vad_init = true;
//     afe_config.wakenet_model_name = wn_name;
//     afe_config.wakenet_model_name_2 = wn_name_2;
//     afe_config.voice_communication_init = false;
//     afe_config.pcm_config.total_ch_num = 2;
//     afe_config.pcm_config.mic_num = 1;
//     afe_config.pcm_config.ref_num = 1;
// #if defined CONFIG_ESP32_S3_BOX_BOARD || defined CONFIG_ESP32_S3_EYE_BOARD
//     afe_config.aec_init = false;
//     #if defined CONFIG_ESP32_S3_EYE_BOARD
 
//     #endif
// #endif
//     afe_data = afe_handle->create_from_config(&afe_config);
    
//     task_flag = 1;
//     xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);
//     xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void*)afe_data, 5, NULL, 1);

    // // You can call afe_handle->destroy to destroy AFE.
    // task_flag = 0;

    // printf("destroy\n");
    // afe_handle->destroy(afe_data);
    // afe_data = NULL;
    // printf("successful\n");
}


