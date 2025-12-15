#include "systime_handler.h"

#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "cJSON.h"
#include "sd_storage.h"

static const char *TAG = "SYSTIME";

typedef struct tz {
  const char* name;   // Full name of the timezone
  const char* posix;  // POSIX timezone string
} tz_t;

const tz_t timezones[] = {
    {"Coordinated Universal Time", "UTC0"},
    {"Greenwich Mean Time", "GMT0"},
    {"US Eastern Time", "EST5EDT,M3.2.0,M11.1.0"},
    {"US Central Time", "CST6CDT,M3.2.0,M11.1.0"},
    {"US Mountain Time", "MST7MDT,M3.2.0,M11.1.0"},
    {"US Pacific Time", "PST8PDT,M3.2.0,M11.1.0"},
    {"Central European Time", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Japan Standard Time", "JST-9"},
    {"India Standard Time", "IST-5:30"},
    {"Australian Eastern Time", "AEST-10AEDT,M10.1.0,M4.1.0/3"}
};

esp_err_t sh_get_timezone_list_json(char *buf, size_t buf_length)
{
    esp_err_t ret = ESP_FAIL;
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < sizeof(timezones) / sizeof(timezones[0]); i++){
        cJSON *item = cJSON_CreateObject();

        cJSON_AddStringToObject(item, "name", timezones[i].name);
        cJSON_AddStringToObject(item, "posix", timezones[i].posix);
        cJSON_AddItemToArray(root, item);
    }
    ret = cJSON_PrintPreallocated(root, buf, buf_length, 0) ? ESP_OK : ESP_FAIL;
    cJSON_Delete(root);
    return ret;
}

esp_err_t sh_get_timezone(char *timezone)
{
    esp_err_t ret = ESP_OK;
    if (sd_storage_get_string(SETTINGS_TZ_KEY, timezone) != ESP_OK) {
        ret = sd_storage_set_string(SETTINGS_TZ_KEY, "UTC0", 4);
    }
    return ret;
}

esp_err_t sh_set_timezone(char *timezone)
{
    esp_err_t ret= ESP_OK;
    if (timezone != NULL) {
        setenv("TZ", timezone, 1);
        tzset();
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }
    return ret;
}

esp_err_t sh_init(bool is_sta)
{
    esp_netif_sntp_deinit();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP (err: %s)", esp_err_to_name(ret));
        return ret;
    }

    if (is_sta) {
        ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(60000));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update system time within a timeout (err: %s)", esp_err_to_name(ret));
        }
    } else {
        ret = esp_netif_sntp_sync_wait(1);
    }

    return ret;
}