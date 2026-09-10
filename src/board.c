/**
 * @file board.c
 * @brief Game board model implementation for F256 Switcharoo
 * 
 * Implements board state management, move validation, and win detection
 * using a BFS connectivity check per design.md guidance.
 */

#include "board.h"
#include "ai_agent.h"
#include <string.h>

extern void render_invalidate_cache(void);

#define PROGRESS_CALLBACK_FREQUENCY 3
static uint8_t s_progress_callback_counter = 0;

enum {
    kWinRowCount = WIN_END_ROW - WIN_START_ROW + 1,
    kWinCellCount = kWinRowCount * BOARD_COLS
};

#define WIN_CELL_INDEX(row, col) ((row) * BOARD_COLS + (col))
#define WIN_CELL_BIT(row, col) (UINT32_C(1) << WIN_CELL_INDEX((row), (col)))
#define WIN_CELL_BITS_ROW(row) \
    WIN_CELL_BIT((row), 0), WIN_CELL_BIT((row), 1), WIN_CELL_BIT((row), 2), WIN_CELL_BIT((row), 3)
#define WIN_ROW_MASK_VALUE(row) \
    (WIN_CELL_BIT((row), 0) | WIN_CELL_BIT((row), 1) | WIN_CELL_BIT((row), 2) | WIN_CELL_BIT((row), 3))



// Internal versions that skip bounds checking for performance

static inline void board_set_piece_unchecked(board_t *board, uint8_t row, uint8_t col, piece_type_t piece) {
    piece_type_t old_piece = board->cells[row][col];
    if (board_is_piece_swapped(old_piece)) {
        if (board->swapped_count > 0u) {
            board->swapped_count--;
        }
        player_t old_owner = board_get_piece_owner(old_piece);
        if (old_owner == PLAYER_WHITE) {
            if (board->white_swapped_count > 0u) {
                board->white_swapped_count--;
            }
        } else if (old_owner == PLAYER_BLACK) {
            if (board->black_swapped_count > 0u) {
                board->black_swapped_count--;
            }
        }
    }

    board->cells[row][col] = piece;

    if (board_is_piece_swapped(piece)) {
        board->swapped_count++;
        player_t new_owner = board_get_piece_owner(piece);
        if (new_owner == PLAYER_WHITE) {
            board->white_swapped_count++;
        } else if (new_owner == PLAYER_BLACK) {
            board->black_swapped_count++;
        }
    }
}

// Non-inline definitions for extern inline functions
piece_type_t board_get_piece(const board_t *board, uint8_t row, uint8_t col) {
    if (!board_is_valid_cell(row, col)) {
        return PIECE_NONE;
    }
    return board_get_piece_unchecked(board, row, col);
}

void board_set_piece(board_t *board, uint8_t row, uint8_t col, piece_type_t piece) {
    if (board_is_valid_cell(row, col)) {
        board_set_piece_unchecked(board, row, col, piece);
    }
}


bool board_can_move(const board_t *board, player_t current_player, uint8_t from_row, uint8_t from_col,
                    uint8_t to_row, uint8_t to_col, move_type_t *out_type) {
    // Check adjacency
    if (!board_is_adjacent(from_row, from_col, to_row, to_col)) {
        return false;
    }
    
    // Get pieces
    piece_type_t from_piece = board_get_piece_unchecked(board, from_row, from_col);
    piece_type_t to_piece = board_get_piece_unchecked(board, to_row, to_col);
    
    // Check that source has a piece belonging to current player
    if (board_get_piece_owner(from_piece) != current_player) {
        return false;
    }
    
    // Check that target cell does not contain current player's piece
    if (to_piece != PIECE_NONE) {
        player_t to_owner = board_get_piece_owner(to_piece);
        if (to_owner == current_player) {
            return false;  // Cannot move to cell occupied by own piece
        }
    }
    
    // Empty cell move
    if (to_piece == PIECE_NONE) {
        if (out_type) *out_type = MOVE_TYPE_EMPTY;
        return true;
    }
    
    // Swap move - target must be opponent's NORMAL piece
    player_t to_owner = board_get_piece_owner(to_piece);
    if (to_owner != current_player && board_is_piece_normal(to_piece)) {
        if (out_type) *out_type = MOVE_TYPE_SWAP;
        return true;
    }
    
    // If we reach here, the target is an opponent's piece but NOT normal (i.e., swapped)
    // This should not be a valid move
    return false;
}

