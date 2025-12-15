#ifndef __BOX_CONTROLLER_H__
#define __BOX_CONTROLLER_H__

#include "esp_err.h"
#include <stdbool.h>

#define BOX_CNTR_TEST_LED           "test_led"
#define BOX_CNTR_TEST_MAINTAIN      "test_maintains_mode"

esp_err_t box_controller_init(void);
esp_err_t box_controller_test_pin(char *type, bool *ret_state);

esp_err_t turn_light_on(bool state);

#endif // __BOX_CONTROLLER_H__