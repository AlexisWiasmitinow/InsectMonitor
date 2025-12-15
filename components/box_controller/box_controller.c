#include "box_controller.h"

#include <sys/time.h>
#include <string.h>

#include "driver/ledc.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "utils.h"

#define LIGHT_PIN GPIO_NUM_16
#define PINS_MASK (1ULL << LIGHT_PIN)

static TimerHandle_t pump_timer;
static SemaphoreHandle_t maintain_sem;
static SemaphoreHandle_t settings_sem;
static TaskHandle_t task_handle;

static volatile bool maintenance_mode = false;

static const char* TAG = "BOX_CNTRL";

esp_err_t box_controller_init(void) {
  esp_err_t ret;

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

  maintain_sem = xSemaphoreCreateBinary();
  xSemaphoreGive(maintain_sem);

  return ret;
}

esp_err_t box_controller_test_pin(char* type, bool* ret_state) {
  esp_err_t ret = ESP_OK;
  if (!type || !ret_state) {
    ret = ESP_FAIL;
  } else {
    if (!strcmp(type, BOX_CNTR_TEST_LED)) {
      static bool state = true;
      gpio_set_level(LIGHT_PIN, state);
      *ret_state = state;
      state = !state;
    } else if (!strcmp(type, BOX_CNTR_TEST_MAINTAIN)) {
      if (xSemaphoreTake(maintain_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ret = ESP_ERR_TIMEOUT;
      } else {
        maintenance_mode = !maintenance_mode;
        *ret_state = maintenance_mode;
        xSemaphoreGive(maintain_sem);
      }
    } else {
      ret = ESP_ERR_INVALID_ARG;
    }
  }

  return ret;
}

esp_err_t turn_light_on(bool state) {
  ESP_LOGD(TAG, "Turning light on: %d", state);
  return gpio_set_level(LIGHT_PIN, state);
}
