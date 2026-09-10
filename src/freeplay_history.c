#include "freeplay_history.h"
#include <string.h>

static void freeplay_history_store_entry(freeplay_history_entry_t *entry,
                                         const board_t *board,
                                         const board_context_t *context)
{
    if (!entry || !board) {
        return;
    }

    board_copy(&entry->board, board);

    if (context) {
        entry->current_player = context->current_player;
        entry->last_moving_player = context->last_moving_player;
        entry->layout_id = context->layout_id;
    } else {
        entry->current_player = PLAYER_NONE;
        entry->last_moving_player = PLAYER_NONE;
        entry->layout_id = 0u;
    }
}

void freeplay_history_init(freeplay_history_state_t *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

void freeplay_history_reset(freeplay_history_state_t *state,
                            const board_t *board,
                            const board_context_t *context)
{
    if (!state) {
        return;
    }

    state->count = 0u;
    state->view_index = 0u;
    state->disqualified = false;

    if (board) {
        freeplay_history_capture_live(state, board, context);
    }
}

void freeplay_history_capture_live(freeplay_history_state_t *state,
                                   const board_t *board,
                                   const board_context_t *context)
{
    if (!state || !board) {
        return;
    }

    if (state->count < FREEPLAY_HISTORY_CAPACITY) {
        ++state->count;
    }

    for (uint8_t index = state->count - 1u; index > 0u; --index) {
        state->entries[index] = state->entries[index - 1u];
    }

    freeplay_history_store_entry(&state->entries[0], board, context);
    state->view_index = 0u;
}

uint8_t freeplay_history_prepare_branch(freeplay_history_state_t *state)
{
    if (!state || state->view_index == 0u) {
        return 0u;
    }

    const uint8_t removed = state->view_index;

    for (uint8_t index = 0u; index + removed < state->count; ++index) {
        state->entries[index] = state->entries[index + removed];
    }

    if (state->count > removed) {
        state->count = (uint8_t)(state->count - removed);
    } else {
        state->count = 1u;
    }

    state->view_index = 0u;
    state->disqualified = true;

    return removed;
}

static bool freeplay_history_apply_entry(freeplay_history_state_t *state,
                                         uint8_t target_index,
                                         board_t *board,
                                         board_context_t *context)
{
    if (!state || target_index >= state->count || !board) {
        return false;
    }

    freeplay_history_entry_t *entry = &state->entries[target_index];
    board_copy(board, &entry->board);

    if (context) {
        context->current_player = entry->current_player;
        context->last_moving_player = entry->last_moving_player;
        context->layout_id = entry->layout_id;
    }

    return true;
}

bool freeplay_history_step_backward(freeplay_history_state_t *state,
                                    board_t *board,
                                    board_context_t *context)
{
    if (!state) {
        return false;
    }

    if (!freeplay_history_can_step_backward(state)) {
        return false;
    }

    const uint8_t target_index = (uint8_t)(state->view_index + 1u);
    if (!freeplay_history_apply_entry(state, target_index, board, context)) {
        return false;
    }

    state->view_index = target_index;
    return true;
}

bool freeplay_history_step_forward(freeplay_history_state_t *state,
                                   board_t *board,
                                   board_context_t *context)
{
    if (!state) {
        return false;
    }

    if (!freeplay_history_can_step_forward(state)) {
        return false;
    }

    const uint8_t target_index = (uint8_t)(state->view_index - 1u);
    if (!freeplay_history_apply_entry(state, target_index, board, context)) {
        return false;
    }

    state->view_index = target_index;
    return true;
}

bool freeplay_history_can_step_backward(const freeplay_history_state_t *state)
{
    if (!state) {
        return false;
    }

    return (uint8_t)(state->view_index + 1u) < state->count;
}

bool freeplay_history_can_step_forward(const freeplay_history_state_t *state)
{
    if (!state) {
        return false;
    }

    return state->view_index > 0u;
}

uint8_t freeplay_history_view_index(const freeplay_history_state_t *state)
{
    if (!state) {
        return 0u;
    }

    return state->view_index;
}

uint8_t freeplay_history_snapshot_count(const freeplay_history_state_t *state)
{
    if (!state) {
        return 0u;
    }

    return state->count;
}

bool freeplay_history_is_live(const freeplay_history_state_t *state)
{
    if (!state) {
        return true;
    }

    return state->view_index == 0u;
}

bool freeplay_history_has_start_entry(const freeplay_history_state_t *state)
{
    if (!state || state->count == 0u)
    {
        return false;
    }

    for (uint8_t index = 0u; index < state->count; ++index)
    {
        if (state->entries[index].board.move_count == 0u)
        {
            return true;
        }
    }

    return false;
}

bool freeplay_history_is_disqualified(const freeplay_history_state_t *state)
{
    if (!state) {
        return false;
    }

    return state->disqualified;
}

void freeplay_history_clear_disqualified(freeplay_history_state_t *state)
{
    if (!state) {
        return;
    }

    state->disqualified = false;
}
