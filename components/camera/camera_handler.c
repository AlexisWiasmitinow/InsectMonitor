#include "camera.h"
#include "camera_handler.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "status_handler.h"
#include "utils.h"
#include "remote_server.h"

#define TASK_SIZE           (8*1024)

SemaphoreHandle_t sem;

static void camera_handler_task(void *arg);

const static char *TAG = "CAM_HANDLER";

static TaskHandle_t camera_handler_task_handle = NULL;

esp_err_t camera_handler_init(void)
{
    sem = xSemaphoreCreateBinary();
    if (!sem) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_FAIL;
    }
    xSemaphoreTake(sem, 0);

    if (xTaskCreateWithCaps(camera_handler_task, "ch_task", TASK_SIZE, NULL, 8, &camera_handler_task_handle, MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create handler task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void camera_handler_update_settings(void)
{

}

esp_err_t camera_capture(void)
{
    esp_err_t ret = ESP_OK;
    xSemaphoreGive(sem);
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

static esp_err_t take_and_send(void)
{
    esp_err_t ret = ESP_OK;
    camera_fb_t *pic = camera_take_pic();
    if (pic) {
        rs_send_image(pic->buf, pic->len);
        camera_release_buf(pic);
    } else {
        ESP_LOGE(TAG, "Failed to take pic");
        ret = ESP_FAIL;
    }
    return ret;
}

static void camera_handler_task(void *arg)
{
    camera_handler_update_settings();

    while (1)
    {        
        if (sem) {
            if (xSemaphoreTake(sem, portMAX_DELAY)) {
                take_and_send();
            }
        }
    }
}