// Unchecked version of board_can_move - assumes all cells are valid
// Used in performance-critical loops where bounds are already verified
bool board_can_move_unchecked(const board_t *board, player_t current_player, uint8_t from_row, uint8_t from_col,
                              uint8_t to_row, uint8_t to_col, move_type_t *out_type) {
    // Check adjacency - unneeded because caller guarantees valid cells
    // if (!board_is_adjacent(from_row, from_col, to_row, to_col)) {
    //     return false;
    // }
    
    // Get pieces
    piece_type_t from_piece = board_get_piece_unchecked(board, from_row, from_col);
    piece_type_t to_piece = board_get_piece_unchecked(board, to_row, to_col);
    
    // Check that source has a piece belonging to current player
    if (board_get_piece_owner(from_piece) != current_player) {
        return false;
    }
    
    // Check that target cell does not contain current player's piece
    if (to_piece != PIECE_NONE) {
        player_t to_owner = board_get_piece_owner(to_piece);
        if (to_owner == current_player) {
            return false;  // Cannot move to cell occupied by own piece
        }
    }
    
    // Empty cell move
    if (to_piece == PIECE_NONE) {
        if (out_type) *out_type = MOVE_TYPE_EMPTY;
        return true;
    }
    
    // Swap move - target must be opponent's NORMAL piece
    player_t to_owner = board_get_piece_owner(to_piece);
    if (to_owner != current_player && board_is_piece_normal(to_piece)) {
        if (out_type) *out_type = MOVE_TYPE_SWAP;
        return true;
    }
    
    // If we reach here, the target is an opponent's piece but NOT normal (i.e., swapped)
    // This should not be a valid move
    return false;
}

// Direction deltas for 8-way adjacency (N, NE, E, SE, S, SW, W, NW)
static const int8_t kDirRow[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const int8_t kDirCol[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

// Starting layout definitions
static const piece_type_t kStartingLayout0[BOARD_ROWS][BOARD_COLS] = {
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL }
};

static const piece_type_t kStartingLayout1[BOARD_ROWS][BOARD_COLS] = {
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE }
};

static const piece_type_t kStartingLayout2[BOARD_ROWS][BOARD_COLS] = {
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL, PIECE_WHITE_NORMAL, PIECE_BLACK_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_NONE }
};

static const piece_type_t kStartingLayout3[BOARD_ROWS][BOARD_COLS] = {
    { PIECE_BLACK_NORMAL, PIECE_NONE, PIECE_NONE, PIECE_NONE },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL, PIECE_NONE, PIECE_NONE },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL,  PIECE_NONE, PIECE_NONE },
    { PIECE_BLACK_NORMAL, PIECE_BLACK_NORMAL,  PIECE_BLACK_NORMAL, PIECE_NONE },
    { PIECE_NONE, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_WHITE_NORMAL, PIECE_WHITE_NORMAL },
    { PIECE_NONE, PIECE_NONE, PIECE_NONE, PIECE_WHITE_NORMAL }
};


void board_init(board_t *board) {
    memset(board, 0, sizeof(board_t));
    board_reset(board);
}

void board_reset(board_t *board) {
    // Copy selected starting layout
    board_set_starting_layout(board, 0); // Default layout
    board->move_count = 0;
    board->swapped_count = 0;  // No swapped pieces in starting layout
    board->white_swapped_count = 0;
    board->black_swapped_count = 0;
}

void board_set_starting_layout(board_t *board, uint8_t layout_id) {
    const piece_type_t *layout = (const piece_type_t *)kStartingLayout0;
    switch (layout_id) {
        case 0:
            layout = (const piece_type_t *)kStartingLayout0;
            break;
        case 1:
            layout = (const piece_type_t *)kStartingLayout1;
            break;
        case 2:
            layout = (const piece_type_t *)kStartingLayout2;
            break;
        case 3:
            layout = (const piece_type_t *)kStartingLayout3;
            break;
        default:
            layout = (const piece_type_t *)kStartingLayout0;
            break;
    }
    memcpy(board->cells, layout, sizeof(board->cells));
    board->move_count = 0;
    board->swapped_count = 0;
    board->white_swapped_count = 0;
    board->black_swapped_count = 0;
}


