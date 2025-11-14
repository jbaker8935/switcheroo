/**
 * @file freeplay_history.h
 * @brief Free play board snapshot history management
 */

#ifndef FREEPLAY_HISTORY_H
#define FREEPLAY_HISTORY_H

#include <stdint.h>
#if defined(AI_AGENT_HOST_TEST)
#include <stdbool.h>
#endif

#include "../src/board.h"

#define FREEPLAY_HISTORY_CAPACITY 4u

typedef struct {
    board_t board;
    player_t current_player;
    player_t last_moving_player;
    uint8_t layout_id;
} freeplay_history_entry_t;

typedef struct {
    freeplay_history_entry_t entries[FREEPLAY_HISTORY_CAPACITY];
    uint8_t count;
    uint8_t view_index;
    bool disqualified;
} freeplay_history_state_t;

void freeplay_history_init(freeplay_history_state_t *state);
void freeplay_history_reset(freeplay_history_state_t *state,
                            const board_t *board,
                            const board_context_t *context);
void freeplay_history_capture_live(freeplay_history_state_t *state,
                                   const board_t *board,
                                   const board_context_t *context);
uint8_t freeplay_history_prepare_branch(freeplay_history_state_t *state);
bool freeplay_history_step_backward(freeplay_history_state_t *state,
                                    board_t *board,
                                    board_context_t *context);
bool freeplay_history_step_forward(freeplay_history_state_t *state,
                                   board_t *board,
                                   board_context_t *context);
bool freeplay_history_can_step_backward(const freeplay_history_state_t *state);
bool freeplay_history_can_step_forward(const freeplay_history_state_t *state);
uint8_t freeplay_history_view_index(const freeplay_history_state_t *state);
uint8_t freeplay_history_snapshot_count(const freeplay_history_state_t *state);
bool freeplay_history_is_live(const freeplay_history_state_t *state);
bool freeplay_history_is_disqualified(const freeplay_history_state_t *state);
void freeplay_history_clear_disqualified(freeplay_history_state_t *state);
bool freeplay_history_has_start_entry(const freeplay_history_state_t *state);

#endif // FREEPLAY_HISTORY_H
