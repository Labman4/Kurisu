#ifndef I2S_H
#define I2S_H

#ifdef __cplusplus
extern "C" {
#endif

void start_i2s();

void start_i2sServer(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif