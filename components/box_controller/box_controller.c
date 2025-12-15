#include "box_controller.h"

#include <sys/time.h>
#include <string.h>

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "sensors_handler.h"
#include "sd_storage.h"
#include "utils.h"

#define HEATER_PIN GPIO_NUM_1
#define FAN_PIN GPIO_NUM_47
#define PUMP_PIN GPIO_NUM_48
#define LIGHT_PIN GPIO_NUM_2
#define PINS_MASK                                                \
  (1ULL << PUMP_PIN) | (1ULL << LIGHT_PIN) | (1ULL << FAN_PIN) | \
      (1ULL << HEATER_PIN)

typedef struct {
  float target_temperature;
  int temp_check_interval;

  int humidity_target;
  int humidity_check_interval;
  int pump_ms_per_h;
} bc_settings_t;

static TimerHandle_t pump_timer;
static SemaphoreHandle_t maintains_sem;
static SemaphoreHandle_t settings_sem;
static TaskHandle_t task_handle;

static volatile bool maintains_mode = false;
static bc_settings_t bc_settings = {
    .temp_check_interval = 10,
    .target_temperature = 25.0f,

    .humidity_target = 60,
    .humidity_check_interval = 10,
    .pump_ms_per_h = 100,
};

static const char *TAG = "BOX_CNTRL";

static void bc_task(void *arg);

static void pump_timer_cb(TimerHandle_t timer) { gpio_set_level(PUMP_PIN, 0); }
static void heater_timer_cb(TimerHandle_t timer) {
  gpio_set_level(HEATER_PIN, 0);
}

esp_err_t box_controller_init(void) {
  esp_err_t ret;

  ret = sens_handler_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG,
             "Failed to initialize temperature and humidity sensor: (ret:%d)",
             ret);
    return ret;
  }

  gpio_config_t gpio_cfg = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pin_bit_mask = PINS_MASK,
  };
  ret = gpio_config(&gpio_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize output pins mask: (ret:%d)", ret);
    return ret;
  }

  gpio_set_level(FAN_PIN, 1);

  maintains_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(maintains_sem);

  settings_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(settings_sem);

  pump_timer = xTimerCreate("pump_tmr", pdMS_TO_TICKS(1000), pdFALSE, NULL,
                            pump_timer_cb);
  if (!pump_timer) {
    ESP_LOGE(TAG, "Failed to create pump timer");
    return ESP_FAIL;
  }

  xTaskCreateWithCaps(bc_task, "bc_task", 6 * 1024, NULL, 5, &task_handle, MALLOC_CAP_SPIRAM);
  return ret;
}

esp_err_t box_controller_test_pin(char *type, bool *ret_state) {
  esp_err_t ret = ESP_OK;
  if (!type || !ret_state) {
    ret = ESP_FAIL;
  } else {
    if (!strcmp(type, BOX_CNTR_TEST_LED)) {
      static bool state = true;
      gpio_set_level(LIGHT_PIN, state);
      *ret_state = state;
      state = !state;
    } else if (!strcmp(type, BOX_CNTR_TEST_HEATER)) {
      if (maintains_mode) {
        static bool state = true;
        gpio_set_level(HEATER_PIN, state);
        *ret_state = state;
        state = !state;
      } else {
        ret = ESP_FAIL;
      }
    } else if (!strcmp(type, BOX_CNTR_TEST_PUMP)) {
      if (maintains_mode) {
        static bool state = true;
        gpio_set_level(PUMP_PIN, state);
        *ret_state = state;
        state = !state;
      } else {
        ret = ESP_FAIL;
      }
    } else if (!strcmp(type, BOX_CNTR_TEST_FAN)) {
      static bool state = true;
      gpio_set_level(FAN_PIN, state);
      *ret_state = state;
      state = !state;
    } else if (!strcmp(type, BOX_CNTR_TEST_MAINTAIN)) {
      if (xSemaphoreTake(maintains_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ret = ESP_ERR_TIMEOUT;
      } else {
        maintains_mode = !maintains_mode;
        *ret_state = maintains_mode;

        xTimerStop(pump_timer, 0);
        gpio_set_level(PUMP_PIN, 0);
        gpio_set_level(HEATER_PIN, 0);

        xSemaphoreGive(maintains_sem);
      }
    } else {
      ret = ESP_ERR_INVALID_ARG;
    }
  }

  return ret;
}

esp_err_t box_controller_reload_settings(void) {
  if (settings_sem) {
    if (xSemaphoreTake(settings_sem, pdMS_TO_TICKS(5000)) == pdTRUE) {
      if (sd_storage_get_float(SETTINGS_TEMP_TARGET_KEY,
                               &bc_settings.target_temperature) != ESP_OK) {
        bc_settings.target_temperature = 25.0f;
        sd_storage_set_float(SETTINGS_TEMP_TARGET_KEY,
                             bc_settings.target_temperature);
        sd_storage_update_settings();
      }
      if (sd_storage_get_int(SETTINGS_TEMP_CHECK_INTERVAL_KEY,
                             &bc_settings.temp_check_interval) != ESP_OK) {
        bc_settings.temp_check_interval = 10;
        sd_storage_set_int(SETTINGS_TEMP_CHECK_INTERVAL_KEY,
                           bc_settings.temp_check_interval);
        sd_storage_update_settings();
      }
      if (bc_settings.temp_check_interval < 1) {
        bc_settings.temp_check_interval = 1;
        sd_storage_set_int(SETTINGS_TEMP_CHECK_INTERVAL_KEY,
                           bc_settings.temp_check_interval);
        sd_storage_update_settings();
      }

      if (sd_storage_get_int(SETTINGS_HUMIDITY_TARGET_KEY,
                             &bc_settings.humidity_target) != ESP_OK) {
        bc_settings.humidity_target = 60;
        sd_storage_set_int(SETTINGS_HUMIDITY_TARGET_KEY,
                           bc_settings.humidity_target);
        sd_storage_update_settings();
      }
      if (sd_storage_get_int(SETTINGS_PUMP_MS_PER_H_KEY,
                             &bc_settings.pump_ms_per_h) != ESP_OK) {
        bc_settings.pump_ms_per_h = 100;
        sd_storage_set_int(SETTINGS_PUMP_MS_PER_H_KEY,
                           bc_settings.pump_ms_per_h);
        sd_storage_update_settings();
      }
      if (bc_settings.pump_ms_per_h < 10) {
        bc_settings.pump_ms_per_h = 10;
        sd_storage_set_int(SETTINGS_PUMP_MS_PER_H_KEY,
                           bc_settings.pump_ms_per_h);
        sd_storage_update_settings();
      }

      if (sd_storage_get_int(SETTINGS_HUM_CHECK_INTERVAL_KEY,
                             &bc_settings.humidity_check_interval) != ESP_OK) {
        bc_settings.humidity_check_interval = 4;
        sd_storage_set_int(SETTINGS_HUM_CHECK_INTERVAL_KEY,
                           bc_settings.humidity_check_interval);
        sd_storage_update_settings();
      }
      if (bc_settings.humidity_check_interval < 1) {
        bc_settings.humidity_check_interval = 1;
        sd_storage_set_int(SETTINGS_HUM_CHECK_INTERVAL_KEY,
                           bc_settings.humidity_check_interval);
        sd_storage_update_settings();
      }

      xTimerChangePeriod(pump_timer, pdMS_TO_TICKS(bc_settings.pump_ms_per_h),
                         pdMS_TO_TICKS(100));

      xSemaphoreGive(settings_sem);

      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

esp_err_t turn_light_on(bool state) {
  ESP_LOGD(TAG, "Turning light on: %d", state);
  return gpio_set_level(LIGHT_PIN, state);
}

esp_err_t box_controller_pause_thread(bool pause) {
  if (pause) {
    vTaskSuspend(task_handle);
  } else {
    vTaskResume(task_handle);
  }
  return ESP_OK;
}

static void update_temperature(float target_temperature) {
  float temp = 0.f;
  if (sens_handler_get_temp(&temp) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get temperature");
  }

  temp = round_double(temp, 1);
  ESP_LOGI(TAG, "Tnow: (%.1f) | Ttarget: (%.1f)", temp, target_temperature);

  uint8_t state = 1;
  if (temp > target_temperature) {
    state = 0;
  }
  gpio_set_level(HEATER_PIN, state);

  time_t ts = 0;
  time(&ts);
  sd_storage_data_logs_add_entry(DATA_LOGS_TYPE_TEMPERATURE, temp, state, ts);
}

static void update_humidity(float humidity_target) {
  float hum = 0.f;
  if (sens_handler_get_humidity(&hum) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get humidity");
  }
  hum = round_double(hum, 1);
  uint16_t pumpTime = 0;
  double delta = humidity_target - hum;
  if (delta > 0) {
    pumpTime = (uint16_t)(bc_settings.pump_ms_per_h * delta);
    if (pumpTime < 10) {
      pumpTime = 10;
    } else if (pumpTime > 200) {
      pumpTime = 200;
    }
    xTimerStop(pump_timer, 0);
    xTimerChangePeriod(pump_timer, pdMS_TO_TICKS(pumpTime), pdMS_TO_TICKS(100));
    xTimerStart(pump_timer, pdMS_TO_TICKS(100));
    gpio_set_level(PUMP_PIN, 1);
  } else {
    pumpTime = 0;
    gpio_set_level(PUMP_PIN, 0);
  }

  ESP_LOGI(TAG, "Hnow: (%.1f) | Htarget: (%.1f) | delta: (%.1f) pumpTime: (%d)",
           hum, humidity_target, delta, pumpTime);

  time_t ts = 0;
  time(&ts);
  sd_storage_data_logs_add_entry(DATA_LOGS_TYPE_HUMIDITY, hum, pumpTime, ts);
}

static void bc_task(void *arg) {
  bool pump_skip_first_cycle = true;
  time_t now_ms = 0, pump_ts = 0, heater_ts = 0;
  struct timeval ts = {0};

  box_controller_reload_settings();

  while (1) {
    if (xSemaphoreTake(maintains_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (!maintains_mode) {
        gettimeofday(&ts, NULL);

        now_ms = (ts.tv_sec) * 1000 + ts.tv_usec / 1000;

        if (xSemaphoreTake(settings_sem, pdMS_TO_TICKS(2000) == pdTRUE)) {
          if (now_ms > heater_ts) {
            update_temperature(bc_settings.target_temperature);
            heater_ts = now_ms + bc_settings.temp_check_interval * 1000;
          }

          if (now_ms > pump_ts) {
            if (pump_skip_first_cycle) {
              pump_skip_first_cycle = false;
            } else {
              update_humidity(bc_settings.humidity_target);
            }
            pump_ts = now_ms + bc_settings.humidity_check_interval * 1000;
          }
          xSemaphoreGive(settings_sem);
        }
      }

      xSemaphoreGive(maintains_sem);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
