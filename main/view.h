#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Tea timer states for UI display
 */
typedef enum {
    VIEW_STATE_SETUP,    /* Selecting time - arc blue, shows target */
    VIEW_STATE_RUNNING,  /* Countdown active - arc green, shows remaining */
    VIEW_STATE_ALARM,    /* Timer complete - arc red, flashing */
    VIEW_STATE_SLEEP     /* Display off */
} view_state_t;

/**
 * Initialize the UI elements.
 * Creates arc, time label, and status label.
 * Must be called after bsp_display_start().
 * Handles its own display locking.
 */
void view_init(void);

/**
 * Update the UI to reflect current state.
 * Caller MUST hold the display lock before calling.
 *
 * @param state       Current timer state
 * @param time_secs   Time to display (target in SETUP, remaining in RUNNING)
 * @param progress    Arc progress 0-100 (100 = full circle)
 */
void view_update(view_state_t state, uint32_t time_secs, uint8_t progress);

/**
 * Toggle the alarm flash state (for ALARM state animation).
 * Caller MUST hold the display lock before calling.
 *
 * @param flash_on    true = red background, false = normal background
 */
void view_set_alarm_flash(bool flash_on);

#endif /* VIEW_H */
