/**
 * @file remote_server.h
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief 
 * @version 0.1
 * @date 2025-04-22
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef REMOTE_SERVER_H
#define REMOTE_SERVER_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t rs_init(void);
esp_err_t rs_send_image(const uint8_t *data, size_t len);

#endif // REMOTE_SERVER_H
