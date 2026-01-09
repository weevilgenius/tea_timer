#include <bsp/esp-bsp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/pulse_cnt.h>

#include <lvgl.h>
#include <iot_button.h>
#include <button_gpio.h>

#include "view.h"
#include "logic.h"

// uncomment to rotate UI: 90, 180, or 270 degrees
//#define ROTATE_UI 90

#define USE_BUZZER 0

#if USE_BUZZER
#include "buzzer.h"
#endif

static const char *TAG = "tea_timer";

/* Event types for thread-safe event loop */
typedef enum {
    EVENT_NONE,
    EVENT_BUTTON_PRESS,
    EVENT_ENCODER_CHANGE,  // .value holds new absolute count
    EVENT_TICK_1HZ,        // For countdown
    EVENT_TICK_FAST,       // For alarm flashing (4Hz)
    EVENT_INACTIVITY       // For sleep timeout
} event_type_t;

typedef struct {
    event_type_t type;
    int32_t value;
} app_event_t;

/* Event queue handle */
static QueueHandle_t s_event_queue = NULL;

/* Timer handles */
static esp_timer_handle_t s_tick_timer = NULL;
static esp_timer_handle_t s_fast_timer = NULL;

/* Inactivity timeout (1 minutes) */
#define INACTIVITY_TIMEOUT_MS  (60 * 1000)
static uint32_t s_last_activity_ms = 0;

/**
 * 1Hz tick timer callback - sends EVENT_TICK_1HZ to queue
 */
static void tick_timer_cb(void *arg) {
    (void)arg;
    app_event_t evt = { .type = EVENT_TICK_1HZ, .value = 0 };
    xQueueSendFromISR(s_event_queue, &evt, NULL);
}

/**
 * Fast tick timer callback - sends EVENT_TICK_FAST to queue
 */
static void fast_timer_cb(void *arg) {
    (void)arg;
    app_event_t evt = { .type = EVENT_TICK_FAST, .value = 0 };
    xQueueSendFromISR(s_event_queue, &evt, NULL);
}

/* PCNT handles */
static pcnt_unit_handle_t s_pcnt_unit = NULL;
static int s_last_polled_encoder = 0;

/* Application state (managed by logic module) */
static app_state_t s_app_state;

/**
 * Map logic state to view state
 */
static view_state_t state_to_view(tea_state_t state) {
    switch (state) {
        case STATE_SETUP:   return VIEW_STATE_SETUP;
        case STATE_RUNNING: return VIEW_STATE_RUNNING;
        case STATE_ALARM:   return VIEW_STATE_ALARM;
        case STATE_SLEEP:   return VIEW_STATE_SLEEP;
        default:            return VIEW_STATE_SETUP;
    }
}

/**
 * Map queue event to logic event
 */
static logic_event_t event_to_logic(event_type_t type) {
    switch (type) {
        case EVENT_BUTTON_PRESS:   return EVT_BUTTON_PRESS;
        case EVENT_ENCODER_CHANGE: return EVT_ENCODER_CHANGE;
        case EVENT_TICK_1HZ:       return EVT_TICK_1HZ;
        case EVENT_TICK_FAST:      return EVT_TICK_FAST;
        case EVENT_INACTIVITY:     return EVT_INACTIVITY_TIMEOUT;
        default:                   return EVT_NONE;
    }
}

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

/* Button callback - sends event to queue, no UI code allowed here */
static void button_press_cb(void *button_handle, void *usr_data) {
  ESP_LOGI(TAG, "Button pressed - sending event");
  app_event_t evt = { .type = EVENT_BUTTON_PRESS, .value = 0 };
  xQueueSendFromISR(s_event_queue, &evt, NULL);
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

#if USE_BUZZER
  /* Initializing the buzzer can turn off the screen backlight, so do it first */
  ESP_ERROR_CHECK(buzzer_init());
#endif

  /* Initialize display and start LVGL handling task */
  lv_display_t *disp = bsp_display_start();
  if (disp == NULL) {
    ESP_LOGE(TAG, "bsp_display_start() failed");
    return;
  }

#ifdef ROTATE_UI
#if ROTATE_UI == 90
  bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90);
#elif ROTATE_UI == 180
  bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
#elif ROTATE_UI == 270
  bsp_display_rotate(disp, LV_DISPLAY_ROTATION_270);
#elif ROTATE_UI != 0
  #error "ROTATE_UI must be 0, 90, 180, or 270"
