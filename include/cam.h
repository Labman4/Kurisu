
#ifndef CAM_H
#define CAM_H

#ifdef __cplusplus
extern "C" {
#endif

void start_cam(httpd_handle_t server, int xclk_freq_hz, char* pixel_format, char* frame_size, uint8_t fb_count, char* handler);

void camera_performance_test_with_format(httpd_handle_t server, uint32_t xclk_freq, uint32_t pic_num, char* pixel_format, char* frame_size, uint8_t fb_count);

#ifdef __cplusplus
}
#endif

#endif