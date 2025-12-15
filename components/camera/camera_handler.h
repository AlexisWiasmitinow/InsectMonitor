#ifndef __CAMERA_HANDLER_H__
#define __CAMERA_HANDLER_H__

#include "esp_err.h"

esp_err_t camera_handler_init(void);
void camera_handler_update_settings(void);
esp_err_t camera_capture(void);
esp_err_t camera_pause_thread(bool pause);

#endif // __CAMERA_HANDLER_H__