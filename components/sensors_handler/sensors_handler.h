#ifndef __SENSORS_HANDLER_H__
#define __SENSORS_HANDLER_H__

#include "esp_err.h"

esp_err_t sens_handler_init(void);
esp_err_t sens_handler_get_temp(float *temp);
esp_err_t sens_handler_get_humidity(float *humidity);

#endif // __SENSORS_HANDLER_H__