#ifndef _WIFI_HANDLER_H_
#define _WIFI_HANDLER_H_

#include <esp_err.h>
#include <stdbool.h>
#include "esp_wifi_types_generic.h"

typedef enum {
    WIFI_HANDLER_MODE_FROM_SETTINGS,
    WIFI_HANDLER_MODE_DEFAULT_AP,
} wifi_handler_mode_t;

esp_err_t restart_wifi(void);
esp_err_t start_wifi(wifi_handler_mode_t mode);
bool wifi_is_connected(void);
wifi_mode_t wifi_get_mode(void);

#endif // _WIFI_HANDLER_H_