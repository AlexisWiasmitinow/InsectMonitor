#include <esp_log.h>
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "camera.h"
#include "camera_handler.h"
#include "wifi_handler.h"
#include "webpage.h"
#include "systime_handler.h"
#include "status_handler.h"
#include "utils.h"
#include "box_controller.h"
#include "port_pmu.h"
#include "remote_server.h"
#include "u8g2_esp32_hal.h"

#include <string.h>

static const char *TAG = "MAIN";

static void nvs_memory_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES
        || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

// SDA - GPIO21
#define PIN_SDA 7

// SCL - GPIO22
#define PIN_SCL 6

static void screen_init(void) {
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = PIN_SDA;
    u8g2_esp32_hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_t u8g2;  // a structure which will contain all the data for one display
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2, U8G2_R2,
        // u8x8_byte_sw_i2c,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);  // init u8g2 structure
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);

    ESP_LOGI(TAG, "u8g2_InitDisplay");
    u8g2_InitDisplay(&u8g2);  // send init sequence to the display, display is in
                                // sleep mode after this,

    ESP_LOGI(TAG, "u8g2_SetPowerSave");
    u8g2_SetPowerSave(&u8g2, 0);  // wake up display
    ESP_LOGI(TAG, "u8g2_ClearBuffer");
    u8g2_ClearBuffer(&u8g2);
    ESP_LOGI(TAG, "u8g2_DrawBox");
    u8g2_DrawBox(&u8g2, 0, 26, 80, 6);
    u8g2_DrawFrame(&u8g2, 0, 26, 100, 6);

    ESP_LOGI(TAG, "u8g2_SetFont");
    u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    ESP_LOGI(TAG, "u8g2_DrawStr");
    u8g2_DrawStr(&u8g2, 2, 17, "Hello World!");
    ESP_LOGI(TAG, "u8g2_SendBuffer");
    u8g2_SendBuffer(&u8g2);

    ESP_LOGI(TAG, "All done!");
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    nvs_memory_init();

    i2c_init();

    pmu_init();

    screen_init();

    ret = box_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize box controller (ret:%s)", esp_err_to_name(ret));
    }

    if(status_handler_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize status handler");
    }

    if(camera_init() != ESP_OK) {
        return;
    }

    if(start_wifi(WIFI_HANDLER_MODE_FROM_SETTINGS) != ESP_OK) {
    }
    int await = 0;
    while(await < 60 && !wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // char timezone[30] = {0};
    // sd_storage_get_string(SETTINGS_TZ_KEY, timezone);
    // sh_set_timezone(timezone);

    bool sta = wifi_get_mode() == WIFI_MODE_STA ? true : false;
    ret = sh_init(sta);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to adjust time. Use default system time");
    }

    ret = rs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start webpage: (ret:%d)", ret);
        return;
    }

    ret = camera_handler_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init camera handler unit: (ret:%d)", ret);
        return;
    }

    ret = webpage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start webpage: (ret:%d)", ret);
        return;
    }

    // int i = 0;
    // while (i < 10) {
    //     i++;
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    // ESP_LOGW(TAG, "Going sleep...");
    // vTaskDelay(pdMS_TO_TICKS(500));
    // esp_deep_sleep_start();

    // xTaskCreate(stats_task, "stats", 4 * 1024, NULL, tskIDLE_PRIORITY + 3, NULL);

    // esp_log_level_set("esp_netif_lwip", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_uri", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_txrx", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_parse", ESP_LOG_DEBUG);

    
}
