#include "view.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"

/* Set to 1 to use scaled monospace font (stable width), 0 for proportional Montserrat */
#define USE_MONOSPACED_FONT 0

static const char *TAG = "view";

/* Display dimensions (M5Dial is 240x240 circular) */
#define DISPLAY_SIZE 240
#define ARC_WIDTH    20

/* Colors for different states */
#define COLOR_SETUP   lv_color_hex(0x2196F3)  /* Blue */
#define COLOR_RUNNING lv_color_hex(0x4CAF50)  /* Green */
#define COLOR_ALARM   lv_color_hex(0xF44336)  /* Red */
#define COLOR_BG      lv_color_hex(0x000000)  /* Black background */
#define COLOR_TEXT    lv_color_hex(0xFFFFFF)  /* White text */

/* UI widget handles */
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_arc = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_status_label = NULL;

/* Current state for color management */
static view_state_t s_current_state = VIEW_STATE_SETUP;

void view_init(void) {
    ESP_LOGI(TAG, "view_init() starting");
    bsp_display_lock(0);

    s_screen = lv_scr_act();
    ESP_LOGI(TAG, "Got screen: %p", s_screen);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);

    /* Create arc - full screen, centered */
    s_arc = lv_arc_create(s_screen);
    lv_obj_set_size(s_arc, DISPLAY_SIZE - 10, DISPLAY_SIZE - 10);
    lv_obj_center(s_arc);

    /* Arc styling */
    lv_arc_set_rotation(s_arc, 270);  /* Start from top */
    lv_arc_set_bg_angles(s_arc, 0, 360);  /* Full circle background */
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 100);

    /* Remove knob and make arc non-interactive */
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    /* Arc indicator (foreground) style */
    lv_obj_set_style_arc_width(s_arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, COLOR_SETUP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    /* Arc background style */
    lv_obj_set_style_arc_width(s_arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x333333), LV_PART_MAIN);

    /* Create time label - center, large font */
    s_time_label = lv_label_create(s_screen);
    ESP_LOGI(TAG, "Created time label: %p", s_time_label);
#if USE_MONOSPACED_FONT
    /* Scaled monospace font for stable width during countdown */
    lv_obj_set_style_text_font(s_time_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_transform_zoom(s_time_label, 512, 0);  /* 2x scale */
    lv_obj_set_style_transform_pivot_x(s_time_label, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(s_time_label, LV_PCT(50), 0);
#else
    /* Proportional font - may shift slightly during countdown */
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, 0);
#endif
    lv_obj_set_style_text_color(s_time_label, COLOR_TEXT, 0);
    lv_label_set_text(s_time_label, "5:00");
    lv_obj_center(s_time_label);

    /* Create status label - bottom, smaller font */
    s_status_label = lv_label_create(s_screen);
    ESP_LOGI(TAG, "Created status label: %p", s_status_label);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_status_label, "SET TIME");
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 50);

    bsp_display_unlock();
    ESP_LOGI(TAG, "view_init() complete");
}

void view_update(view_state_t state, uint32_t time_secs, uint8_t progress) {
    ESP_LOGI(TAG, "view_update(state=%d, time=%lu, progress=%d)", state, time_secs, progress);

    /* Reset flash state when not in alarm (ensures clean transition from alarm) */
    if (state != VIEW_STATE_ALARM) {
        lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
        lv_obj_set_style_text_color(s_time_label, COLOR_TEXT, 0);
    }

    /* Update arc color based on state */
    lv_color_t arc_color;
    const char *status_text;

    switch (state) {
        case VIEW_STATE_SETUP:
            arc_color = COLOR_SETUP;
            status_text = "BREW TIME";
            break;
        case VIEW_STATE_RUNNING:
            arc_color = COLOR_RUNNING;
            status_text = "BREWING";
            break;
        case VIEW_STATE_ALARM:
            arc_color = COLOR_ALARM;
            status_text = "TEA IS READY!";
            break;
        case VIEW_STATE_SLEEP:
            /* Sleep state handled by backlight, not UI */
            return;
        default:
            arc_color = COLOR_SETUP;
            status_text = "";
            break;
    }

    s_current_state = state;

    /* Update arc */
    lv_obj_set_style_arc_color(s_arc, arc_color, LV_PART_INDICATOR);
    lv_arc_set_value(s_arc, progress);

    /* Update time label */
    uint32_t minutes = time_secs / 60;
    uint32_t seconds = time_secs % 60;
    lv_label_set_text_fmt(s_time_label, "%lu:%02lu", minutes, seconds);

    /* Update status label */
    lv_label_set_text(s_status_label, status_text);

    /* Force screen refresh */
    lv_obj_invalidate(s_screen);
}

void view_set_alarm_flash(bool flash_on) {
    if (flash_on) {
        lv_obj_set_style_bg_color(s_screen, COLOR_ALARM, 0);
        lv_obj_set_style_text_color(s_time_label, COLOR_BG, 0);
    } else {
        lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
        lv_obj_set_style_text_color(s_time_label, COLOR_TEXT, 0);
    }
}
