#include "camera.h"
#include "camera_handler.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "sd_storage.h"
#include "status_handler.h"
#include "utils.h"
#include "tg_bot.h"

#define TASK_SIZE           (8*1024)

typedef struct camera_handler
{
    int maxPhotos;

    int checkMotionInterval;

    time_t now_ts;
    time_t next_ts;

    SemaphoreHandle_t sem;
} camera_handler_t;

static void camera_handler_task(void *arg);
static esp_err_t take_and_save (char *path, size_t path_len, 
                                char *timestamp, size_t ts_len);

const static char *TAG = "CAM_HANDLER";

static SemaphoreHandle_t take_pic_immediately = NULL;
static camera_handler_t instance = {
    .maxPhotos = 10,
    .now_ts = 0,
    .next_ts = 0,
};

static TaskHandle_t camera_handler_task_handle = NULL;

esp_err_t camera_handler_init(void)
{
    instance.sem = xSemaphoreCreateBinary();
    if (!instance.sem) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_FAIL;
    }
    xSemaphoreGive(instance.sem);

    take_pic_immediately = xSemaphoreCreateBinary();
    if (!take_pic_immediately) {
        ESP_LOGE(TAG, "Failed to create take_pic_immediately semaphore");
        return ESP_FAIL;
    }

    if (xTaskCreateWithCaps(camera_handler_task, "ch_task", TASK_SIZE, NULL, 8, &camera_handler_task_handle, MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create handler task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void camera_handler_update_settings(void)
{
    if (instance.sem) {
        if (xSemaphoreTake(instance.sem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (sd_storage_get_int(SETTINGS_CHECK_MOTION_INT_KEY, &instance.checkMotionInterval) != ESP_OK) {
                instance.checkMotionInterval = 60;
                sd_storage_set_int(SETTINGS_CHECK_MOTION_INT_KEY, instance.checkMotionInterval);
                sd_storage_update_settings();
            }
            if (instance.checkMotionInterval < 1) {
                instance.checkMotionInterval = 60;
                sd_storage_set_int(SETTINGS_CHECK_MOTION_INT_KEY, instance.checkMotionInterval);
                sd_storage_update_settings();
            }
            time(&instance.next_ts);
            instance.next_ts += instance.checkMotionInterval;
            if (sd_storage_get_int(SETTINGS_MAX_PHOTOS, &instance.maxPhotos) != ESP_OK) {
                instance.maxPhotos = 10;
                sd_storage_set_int(SETTINGS_MAX_PHOTOS, instance.maxPhotos);
                sd_storage_update_settings();
            }
            if (instance.maxPhotos < 2) {
                instance.maxPhotos = 2;
            }
            sd_storage_delete_oldest_entries(instance.maxPhotos);

            xSemaphoreGive(instance.sem);
        }
    }
}

esp_err_t camera_capture(void)
{
    esp_err_t ret = ESP_OK;
    xSemaphoreTake(take_pic_immediately, pdMS_TO_TICKS(1000));
    return ret;
}

esp_err_t camera_pause_thread(bool pause)
{
    if (pause) {
        vTaskSuspend(camera_handler_task_handle);
    } else {
        vTaskResume(camera_handler_task_handle);
    }
    return ESP_OK;
}

static esp_err_t take_and_save( char *path, size_t path_len, 
                                char *timestamp, size_t ts_len)
{
    esp_err_t ret = ESP_OK;
    camera_fb_t *pic = camera_take_pic();
    if (pic) {
        int64_t timstmp_int = get_timestamp(timestamp, ts_len);

        memset(path,0, path_len);
        snprintf(path, path_len, "pic_%s.jpeg", timestamp);
        sd_storage_csv_update(path, pic->height * pic->height, timstmp_int);
        camera_save_pic(pic, path);
        tg_bot_send_image(path);

        memset(path,0, path_len);

        camera_release_buf(pic);
    } else {
        ESP_LOGE(TAG, "Failed to take pic");
        ret = ESP_FAIL;
    }
    return ret;
}

static void camera_handler_task(void *arg)
{
    char timestamp[20] = {0};
    char path[70] = {0};

    camera_handler_update_settings();

    time(&instance.now_ts);
    instance.next_ts = instance.checkMotionInterval + instance.now_ts;

    while (1)
    {        
        if (instance.sem) {
            if (xSemaphoreTake(instance.sem, pdMS_TO_TICKS(2000)) == pdTRUE) {

                time(&instance.now_ts);
                if (xSemaphoreTake(take_pic_immediately, pdMS_TO_TICKS(100)) == pdFALSE) {
                    take_and_save(path, sizeof(path), timestamp, sizeof(timestamp));
                    instance.next_ts = instance.checkMotionInterval + instance.now_ts;
                    xSemaphoreGive(take_pic_immediately);
                } else {
                    xSemaphoreGive(take_pic_immediately);
    
                    if (instance.now_ts >= instance.next_ts) {
                        instance.next_ts = instance.checkMotionInterval + instance.now_ts;
    
                        take_and_save(path, sizeof(path), timestamp, sizeof(timestamp));
                    }
                }

                xSemaphoreGive(instance.sem);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

