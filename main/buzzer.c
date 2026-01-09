#include "buzzer.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "buzzer";

/* Buzzer configuration */
#define BUZZER_GPIO         GPIO_NUM_3
#define BUZZER_DUTY_RES     LEDC_TIMER_10_BIT
#define BUZZER_DUTY_50PCT   512     /* 50% duty cycle (1024/2) */

/* LEDC configuration */
#define BUZZER_LEDC_TIMER   LEDC_TIMER_1
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_2 /* BSP backlight uses channel 1 */
#define BUZZER_LEDC_MODE    LEDC_LOW_SPEED_MODE

/* Single note */
typedef struct {
    uint32_t freq_hz;     /* Frequency in Hz */
    uint32_t duration_ms; /* Duration in milliseconds */
} buzzer_note_t;

/* Alarm melody: will be played in sequence with no pauses */
static const buzzer_note_t s_melody[] = {
    { 523,  120 },  /* C5 */
    { 659,  120 },  /* E5 */
    { 784,  120 },  /* G5 */
    { 1047, 120 },  /* C6 */
    { 1319, 120 },  /* E6 */
};
static const size_t s_melody_len = sizeof(s_melody) / sizeof(s_melody[0]);

/* Playback state */
static esp_timer_handle_t s_melody_timer = NULL;
static size_t s_note_index = 0;
static bool s_playing = false;

/**
 * Set the buzzer frequency and enable output.
 */
static void set_note(uint32_t freq_hz) {
    ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_DUTY_50PCT);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

/**
 * Stop the buzzer output (set duty to 0).
 */
static void silence(void) {
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

/**
 * Timer callback to advance through melody notes.
 */
static void melody_timer_cb(void *arg) {
    (void)arg;

    s_note_index++;
    if (s_note_index >= s_melody_len) {
        /* End of melody */
        silence();
        s_playing = false;
        return;
    }

    /* Play next note */
    const buzzer_note_t *note = &s_melody[s_note_index];
    set_note(note->freq_hz);
    esp_timer_start_once(s_melody_timer, note->duration_ms * 1000);
}

esp_err_t buzzer_init(void) {
    /* Configure LEDC timer with initial frequency */
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_LEDC_MODE,
        .timer_num = BUZZER_LEDC_TIMER,
        .duty_resolution = BUZZER_DUTY_RES,
        .freq_hz = 1000,  /* Initial frequency, will be changed per note */
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(err));
        return err;
    }

    /* Configure LEDC channel */
    ledc_channel_config_t channel_conf = {
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .timer_sel = BUZZER_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BUZZER_GPIO,
        .duty = 0,  /* Start with buzzer off */
        .hpoint = 0
    };
    err = ledc_channel_config(&channel_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(err));
        return err;
    }

    /* Create melody timer */
    esp_timer_create_args_t timer_args = {
        .callback = melody_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "melody"
    };
    err = esp_timer_create(&timer_args, &s_melody_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create melody timer: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", BUZZER_GPIO);
    return ESP_OK;
}

void buzzer_play_alarm(void) {
    /* Stop any existing playback */
    if (s_playing) {
        esp_timer_stop(s_melody_timer);
    }

    s_note_index = 0;
    s_playing = true;

    const buzzer_note_t *note = &s_melody[0];
    set_note(note->freq_hz);
    esp_timer_start_once(s_melody_timer, note->duration_ms * 1000);

    ESP_LOGI(TAG, "Melody started");
}

void buzzer_stop(void) {
    if (s_playing) {
        esp_timer_stop(s_melody_timer);
        s_playing = false;
    }
    silence();
    ESP_LOGI(TAG, "Buzzer stopped");
}

bool buzzer_is_playing(void) {
    return s_playing;
}
