/**
 * @file ui_progress.c
 * @brief Callback helpers for AI search progress UI feedback.
 */

#include "../src/ui_progress.h"

#include "../src/mouse_pointer.h"
#include "../src/text_display.h"

const uint8_t dot_frequency = 32u;
static uint8_t dot_counter = 0u;

void ui_progress_init(ui_progress_state_t *state) {
    if (!state) {
        return;
    }
    state->dot_phase = 0u;
}

void ui_progress_register(ai_config_t *config, ui_progress_state_t *state) {
    if (!config) {
        return;
    }
    ai_agent_set_progress_callback(config, ui_progress_on_search_progress, state);
}

void ui_progress_on_search_progress(uint8_t current_depth, uint32_t nodes, void *user_data) {
    (void)current_depth;
    (void)nodes;

    poll_and_refresh_mouse_postion();

    ui_progress_state_t *state = (ui_progress_state_t *)user_data;
    if (!state) {
        return;
    }
    dot_counter = (dot_counter + 1u) % dot_frequency;
    if (dot_counter != 0u) {
        return;
    }

    state->dot_phase = (uint8_t)((state->dot_phase % 3u) + 1u);
    text_display_update_ai_thinking_indicator(state->dot_phase);
}
