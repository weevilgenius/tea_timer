#ifndef BUZZER_H
#define BUZZER_H

#include <esp_err.h>
#include <stdbool.h>

/**
 * Initialize the buzzer using LEDC PWM.
 * Configures GPIO 3 for PWM output and creates the melody timer.
 *
 * @return ESP_OK on success
 */
esp_err_t buzzer_init(void);

/**
 * Start playing the alarm melody (non-blocking).
 * The melody plays through automatically using an esp_timer.
 */
void buzzer_play_alarm(void);

/**
 * Stop the buzzer immediately.
 * Cancels any melody in progress.
 */
void buzzer_stop(void);

/**
 * Check if melody is currently playing.
 *
 * @return true if melody is playing, false otherwise
 */
bool buzzer_is_playing(void);

#endif /* BUZZER_H */
