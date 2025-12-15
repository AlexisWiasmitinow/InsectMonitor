#ifndef _WEBPAGE_H_
#define _WEBPAGE_H_

#include <esp_err.h>
#include <stdbool.h>

esp_err_t webpage_init(void);
esp_err_t webpage_pause(bool pause);

#endif // _WEBPAGE_H_