/**
 * @file ui_progress.h
 * @brief Utilities for displaying AI search progress feedback.
 */

#ifndef UI_PROGRESS_H
#define UI_PROGRESS_H

#include <stdint.h>

#include "../src/ai_agent.h"

typedef struct {
    uint8_t dot_phase;
} ui_progress_state_t;

void ui_progress_init(ui_progress_state_t *state);
void ui_progress_register(ai_config_t *config, ui_progress_state_t *state);
void ui_progress_on_search_progress(uint8_t current_depth, uint32_t nodes, void *user_data);

#endif // UI_PROGRESS_H
