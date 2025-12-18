#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "iot_button.h"
#include "button_gpio.h"

static const char *TAG = "tea_timer";

/* PCNT handles */
static pcnt_unit_handle_t s_pcnt_unit = NULL;
static int s_last_count = 0;

/* UI state */
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_count_label = NULL;
static int s_color_index = 0;

/* Background colors to cycle through */
static const lv_color_t s_colors[] = {
  LV_COLOR_MAKE(255, 255, 255), /* White (default) */
  LV_COLOR_MAKE(255, 0, 0),     /* Red */
  LV_COLOR_MAKE(0, 255, 0),     /* Green */
  LV_COLOR_MAKE(0, 0, 255),     /* Blue */
};
#define NUM_COLORS (sizeof(s_colors) / sizeof(s_colors[0]))

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

/* Button callback */
static void button_press_cb(void *button_handle, void *usr_data) {
  ESP_LOGI(TAG, "Faceplate button pressed!");

  /* Not supposed to block in a button callback */
  if (!bsp_display_lock(5)) { return; }

  /* Cycle to next background color */
  s_color_index = (s_color_index + 1) % NUM_COLORS;
  lv_obj_set_style_bg_color(s_screen, s_colors[s_color_index], 0);
  bsp_display_unlock();
}

/* Initialize faceplate button */
void button_init(void) {
  const button_config_t btn_cfg = {0};  /* Use defaults */
  const button_gpio_config_t gpio_cfg = {
    .gpio_num = BSP_BTN_PRESS,
    .active_level = 0,  /* Active low */
  };

  button_handle_t btn = NULL;
  ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn));
  ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_press_cb, NULL));

  ESP_LOGI(TAG, "Faceplate button initialized");
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

  /* Create UI elements */
  bsp_display_lock(0); /* 0 = block indefinitely */
  {
    s_screen = lv_scr_act();

    /* "Hello, Dial!" label */
    lv_obj_t *hello_label = lv_label_create(s_screen);
    lv_label_set_text(hello_label, "Hello, Dial!");
    lv_obj_align(hello_label, LV_ALIGN_CENTER, 0, -15);

    /* Encoder count label */
    s_count_label = lv_label_create(s_screen);
    lv_label_set_text(s_count_label, "Count: 0");
    lv_obj_align(s_count_label, LV_ALIGN_CENTER, 0, 15);
  }
  bsp_display_unlock();

  ESP_LOGI(TAG, "LVGL hello displayed");

  /* Initialize rotary encoder with hardware PCNT */
  encoder_init();

  /* Initialize faceplate button */
  button_init();

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

      /* Update display */
      bsp_display_lock(0);
      lv_label_set_text_fmt(s_count_label, "Count: %d", count);
      bsp_display_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
