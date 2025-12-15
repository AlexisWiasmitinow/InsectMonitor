#include "sensors_handler.h"

#include "htu21d.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define I2C_PINS_SDA        GPIO_NUM_14
#define I2C_PINS_SCL        GPIO_NUM_21

static bool temp_hum_sens_initialised = false;

static SemaphoreHandle_t share_resource_sem = NULL;

static const char *TAG = "SENS_HANDLER";

esp_err_t sens_handler_init(void)
{
    share_resource_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(share_resource_sem);

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_PINS_SDA,
        .scl_io_num = I2C_PINS_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 400000
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0));

    if (htu21d_init(I2C_NUM_0) != ESP_OK) {
        return ESP_FAIL;
    }

    temp_hum_sens_initialised = true;
    return ESP_OK;
}

esp_err_t sens_handler_get_temp(float *temp)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(share_resource_sem, pdMS_TO_TICKS(1000)) && temp_hum_sens_initialised) {
        float _temp = htu21d_read_temperature();
        if (_temp != -1000) {
            ret = ESP_OK;
        }
        *temp = _temp;
        xSemaphoreGive(share_resource_sem);
    } else {
        *temp = -1000;
    }

    return ret;
}

esp_err_t sens_handler_get_humidity(float *humidity)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(share_resource_sem, pdMS_TO_TICKS(1000)) && temp_hum_sens_initialised) {
        float _hum = htu21d_read_humidity();
        if (_hum != -1000) {
            ret = ESP_OK;
        }
        *humidity = _hum;
        xSemaphoreGive(share_resource_sem);
    } else {
        *humidity = -1000;
    }

    return ret;
}