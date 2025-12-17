#include "box_controller.h"

#include <sys/time.h>
#include <string.h>

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "camera_handler.h"
#include "utils.h"

#define LIGHT_PIN GPIO_NUM_16
#define TRIGGER_PIN GPIO_NUM_16 // onboard PIR sensor
#define PINS_MASK_OUTPUT (1ULL << LIGHT_PIN)
#define PINS_MASK_INPUT (1ULL << TRIGGER_PIN)

static SemaphoreHandle_t maintain_sem;

static volatile bool maintenance_mode = false;

static const char* TAG = "BOX_CNTRL";

static IRAM_ATTR void trigger_pin_handler(void *arg) {
  camera_capture();
}

esp_err_t box_controller_init(void) {
  esp_err_t ret;

  gpio_config_t gpio_cfg = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pin_bit_mask = PINS_MASK_OUTPUT,
  };
  ret = gpio_config(&gpio_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize output pins mask: (ret:%d)", ret);
    return ret;
  }

  gpio_cfg.intr_type = GPIO_INTR_POSEDGE;
  gpio_cfg.mode = GPIO_MODE_INPUT;
  gpio_cfg.pin_bit_mask = PINS_MASK_INPUT;
  ret = gpio_config(&gpio_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize output pins mask: (ret:%d)", ret);
    return ret;
  }

  gpio_install_isr_service(0);
  gpio_isr_handler_add(TRIGGER_PIN, trigger_pin_handler, NULL);

  esp_sleep_enable_ext0_wakeup(TRIGGER_PIN, 1);

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
