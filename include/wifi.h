#ifndef WIFI_H
#define WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

void load_wifi_config_and_connect();

void udp_broadcast_task(void *pvParameters);

void initialise_wifi();

#ifdef __cplusplus
}
#endif

#endif