uint8_t board_get_legal_moves(const board_t *board, player_t current_player, uint8_t row, uint8_t col,
                               move_t *moves, uint8_t max_moves) {
    uint8_t count = 0;
    
    // Check all 8 directions
    for (uint8_t dir = 0; dir < 8 && count < max_moves; ++dir) {
        int8_t new_row = (int8_t)row + kDirRow[dir];
        int8_t new_col = (int8_t)col + kDirCol[dir];
        
        if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
            move_type_t type;
            if (board_can_move(board, current_player, row, col, (uint8_t)new_row, (uint8_t)new_col, &type)) {
                moves[count].from_row = row;
                moves[count].from_col = col;
                moves[count].to_row = (uint8_t)new_row;
                moves[count].to_col = (uint8_t)new_col;
                moves[count].type = type;
                moves[count].player = current_player;
                count++;
            }
        }
    }
    
    return count;
}

// SOA version for better 6502 performance - uses struct of arrays layout
uint8_t board_get_legal_moves_soa(const board_t *board, player_t current_player, uint8_t row, uint8_t col,
                                  move_array_t *out_moves) {
    uint8_t count = 0;
    
    // Check all 8 directions
    for (uint8_t dir = 0; dir < 8 && count < 32; ++dir) {
        int8_t new_row = (int8_t)row + kDirRow[dir];
        int8_t new_col = (int8_t)col + kDirCol[dir];
        
        if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
            move_type_t type;
            if (board_can_move_unchecked(board, current_player, row, col, (uint8_t)new_row, (uint8_t)new_col, &type)) {
                out_moves->from_row[count] = row;
                out_moves->from_col[count] = col;
                out_moves->to_row[count] = (uint8_t)new_row;
                out_moves->to_col[count] = (uint8_t)new_col;
                out_moves->type[count] = type;
                out_moves->player[count] = current_player;
                count++;
            }
        }
    }
    
    out_moves->count = count;
    return count;
}

