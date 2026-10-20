#ifndef fsm_h
#define fsm_h

#include "keypad.h"   /* key_t */

#ifdef __cplusplus
extern "C" {
#endif

/* One-time bring-up: initializes PWM + ADC, physically asserts the
 * locked position, and starts the sleep-timeout clock.
 * Call exactly once (from setup()). */
void fsm_init(void);

/* Runs one iteration of the main loop: services the millisecond tick
 * and advances the state machine.
 * `key` is the debounced keypad event for this iteration (KEY_NONE if
 * no key). The caller owns the single keypad_read() poll per loop so
 * that edge-triggered events are not consumed twice.
 * Call repeatedly (from loop()). */
void fsm_tick(key_t key);

#ifdef __cplusplus
}
#endif

#endif
