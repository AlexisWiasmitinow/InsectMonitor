#ifndef _SD_STORAGE_H_
#define _SD_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

#define SETTINGS_WIFI_SSID_KEY          "ssid"
#define SETTINGS_WIFI_PASS_KEY          "password"
#define SETTINGS_WIFI_MODE_KEY          "wifiMode"

#define SETTINGS_CHECK_MOTION_INT_KEY   "checkMotionInterval"
#define SETTINGS_MAX_PHOTOS             "maxPhotos"

#define SETTINGS_TZ_KEY                 "timezone"

#define SETTINGS_TEMP_TARGET_KEY        "temperatureTarget"
#define SETTINGS_TEMP_CHECK_INTERVAL_KEY  "tempCheckInterval"

#define SETTINGS_HUMIDITY_TARGET_KEY    "humidityTarget"
#define SETTINGS_PUMP_MS_PER_H_KEY      "pumpMsPerH"
#define SETTINGS_HUM_CHECK_INTERVAL_KEY "humidityCheckInterval"
#define SETTINGS_API_TOKEN_KEY          "apiToken"
#define SETTINGS_DEVICEID_KEY           "deviceId"

#define DEFAULT_BASE_PATH               "/sdcard"
#define PICS_PATH                       DEFAULT_BASE_PATH"/pics"
#define DATA_LOGS_PATH                  DEFAULT_BASE_PATH"/logs"
#define CSV_FILE_EXT                    ".csv"

esp_err_t sd_storage_init(void);
esp_err_t sd_storage_update_settings(void);
esp_err_t sd_storage_get_float(char *key, float *val);
esp_err_t sd_storage_set_float(char *key, float val);
esp_err_t sd_storage_get_int(char *key, int *val);
esp_err_t sd_storage_set_int(char *key, int val);
esp_err_t sd_storage_get_string(char *key, char *val);
esp_err_t sd_storage_set_string(char *key, char *val, size_t len);
esp_err_t sd_storage_clear_pics_folder(void);
esp_err_t sd_storage_delete_oldest_entries(int max_photos);
int sd_storage_count_files_with_ext(const char *extension);
int sd_storage_get_list_of_files_with_ext(char *list, int list_len, const char *path, const char *extension);

esp_err_t sd_storage_csv_init(void);
esp_err_t sd_storage_set_max_photos(int _max_photos);
esp_err_t sd_storage_csv_update(char *pic_path, size_t size, long long timestamp);
long long sd_storage_csv_get_oldest_photo(char *oldest_photo_name);
long long sd_storage_csv_get_newest_photo(char *newest_photo_name);
int sd_storage_csv_remove_entry(const char *value);

typedef enum  {
    DATA_LOGS_TYPE_HUMIDITY,
    DATA_LOGS_TYPE_TEMPERATURE,
} data_logs_type_t;
esp_err_t sd_storage_data_logs_add_entry(data_logs_type_t type, float value, char state, long long timestamp);
esp_err_t sd_storage_data_logs_get_list(char *list, int list_len);

#ifdef __cplusplus
}
#endif

#endif // _SD_STORAGE_H_