static bool board_execute_move_internal(board_t *board, board_context_t *context, const move_t *move, uint8_t swap_rule_val,
                                        bool record_history) {
    swap_rule_t swap_rule = (swap_rule_t)swap_rule_val;
    // Validate move
    move_type_t type;
    if (!board_can_move(board, context->current_player, move->from_row, move->from_col,
                        move->to_row, move->to_col, &type)) {
        return false;
    }

    // Verify move type matches
    if (type != move->type) {
        return false;
    }

    // Save to history when requested so live boards retain full tracking
    if (record_history) {
        for (uint8_t i = MAX_MOVE_HISTORY - 1; i > 0; --i) {
            context->history[i] = context->history[i - 1];
        }
        context->history[0] = *move;
        if (context->history_count < MAX_MOVE_HISTORY) {
            context->history_count++;
        }
    }

    piece_type_t from_piece = board_get_piece_unchecked(board, move->from_row, move->from_col);
    piece_type_t to_piece = board_get_piece_unchecked(board, move->to_row, move->to_col);
    
    if (type == MOVE_TYPE_EMPTY) {
        // Move to empty cell
        board_set_piece(board, move->to_row, move->to_col, from_piece);
        board_set_piece(board, move->from_row, move->from_col, PIECE_NONE);
        
        // Clear swapped pieces according to swap rule
        switch (swap_rule) {
            case SWAP_RULE_CLASSIC:
                // Classic: clear all swapped pieces on an empty move
                if (board->swapped_count > 0) {
                    board_clear_all_swapped(board);
                }
                break;
            case SWAP_RULE_CLEARS_OWN:
                // Clear only the moving player's swapped pieces
                {
                    player_t mover = board_get_piece_owner(from_piece);
                    if (mover == PLAYER_WHITE) {
                        // Clear white swapped pieces - only scan if any swapped pieces exist
                        if (board->swapped_count > 0) {
                            // Unroll inner loop for BOARD_COLS == 4 for performance
                            for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                                if (board_get_piece_unchecked(board, r, 0) == PIECE_WHITE_SWAPPED) board_set_piece_unchecked(board, r, 0, PIECE_WHITE_NORMAL);
                                if (board_get_piece_unchecked(board, r, 1) == PIECE_WHITE_SWAPPED) board_set_piece_unchecked(board, r, 1, PIECE_WHITE_NORMAL);
                                if (board_get_piece_unchecked(board, r, 2) == PIECE_WHITE_SWAPPED) board_set_piece_unchecked(board, r, 2, PIECE_WHITE_NORMAL);
                                if (board_get_piece_unchecked(board, r, 3) == PIECE_WHITE_SWAPPED) board_set_piece_unchecked(board, r, 3, PIECE_WHITE_NORMAL);
                            }
                        }
                    } else if (mover == PLAYER_BLACK) {
                        // Clear black swapped pieces - only scan if any swapped pieces exist
                        if (board->swapped_count > 0) {
                            // Unroll inner loop for BOARD_COLS == 4 for performance
                            for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                                if (board_get_piece_unchecked(board, r, 0) == PIECE_BLACK_SWAPPED) board_set_piece_unchecked(board, r, 0, PIECE_BLACK_NORMAL);
                                if (board_get_piece_unchecked(board, r, 1) == PIECE_BLACK_SWAPPED) board_set_piece_unchecked(board, r, 1, PIECE_BLACK_NORMAL);
                                if (board_get_piece_unchecked(board, r, 2) == PIECE_BLACK_SWAPPED) board_set_piece_unchecked(board, r, 2, PIECE_BLACK_NORMAL);
                                if (board_get_piece_unchecked(board, r, 3) == PIECE_BLACK_SWAPPED) board_set_piece_unchecked(board, r, 3, PIECE_BLACK_NORMAL);
                            }
                        }
                    }
                }
                break;
            case SWAP_RULE_SWAPPED_CLEARS:
                // Clear all swapped pieces only if the mover piece was swapped
                if (board_is_piece_swapped(from_piece)) {
                    board_clear_all_swapped(board);
                }
                break;
            case SWAP_RULE_SWAPPED_CLEARS_OWN:
                // Clear only the mover's swapped pieces, but only if mover was swapped
                if (board_is_piece_swapped(from_piece)) {
                    player_t mover = board_get_piece_owner(from_piece);
                    if (mover == PLAYER_WHITE) {
                        // Clear white swapped pieces - only scan if any swapped pieces exist
                        if (board->swapped_count > 0) {
                            for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                                for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                    if (board_get_piece_unchecked(board, r, c) == PIECE_WHITE_SWAPPED) {
                                        board_set_piece_unchecked(board, r, c, PIECE_WHITE_NORMAL);
                                    }
                                }
                            }
                        }
                    } else if (mover == PLAYER_BLACK) {
                        // Clear black swapped pieces - only scan if any swapped pieces exist
                        if (board->swapped_count > 0) {
                            for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                                for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                    if (board_get_piece_unchecked(board, r, c) == PIECE_BLACK_SWAPPED) {
                                        board_set_piece_unchecked(board, r, c, PIECE_BLACK_NORMAL);
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            default:
                board_clear_all_swapped(board);
                break;
        }
        
    } else if (type == MOVE_TYPE_SWAP) {
        // Swap pieces and mark both as swapped
        player_t from_owner = board_get_piece_owner(from_piece);
        player_t to_owner = board_get_piece_owner(to_piece);
        
        piece_type_t from_swapped = (from_owner == PLAYER_WHITE) ? 
            PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
        piece_type_t to_swapped = (to_owner == PLAYER_WHITE) ?
            PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
        
        board_set_piece(board, move->to_row, move->to_col, from_swapped);
        board_set_piece(board, move->from_row, move->from_col, to_swapped);
        
        // Some swap rules may clear swapped pieces as a result of swaps; currently no-op here
    }
    
    board->move_count++;
    context->last_moving_player = move->player;

    if (record_history) {
        // Ensure renderer updates immediately to reflect new piece states
        render_invalidate_cache();
    }

    return true;
}

bool board_execute_move(board_t *board, board_context_t *context, const move_t *move, uint8_t swap_rule_val) {
    return board_execute_move_internal(board, context, move, swap_rule_val, true);
}

bool board_execute_move_without_history(board_t *board, board_context_t *context, const move_t *move, uint8_t swap_rule_val) {
    return board_execute_move_internal(board, context, move, swap_rule_val, false);
}

