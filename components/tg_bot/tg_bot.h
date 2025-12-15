/**
 * @file tg_bot.h
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief 
 * @version 0.1
 * @date 2025-04-22
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef TG_BOT_H
#define TG_BOT_H

#include "esp_err.h"

esp_err_t tg_bot_init(void);
esp_err_t tg_bot_send_image(const char *path);

#endif // TG_BOT_H
