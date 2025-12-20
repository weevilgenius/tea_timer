#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Tea timer states
 */
typedef enum {
    STATE_SETUP,    /* Selecting time */
    STATE_RUNNING,  /* Countdown active */
    STATE_ALARM,    /* Timer complete */
    STATE_SLEEP     /* Display off, low power */
} tea_state_t;

/**
 * Event types for state machine
 */
typedef enum {
    EVT_NONE,
    EVT_BUTTON_PRESS,
    EVT_ENCODER_CHANGE,
    EVT_TICK_1HZ,
    EVT_TICK_FAST,
    EVT_INACTIVITY_TIMEOUT
} logic_event_t;

/**
 * Actions requested by state machine (bitmask)
 */
typedef enum {
    ACTION_NONE           = 0,
    ACTION_UPDATE_UI      = (1 << 0),
    ACTION_START_TIMER    = (1 << 1),
    ACTION_STOP_TIMER     = (1 << 2),
    ACTION_BUZZER_ON      = (1 << 3),
    ACTION_BUZZER_OFF     = (1 << 4),
    ACTION_BACKLIGHT_ON   = (1 << 5),
    ACTION_BACKLIGHT_OFF  = (1 << 6),
    ACTION_TOGGLE_FLASH   = (1 << 7)
} logic_action_t;

/**
 * Application state structure
 */
typedef struct {
    tea_state_t state;
    uint32_t target_time_secs;    /* User-selected time (seconds) */
    uint32_t remaining_time_secs; /* Countdown remaining (seconds) */
    int32_t last_encoder_count;   /* For delta calculation */
    bool alarm_flash_on;          /* Toggle state for alarm flashing */
} app_state_t;

/**
 * Timer configuration constants
 */
#define LOGIC_MIN_TIME_SECS   60   /* 1 minute minimum */
#define LOGIC_MAX_TIME_SECS   600  /* 10 minutes maximum */
#define LOGIC_TIME_STEP_SECS  60   /* 1 minute increments */
#define LOGIC_ENCODER_DIVISOR 4    /* 4 counts per detent */

/**
 * Initialize the application state.
 *
 * @param state  Pointer to state structure to initialize
 */
void logic_init(app_state_t *state);

/**
 * Process an event and update state.
 * This is a pure function with no side effects on hardware.
 *
 * @param state        Pointer to current state (will be modified)
 * @param event        The event to process
 * @param event_value  Value associated with event (e.g., encoder count)
 * @return Bitmask of actions to perform (logic_action_t)
 */
uint32_t logic_process_event(app_state_t *state, logic_event_t event, int32_t event_value);

/**
 * Get progress percentage for UI arc display.
 *
 * @param state  Current application state
 * @return Progress 0-100 (100 = full, 0 = empty)
 */
uint8_t logic_get_progress(const app_state_t *state);

#endif /* LOGIC_H */
