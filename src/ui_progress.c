/**
 * @file ui_progress.c
 * @brief Callback helpers for AI search progress UI feedback.
 */

#include "ui_progress.h"

#include "mouse_pointer.h"
#include "text_display.h"
#include "timer.h"

static const uint8_t kDotFrequencyTicks = T0_TICK_FREQ; // every 1 second

void ui_progress_init(ui_progress_state_t *state) {
    if (!state) {
        return;
    }
    state->dot_phase = 0u;
    setAlarm(TIMER_ALARM_GENERAL0, kDotFrequencyTicks);
}

void ui_progress_register(ai_config_t *config, ui_progress_state_t *state) {
    (void)config; // Unused parameter
    ai_agent_set_progress_callback(ui_progress_on_search_progress, state);
}

void ui_progress_on_search_progress(void *user_data) {

    poll_and_refresh_mouse_postion();

    ui_progress_state_t *state = (ui_progress_state_t *)user_data;
    if (!state) {
        return;
    }
    if (!checkAlarm(TIMER_ALARM_GENERAL0)) {
        return;
    }

    setAlarm(TIMER_ALARM_GENERAL0, kDotFrequencyTicks);

    state->dot_phase = (uint8_t)((state->dot_phase % 3u) + 1u);
    text_display_update_ai_thinking_indicator(state->dot_phase);
}
