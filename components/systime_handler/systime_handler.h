#ifndef __SYSTIME_HANDLER_H__
#define __SYSTIME_HANDLER_H__

#include <stdbool.h>
#include "esp_err.h"

esp_err_t sh_init(bool is_sta);
esp_err_t sh_set_timezone(char *timezone);
/**
 * @brief Get current timezone from settings file
 * 
 * @param timezone should be at least 40 bytes
 * @return esp_err_t ESP_OK if OK
 */
esp_err_t sh_get_timezone(char *timezone);
esp_err_t sh_get_timezone_list_json(char *buf, size_t buf_length);

#endif // __SYSTIME_HANDLER_H__