void board_undo_last_move(board_t *board) {
    // TODO: Implement undo functionality
    // Requires storing previous board state
    (void)board;
}

void board_clear_all_swapped(board_t *board) {
    // Early exit if no swapped pieces exist
    if (board->swapped_count == 0) {
        return;
    }
    
    bool any_cleared = false;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);

            if (piece == PIECE_WHITE_SWAPPED) {
                board_set_piece_unchecked(board, row, col, PIECE_WHITE_NORMAL);
                any_cleared = true;
            } else if (piece == PIECE_BLACK_SWAPPED) {
                board_set_piece_unchecked(board, row, col, PIECE_BLACK_NORMAL);
                any_cleared = true;
            }
        }
    }

    if (any_cleared) {
        // Invalidate render cache so sprite bitmaps are redefined on next frame
        render_invalidate_cache();
    }

    board->swapped_count = 0;
    board->white_swapped_count = 0;
    board->black_swapped_count = 0;
}

// Precomputed connectivity table for fast win checking
// Maps [occupancy_mask][previous_row_reachable_mask] -> current_row_reachable_mask
static const uint8_t kConnectivityTable[256] = {
    /* occ= 0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    /* occ= 1 */ 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 
    /* occ= 2 */ 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 
    /* occ= 3 */ 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 
    /* occ= 4 */ 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 
    /* occ= 5 */ 0x00, 0x01, 0x05, 0x05, 0x04, 0x05, 0x05, 0x05, 0x04, 0x05, 0x05, 0x05, 0x04, 0x05, 0x05, 0x05, 
    /* occ= 6 */ 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 
    /* occ= 7 */ 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 
    /* occ= 8 */ 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 
    /* occ= 9 */ 0x00, 0x01, 0x01, 0x01, 0x08, 0x09, 0x09, 0x09, 0x08, 0x09, 0x09, 0x09, 0x08, 0x09, 0x09, 0x09, 
    /* occ=10 */ 0x00, 0x02, 0x02, 0x02, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 
    /* occ=11 */ 0x00, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x0B, 0x0B, 0x08, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 
    /* occ=12 */ 0x00, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 
    /* occ=13 */ 0x00, 0x01, 0x0D, 0x0D, 0x0C, 0x0D, 0x0D, 0x0D, 0x0C, 0x0D, 0x0D, 0x0D, 0x0C, 0x0D, 0x0D, 0x0D, 
    /* occ=14 */ 0x00, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 
    /* occ=15 */ 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 
};

bool board_check_win_fast(const board_t *board, player_t player) {
    if (!board || player == PLAYER_NONE) {
        return false;
    }

    const uint8_t required_rows = (uint8_t)kWinRowCount;
    if(++s_progress_callback_counter >= PROGRESS_CALLBACK_FREQUENCY) {
        s_progress_callback_counter = 0;
        ai_agent_call_progress_callback();
    }
    uint8_t row_masks[kWinRowCount];
    // Optimization: Precompute check mask for fast owner check
    // PLAYER_WHITE (0) -> 0x01, PLAYER_BLACK (1) -> 0x02
    const uint8_t check_mask = (player == PLAYER_WHITE) ? 0x01 : 0x02;

    const piece_type_t *row_ptr = &board->cells[WIN_START_ROW][0];
    for (uint8_t rel_row = 0u; rel_row < required_rows; ++rel_row) {
        uint8_t mask = 0u;
        if (row_ptr[0] & check_mask) mask |= 0x01;
        if (row_ptr[1] & check_mask) mask |= 0x02;
        if (row_ptr[2] & check_mask) mask |= 0x04;
        if (row_ptr[3] & check_mask) mask |= 0x08;
        
        if (mask == 0u) {
            return false;
        }
        row_masks[rel_row] = mask;
        row_ptr += BOARD_COLS;
    }

    uint8_t reachable = row_masks[required_rows - 1u];
    for (int8_t idx = (int8_t)required_rows - 2; idx >= 0; --idx) {
        // Cast to uint8_t to avoid C integer promotion to 16-bit int
        uint8_t table_idx = (uint8_t)((row_masks[idx] << 4) | reachable);
        reachable = kConnectivityTable[table_idx];
        if (reachable == 0u) {
            return false;
        }
    }

    return reachable != 0u;
}

