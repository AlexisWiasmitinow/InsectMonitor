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
#include "sd_storage.h"
#include "wifi_handler.h"
#include "webpage.h"
#include "systime_handler.h"
#include "status_handler.h"
#include "utils.h"
#include "box_controller.h"
// #include "tg_bot.h"

#include <string.h>

static const char *TAG = "MAIN";

static void nvs_memory_init()
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

static inline void check_device_id(void)
{
    char mac_settings[18] = {0};
    char mac_cmp[18] = {0};
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BASE);
    sprintf(mac_cmp, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if((sd_storage_get_string(SETTINGS_DEVICEID_KEY, mac_settings) != ESP_OK)) {
        ESP_LOGE(TAG, "Failed to get device ID from storage");
        sd_storage_set_string(SETTINGS_DEVICEID_KEY, mac_cmp, strlen(mac_cmp));
        sd_storage_update_settings();
    }
    if (strcmp(mac_settings, mac_cmp)) {
        ESP_LOGE(TAG, "Device ID mismatch");
        sd_storage_set_string(SETTINGS_DEVICEID_KEY, mac_cmp, strlen(mac_cmp));
        sd_storage_update_settings();
    } else {
        ESP_LOGI(TAG, "Device ID is %s", mac_cmp);
    }
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    nvs_memory_init();

    if(sd_storage_init() != ESP_OK) {
        return;
    }

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

    check_device_id();

    if(start_wifi(WIFI_HANDLER_MODE_FROM_SETTINGS) != ESP_OK) {
        char ssid[20] = {0};
        char pass[20] = {0};
        char mode[20] = {0};
        sd_storage_get_string(SETTINGS_WIFI_SSID_KEY, ssid);
        sd_storage_get_string(SETTINGS_WIFI_PASS_KEY, pass);
        sd_storage_get_string(SETTINGS_WIFI_MODE_KEY, mode);
        ESP_LOGE(TAG, "Failed to start WIFI: (ssid:%s) (pass:%s) (mode:%s)", ssid, pass, mode);
        return;
    }
    int await = 0;
    while(await < 60 && !wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    char timezone[30] = {0};
    sd_storage_get_string(SETTINGS_TZ_KEY, timezone);
    sh_set_timezone(timezone);

    bool sta = wifi_get_mode() == WIFI_MODE_STA ? true : false;
    ret = sh_init(sta);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to adjust time. Use default system time");
    }

    // ret = tg_bot_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to start webpage: (ret:%d)", ret);
    //     return;
    // }

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

    int i = 0;
    while (i < 10) {
        i++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGW(TAG, "Going sleep...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_deep_sleep_start();

    // xTaskCreate(stats_task, "stats", 4 * 1024, NULL, tskIDLE_PRIORITY + 3, NULL);

    // esp_log_level_set("esp_netif_lwip", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_uri", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_txrx", ESP_LOG_DEBUG);
    // esp_log_level_set("httpd_parse", ESP_LOG_DEBUG);
}
