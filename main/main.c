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

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    nvs_memory_init();

    i2c_init();

    pmu_init();

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