bool board_check_win_with_path(const board_t *board, player_t player, win_path_t *out_path) {
    if (out_path) {
        out_path->has_path = false;
        out_path->path_length = 0;
        out_path->winner = PLAYER_NONE;
    }

    if (!board || player == PLAYER_NONE) {
        return false;
    }

    /* BFS traversal with path reconstruction */
    uint8_t queue[BOARD_CELLS];
    uint8_t parent[BOARD_CELLS];
    uint8_t visited[BOARD_CELLS];
    for (uint8_t i = 0; i < BOARD_CELLS; ++i) {
        parent[i] = 0xFF;
        visited[i] = 0;
    }

    uint8_t q_front = 0;
    uint8_t q_back = 0;

    // Seed queue with player's pieces in WIN_START_ROW
    for (uint8_t col = 0; col < BOARD_COLS; ++col) {
        uint8_t idx = (uint8_t)(WIN_START_ROW * BOARD_COLS + col);
        piece_type_t piece = board_get_piece_unchecked(board, WIN_START_ROW, col);
        if (board_get_piece_owner(piece) == player) {
            queue[q_back++] = idx;
            visited[idx] = 1;
            parent[idx] = idx;
        }
    }

    bool win = false;
    uint8_t target_idx = 0xFF;

    while (q_front < q_back) {
        uint8_t current = queue[q_front++];
        uint8_t current_row = current / BOARD_COLS;

        if (current_row == WIN_END_ROW) {
            win = true;
            target_idx = current;
            break;
        }

        uint8_t current_col = current % BOARD_COLS;

        for (uint8_t dir = 0; dir < 8; ++dir) {
            int8_t next_row = (int8_t)current_row + kDirRow[dir];
            int8_t next_col = (int8_t)current_col + kDirCol[dir];
            if (next_row < 0 || next_row >= BOARD_ROWS ||
                next_col < 0 || next_col >= BOARD_COLS) {
                continue;
            }

            uint8_t neighbor_idx = (uint8_t)next_row * BOARD_COLS + (uint8_t)next_col;
            if (visited[neighbor_idx]) {
                continue;
            }

            piece_type_t neighbor_piece = board_get_piece_unchecked(board, (uint8_t)next_row, (uint8_t)next_col);
            if (board_get_piece_owner(neighbor_piece) != player) {
                continue;
            }

            visited[neighbor_idx] = 1;
            parent[neighbor_idx] = current;
            queue[q_back++] = neighbor_idx;
        }
    }

    if (!win) {
        return false;
    }

    if (out_path) {
        out_path->has_path = true;
        out_path->winner = player;
        out_path->path_length = 0;

        uint8_t reversed[BOARD_CELLS];
        uint8_t length = 0;
        uint8_t cursor = target_idx;

        while (cursor < BOARD_CELLS && length < BOARD_CELLS) {
            reversed[length++] = cursor;
            if (parent[cursor] == cursor) {
                break;
            }
            cursor = parent[cursor];
        }

        while (length > 0) {
            --length;
            out_path->path_cells[out_path->path_length++] = reversed[length];
        }
    }

    return true;
}

// Legacy function - now calls the appropriate optimized version
bool board_check_win(const board_t *board, player_t player, win_path_t *out_path) {
    if (out_path) {
        return board_check_win_with_path(board, player, out_path);
    } else {
        return board_check_win_fast(board, player);
    }
}

bool board_has_legal_moves(const board_t *board, player_t player) {
    // Check if player has any pieces that can move
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                // Check all 8 directions
                for (uint8_t dir = 0; dir < 8; ++dir) {
                    int8_t new_row = (int8_t)row + kDirRow[dir];
                    int8_t new_col = (int8_t)col + kDirCol[dir];
                    
                    if (new_row >= 0 && new_row < BOARD_ROWS &&
                        new_col >= 0 && new_col < BOARD_COLS) {
                        if (board_can_move(board, player, row, col, (uint8_t)new_row, (uint8_t)new_col, NULL)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void board_switch_turn(board_context_t *context) {
    context->current_player = (context->current_player == PLAYER_WHITE) ? 
        PLAYER_BLACK : PLAYER_WHITE;
}

uint8_t board_count_pieces(const board_t *board, player_t player) {
    uint8_t count = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                count++;
            }
        }
    }
    return count;
}


