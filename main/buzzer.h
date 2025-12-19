#ifndef BUZZER_H
#define BUZZER_H

#include "esp_err.h"

/**
 * Initialize the buzzer using LEDC PWM.
 * Configures GPIO 3 for 4kHz PWM output.
 *
 * @return ESP_OK on success
 */
esp_err_t buzzer_init(void);

/**
 * Turn the buzzer on at the configured frequency.
 */
void buzzer_on(void);

/**
 * Turn the buzzer off.
 */
void buzzer_off(void);

#endif /* BUZZER_H */
