#ifndef _STATUS_HANDLER_H_
#define _STATUS_HANDLER_H_

#include <esp_err.h>
#include <stdbool.h>

esp_err_t status_handler_init(void);
esp_err_t status_handler_serialize_status(char *buf, int buf_len);
esp_err_t status_handler_motion_detected(bool detected, int64_t timestamp);
esp_err_t status_handler_wifi_set_state(bool state);

#endif // _STATUS_HANDLER_H_