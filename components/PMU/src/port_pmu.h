/**
 * @file port_pmu.h
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief 
 * @version 0.1
 * @date 2025-12-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once

#include "esp_err.h"
extern esp_err_t pmu_init(void);
extern void pmu_isr_handler(void);
extern esp_err_t i2c_init(void);
