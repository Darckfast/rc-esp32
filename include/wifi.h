#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

esp_err_t wifi_init(void);

esp_err_t wifi_connect(char wifi_ssid, char wifi_password);

esp_err_t wifi_disconnect(void);

esp_err_t mwifi_deinit(void);

#endif
