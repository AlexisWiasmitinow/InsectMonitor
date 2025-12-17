/**
 * @file port_pmu.c
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief 
 * @version 0.1
 * @date 2025-12-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "port_pmu.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "screen.h"

extern void pmu_go_sleep(void);

void pmu_sleep(void) {
    pmu_go_sleep();
    screen_setPowerSave(true);
    esp_deep_sleep_start();
}