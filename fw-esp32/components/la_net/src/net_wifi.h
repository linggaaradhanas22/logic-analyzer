#ifndef NET_WIFI_H
#define NET_WIFI_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "la_net_policy.h"

/* Brings up the station, registers event handlers and posts policy events
 * (WIFI_UP on GOT_IP, WIFI_LOST on disconnect) into event_queue. */
esp_err_t la_net_wifi_init(QueueHandle_t event_queue);

esp_err_t la_net_wifi_connect(void);

esp_err_t la_net_wifi_stop(void);

int la_net_wifi_rssi(void);

uint32_t la_net_wifi_event_drops(void);

#endif /* NET_WIFI_H */
