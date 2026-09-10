#ifndef NET_MDNS_H
#define NET_MDNS_H

#include "esp_err.h"

/* Advertises la-esp32.local and the analyzer service on the gateway. */
esp_err_t la_net_mdns_start(void);

#endif /* NET_MDNS_H */
