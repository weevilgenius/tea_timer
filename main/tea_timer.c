#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"

static const char *TAG = "tea_timer";

/* PCNT handles */
static pcnt_unit_handle_t s_pcnt_unit = NULL;
static int s_last_count = 0;

/* Initialize rotary encoder using hardware PCNT */
void encoder_init(void) {
  /* Configure PCNT unit */
  pcnt_unit_config_t unit_config = {
    .high_limit = INT16_MAX,
    .low_limit = INT16_MIN,
  };
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_pcnt_unit));

  /* Configure glitch filter (1000ns = 1us) */
  pcnt_glitch_filter_config_t filter_config = {
    .max_glitch_ns = 1000,
  };
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config));

  /* Configure channel A (edge on GPIO41, level on GPIO40) */
  pcnt_chan_config_t chan_a_config = {
    .edge_gpio_num = BSP_ENCODER_A,
    .level_gpio_num = BSP_ENCODER_B,
  };
  pcnt_channel_handle_t pcnt_chan_a = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_a_config, &pcnt_chan_a));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,
    PCNT_CHANNEL_EDGE_ACTION_DECREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  /* Configure channel B (edge on GPIO40, level on GPIO41) for full quadrature */
  pcnt_chan_config_t chan_b_config = {
    .edge_gpio_num = BSP_ENCODER_B,
    .level_gpio_num = BSP_ENCODER_A,
  };
  pcnt_channel_handle_t pcnt_chan_b = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_b_config, &pcnt_chan_b));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
    PCNT_CHANNEL_EDGE_ACTION_DECREASE,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  /* Enable and start */
  ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
  ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

  ESP_LOGI(TAG, "Hardware PCNT encoder initialized");
}

/* Get current encoder count */
int encoder_get_count(void) {
  int count = 0;
  pcnt_unit_get_count(s_pcnt_unit, &count);
  return count;
}

/* Application start */
void app_main(void) {

  /* Initialize display and start LVGL handling task */
  lv_display_t *disp = bsp_display_start();
  if (disp == NULL) {
    ESP_LOGE(TAG, "bsp_display_start() failed");
    return;
  }

  /* Backlight is enabled separately */
  bsp_display_backlight_on();

  /* Create a simple "Hello" label */
  bsp_display_lock(0); /* 0 = block indefinitely */
  {
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, Dial!");
    lv_obj_center(label);
  }
  bsp_display_unlock();

  ESP_LOGI(TAG, "LVGL hello displayed");

  /* Initialize rotary encoder with hardware PCNT */
  encoder_init();

  /* Main loop - poll encoder and handle events */
  while (1) {
    int count = encoder_get_count();
    if (count != s_last_count) {
      int delta = count - s_last_count;
      if (delta > 0) {
        ESP_LOGI(TAG, "right, count=%d", count);
      } else {
        ESP_LOGI(TAG, "left, count=%d", count);
      }
      s_last_count = count;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
