#include "logic.h"

void logic_init(app_state_t *state) {
    state->state = STATE_SETUP;
    state->target_time_secs = 300;  /* Default 5 minutes */
    state->remaining_time_secs = 300;
    state->last_encoder_count = 0;
    state->alarm_flash_on = false;
}

/**
 * Handle encoder input in SETUP state
 */
static uint32_t handle_encoder_setup(app_state_t *state, int32_t new_count) {
    int32_t delta = new_count - state->last_encoder_count;
    int32_t effective_clicks = delta / LOGIC_ENCODER_DIVISOR;

    if (effective_clicks == 0) {
        return ACTION_NONE;
    }

    /* Update last count to account for processed clicks */
    state->last_encoder_count += effective_clicks * LOGIC_ENCODER_DIVISOR;

    /* Calculate new time */
    int32_t new_time = (int32_t)state->target_time_secs + (effective_clicks * LOGIC_TIME_STEP_SECS);

    /* Clamp to valid range */
    if (new_time < LOGIC_MIN_TIME_SECS) {
        new_time = LOGIC_MIN_TIME_SECS;
    }
    if (new_time > LOGIC_MAX_TIME_SECS) {
        new_time = LOGIC_MAX_TIME_SECS;
    }

    state->target_time_secs = (uint32_t)new_time;
    state->remaining_time_secs = state->target_time_secs;

    return ACTION_UPDATE_UI;
}

/**
 * Process events in SETUP state
 */
static uint32_t process_setup(app_state_t *state, logic_event_t event, int32_t value) {
    uint32_t actions = ACTION_NONE;

    switch (event) {
        case EVT_BUTTON_PRESS:
            /* Start the timer */
            state->state = STATE_RUNNING;
            state->remaining_time_secs = state->target_time_secs;
            actions = ACTION_UPDATE_UI | ACTION_START_TIMER;
            break;

        case EVT_ENCODER_CHANGE:
            actions = handle_encoder_setup(state, value);
            break;

        case EVT_INACTIVITY_TIMEOUT:
            /* Go to sleep */
            state->state = STATE_SLEEP;
            actions = ACTION_UPDATE_UI | ACTION_BACKLIGHT_OFF;
            break;

        default:
            break;
    }

    return actions;
}

/**
 * Process events in RUNNING state
 */
static uint32_t process_running(app_state_t *state, logic_event_t event, int32_t value) {
    uint32_t actions = ACTION_NONE;
    (void)value;  /* Unused in RUNNING state */

    switch (event) {
        case EVT_BUTTON_PRESS:
            /* Cancel timer, return to setup */
            state->state = STATE_SETUP;
            state->remaining_time_secs = state->target_time_secs;
            actions = ACTION_UPDATE_UI | ACTION_STOP_TIMER;
            break;

        case EVT_TICK_1HZ:
            if (state->remaining_time_secs > 0) {
                state->remaining_time_secs--;
                actions = ACTION_UPDATE_UI;

                if (state->remaining_time_secs == 0) {
                    /* Timer complete - go to alarm */
                    state->state = STATE_ALARM;
                    state->alarm_flash_on = true;
                    actions |= ACTION_STOP_TIMER | ACTION_ALARM_START;
                }
            }
            break;

        case EVT_ENCODER_CHANGE:
            /* Encoder ignored in RUNNING state */
            /* But update last_encoder_count to avoid jump when returning to SETUP */
            state->last_encoder_count = value;
            break;

        default:
            break;
    }

    return actions;
}

/**
 * Process events in ALARM state
 */
static uint32_t process_alarm(app_state_t *state, logic_event_t event, int32_t value) {
    uint32_t actions = ACTION_NONE;

    switch (event) {
        case EVT_BUTTON_PRESS:
        case EVT_ENCODER_CHANGE:
            /* Any input stops the alarm */
            state->state = STATE_SETUP;
            state->remaining_time_secs = state->target_time_secs;
            state->alarm_flash_on = false;
            state->last_encoder_count = value;
            /* Explicitly turn backlight on - buzzer LEDC can interfere */
            actions = ACTION_UPDATE_UI | ACTION_ALARM_STOP | ACTION_BACKLIGHT_ON;
            break;

        case EVT_TICK_FAST:
            /* Toggle flash state */
            state->alarm_flash_on = !state->alarm_flash_on;
            actions = ACTION_TOGGLE_FLASH;
            break;

        default:
            break;
    }

    return actions;
}

/**
 * Process events in SLEEP state
 */
static uint32_t process_sleep(app_state_t *state, logic_event_t event, int32_t value) {
    uint32_t actions = ACTION_NONE;

    switch (event) {
        case EVT_BUTTON_PRESS:
        case EVT_ENCODER_CHANGE:
            /* Any input wakes up */
            state->state = STATE_SETUP;
            state->last_encoder_count = value;
            actions = ACTION_UPDATE_UI | ACTION_BACKLIGHT_ON;
            break;

        default:
            break;
    }

    return actions;
}

uint32_t logic_process_event(app_state_t *state, logic_event_t event, int32_t event_value) {
    switch (state->state) {
        case STATE_SETUP:
            return process_setup(state, event, event_value);

        case STATE_RUNNING:
            return process_running(state, event, event_value);

        case STATE_ALARM:
            return process_alarm(state, event, event_value);

        case STATE_SLEEP:
            return process_sleep(state, event, event_value);

        default:
            return ACTION_NONE;
    }
}

uint8_t logic_get_progress(const app_state_t *state) {
    if (state->target_time_secs == 0) {
        return 100;
    }

    switch (state->state) {
        case STATE_SETUP:
            /* In setup, show full arc */
            return 100;

        case STATE_RUNNING:
        case STATE_ALARM:
            /* Show remaining time as percentage */
            return (uint8_t)((state->remaining_time_secs * 100) / state->target_time_secs);

        case STATE_SLEEP:
        default:
            return 0;
    }
}