#endif
#endif

  /* Backlight is enabled separately */
  bsp_display_backlight_on();

  /* Initialize tea timer UI */
  view_init();
  ESP_LOGI(TAG, "Tea timer UI initialized");

  /* Initialize application state */
  logic_init(&s_app_state);

  /* Create event queue before hardware init (callbacks use queue) */
  s_event_queue = xQueueCreate(10, sizeof(app_event_t));
  if (s_event_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create event queue");
    return;
  }

  /* Create 1Hz tick timer (not started yet) */
  const esp_timer_create_args_t tick_timer_args = {
    .callback = tick_timer_cb,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "tick_1hz"
  };
  ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &s_tick_timer));

  /* Create fast timer for alarm flashing (not started yet) */
  const esp_timer_create_args_t fast_timer_args = {
    .callback = fast_timer_cb,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "tick_fast"
  };
  ESP_ERROR_CHECK(esp_timer_create(&fast_timer_args, &s_fast_timer));

  /* Initialize rotary encoder with hardware PCNT */
  encoder_init();

  /* Initialize faceplate button */
  button_init();

  /* Initial UI update */
  bsp_display_lock(0);
  view_update(state_to_view(s_app_state.state),
              s_app_state.target_time_secs,
              logic_get_progress(&s_app_state));
  bsp_display_unlock();

  /* Initialize activity tracking */
  s_last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

  /* Main loop - event-driven architecture */
  while (1) {
    uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* 1. Poll encoder and send event if changed */
    int count = encoder_get_count();
    if (count != s_last_polled_encoder) {
      app_event_t evt = { .type = EVENT_ENCODER_CHANGE, .value = count };
      xQueueSend(s_event_queue, &evt, 0);
      s_last_polled_encoder = count;
    }

    /* 2. Check for inactivity timeout (only in SETUP state) */
    if (s_app_state.state == STATE_SETUP) {
      if ((current_ms - s_last_activity_ms) >= INACTIVITY_TIMEOUT_MS) {
        app_event_t evt = { .type = EVENT_INACTIVITY, .value = 0 };
        xQueueSend(s_event_queue, &evt, 0);
        /* Reset to prevent repeated timeout events */
        s_last_activity_ms = current_ms;
      }
    }

    /* 3. Process events from queue (10ms timeout allows encoder polling) */
    app_event_t evt;
    if (xQueueReceive(s_event_queue, &evt, pdMS_TO_TICKS(10))) {
      /* Reset activity timer on user input */
      if (evt.type == EVENT_BUTTON_PRESS || evt.type == EVENT_ENCODER_CHANGE) {
        s_last_activity_ms = current_ms;
      }

      /* Convert and process event through logic module */
      logic_event_t logic_evt = event_to_logic(evt.type);
      uint32_t actions = logic_process_event(&s_app_state, logic_evt, evt.value);

      /* Handle requested actions */
      if (actions & ACTION_BACKLIGHT_OFF) {
        bsp_display_backlight_off();
        ESP_LOGI(TAG, "Backlight OFF (sleep)");
      }

      if (actions & ACTION_BACKLIGHT_ON) {
        bsp_display_backlight_on();
        ESP_LOGI(TAG, "Backlight ON (wake)");
      }

      if (actions & ACTION_START_TIMER) {
        ESP_LOGI(TAG, "Timer started: %lu seconds", s_app_state.remaining_time_secs);
        /* Start 1Hz periodic timer (1,000,000 microseconds = 1 second) */
        esp_timer_start_periodic(s_tick_timer, 1000000);
      }

      if (actions & ACTION_STOP_TIMER) {
        ESP_LOGI(TAG, "Timer stopped");
        esp_timer_stop(s_tick_timer);
      }

      if (actions & ACTION_ALARM_START) {
        ESP_LOGI(TAG, "Alarm started");
#if USE_BUZZER
        buzzer_on();
#endif
        /* Start 2Hz fast timer for flashing (500ms) */
        esp_timer_start_periodic(s_fast_timer, 500 * 1000);
      }

      if (actions & ACTION_ALARM_STOP) {
        ESP_LOGI(TAG, "Alarm stopped");
#if USE_BUZZER
        buzzer_off();
        /* Re-enable backlight in case buzzer LEDC interfered */
        bsp_display_backlight_on();
#endif
        esp_timer_stop(s_fast_timer);
      }

      if (actions & ACTION_TOGGLE_FLASH) {
        bsp_display_lock(0);
        view_set_alarm_flash(s_app_state.alarm_flash_on);
        bsp_display_unlock();
      }

      /* Update UI if requested */
      if (actions & ACTION_UPDATE_UI) {
        uint32_t display_time = (s_app_state.state == STATE_RUNNING || s_app_state.state == STATE_ALARM)
                                ? s_app_state.remaining_time_secs
                                : s_app_state.target_time_secs;

        bsp_display_lock(0);
        view_update(state_to_view(s_app_state.state),
                    display_time,
                    logic_get_progress(&s_app_state));
        bsp_display_unlock();

        ESP_LOGI(TAG, "State: %d, Time: %lu, Progress: %d",
                 s_app_state.state, display_time, logic_get_progress(&s_app_state));
      }
    }
  }
}
