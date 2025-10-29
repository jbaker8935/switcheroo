/**
 * @file board.c
 * @brief Game board model implementation for F256 Switcharoo
 * 
 * Implements board state management, move validation, and win detection
 * using a BFS connectivity check per design.md guidance.
 */

#include "../src/board.h"
#include <string.h>

// Extern declarations for inline functions used across multiple translation units
extern piece_type_t board_get_piece(const board_t *board, uint8_t row, uint8_t col);
extern void board_set_piece(board_t *board, uint8_t row, uint8_t col, piece_type_t piece);
extern bool board_is_adjacent(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2);
extern bool board_can_move(const board_t *board, uint8_t from_row, uint8_t from_col,
                           uint8_t to_row, uint8_t to_col, move_type_t *out_type);

// Internal versions that skip bounds checking for performance

static inline void board_set_piece_unchecked(board_t *board, uint8_t row, uint8_t col, piece_type_t piece) {
    piece_type_t old_piece = board->cells[row][col].piece;
    board->cells[row][col].piece = piece;
    
    // Update swapped piece count
    if (board_is_piece_swapped(old_piece) && !board_is_piece_swapped(piece)) {
        board->swapped_count--;
    } else if (!board_is_piece_swapped(old_piece) && board_is_piece_swapped(piece)) {
        board->swapped_count++;
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


bool board_can_move(const board_t *board, uint8_t from_row, uint8_t from_col,
                    uint8_t to_row, uint8_t to_col, move_type_t *out_type) {
    // Validate cells
    if (!board_is_valid_cell(from_row, from_col) || !board_is_valid_cell(to_row, to_col)) {
        return false;
    }
    
    // Check adjacency
    if (!board_is_adjacent(from_row, from_col, to_row, to_col)) {
        return false;
    }
    
    // Get pieces
    piece_type_t from_piece = board_get_piece(board, from_row, from_col);
    piece_type_t to_piece = board_get_piece(board, to_row, to_col);
    
    // Check that source has a piece belonging to current player
    if (board_get_piece_owner(from_piece) != board->current_player) {
        return false;
    }
    
    // Check that target cell does not contain current player's piece
    if (to_piece != PIECE_NONE) {
        player_t to_owner = board_get_piece_owner(to_piece);
        if (to_owner == board->current_player) {
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
    if (to_owner != board->current_player && board_is_piece_normal(to_piece)) {
        if (out_type) *out_type = MOVE_TYPE_SWAP;
        return true;
    }
    
    // If we reach here, the target is an opponent's piece but NOT normal (i.e., swapped)
    // This should not be a valid move
    return false;
}

// Unchecked version of board_can_move - assumes all cells are valid
// Used in performance-critical loops where bounds are already verified
bool board_can_move_unchecked(const board_t *board, uint8_t from_row, uint8_t from_col,
                              uint8_t to_row, uint8_t to_col, move_type_t *out_type) {
    // Skip cell validation - caller guarantees validity
    
    // Check adjacency
    if (!board_is_adjacent(from_row, from_col, to_row, to_col)) {
        return false;
    }
    
    // Get pieces
    piece_type_t from_piece = board_get_piece(board, from_row, from_col);
    piece_type_t to_piece = board_get_piece(board, to_row, to_col);
    
    // Check that source has a piece belonging to current player
    if (board_get_piece_owner(from_piece) != board->current_player) {
        return false;
    }
    
    // Check that target cell does not contain current player's piece
    if (to_piece != PIECE_NONE) {
        player_t to_owner = board_get_piece_owner(to_piece);
        if (to_owner == board->current_player) {
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
    if (to_owner != board->current_player && board_is_piece_normal(to_piece)) {
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
    board_set_starting_layout(board, board->layout_id);
    board->current_player = PLAYER_WHITE;
    board->move_count = 0;
    board->history_count = 0;
    board->swapped_count = 0;  // No swapped pieces in starting layout
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
    board->layout_id = layout_id;
    board->current_player = PLAYER_WHITE;
    board->move_count = 0;
    board->history_count = 0;
}


uint8_t board_get_legal_moves(const board_t *board, uint8_t row, uint8_t col,
                               move_t *moves, uint8_t max_moves) {
    uint8_t count = 0;
    
    // Check all 8 directions
    for (uint8_t dir = 0; dir < 8 && count < max_moves; ++dir) {
        int8_t new_row = (int8_t)row + kDirRow[dir];
        int8_t new_col = (int8_t)col + kDirCol[dir];
        
        if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
            move_type_t type;
            if (board_can_move(board, row, col, (uint8_t)new_row, (uint8_t)new_col, &type)) {
                moves[count].from_row = row;
                moves[count].from_col = col;
                moves[count].to_row = (uint8_t)new_row;
                moves[count].to_col = (uint8_t)new_col;
                moves[count].type = type;
                moves[count].player = board->current_player;
                count++;
            }
        }
    }
    
    return count;
}

// SOA version for better 6502 performance - uses struct of arrays layout
uint8_t board_get_legal_moves_soa(const board_t *board, uint8_t row, uint8_t col,
                                  move_array_t *moves) {
    uint8_t count = 0;
    
    // Check all 8 directions
    for (uint8_t dir = 0; dir < 8 && count < 32; ++dir) {
        int8_t new_row = (int8_t)row + kDirRow[dir];
        int8_t new_col = (int8_t)col + kDirCol[dir];
        
        if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
            move_type_t type;
            if (board_can_move_unchecked(board, row, col, (uint8_t)new_row, (uint8_t)new_col, &type)) {
                moves->from_row[count] = row;
                moves->from_col[count] = col;
                moves->to_row[count] = (uint8_t)new_row;
                moves->to_col[count] = (uint8_t)new_col;
                moves->type[count] = type;
                moves->player[count] = board->current_player;
                count++;
            }
        }
    }
    
    moves->count = count;
    return count;
}

bool board_execute_move(board_t *board, const move_t *move, uint8_t swap_rule_val) {
    swap_rule_t swap_rule = (swap_rule_t)swap_rule_val;
    // Validate move
    move_type_t type;
    if (!board_can_move(board, move->from_row, move->from_col,
                        move->to_row, move->to_col, &type)) {
        return false;
    }
    
    // Verify move type matches
    if (type != move->type) {
        return false;
    }
    
    // Save to history: shift existing moves and place new one at index 0
    for (int i = MAX_MOVE_HISTORY - 1; i > 0; --i) {
        board->history[i] = board->history[i - 1];
    }
    board->history[0] = *move;
    if (board->history_count < MAX_MOVE_HISTORY) {
        board->history_count++;
    }
    
    piece_type_t from_piece = board_get_piece(board, move->from_row, move->from_col);
    piece_type_t to_piece = board_get_piece(board, move->to_row, move->to_col);
    
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
    // Ensure renderer updates immediately to reflect new piece states
    extern void render_invalidate_cache(void);
    render_invalidate_cache();
    return true;
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
        extern void render_invalidate_cache(void);
        render_invalidate_cache();
    }
}

bool board_check_win_fast(const board_t *board, player_t player) {
    if (!board || player == PLAYER_NONE) {
        return false;
    }

    // Short-circuit: Check if player has pieces on each row between WIN_START_ROW and WIN_END_ROW
    // If any row is missing the player's pieces, it's impossible to have a winning path
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        bool has_piece_on_row = false;
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            if (board_get_piece_owner(board_get_piece(board, row, col)) == player) {
                has_piece_on_row = true;
                break;
            }
        }
        if (!has_piece_on_row) {
            return false;
        }
    }

    /* Optimized BFS for 6502: minimal memory usage, early termination */
    uint8_t queue[BOARD_CELLS];
    uint8_t visited[BOARD_CELLS];
    for (uint8_t i = 0; i < BOARD_CELLS; ++i) {
        visited[i] = 0;
    }

    uint8_t q_front = 0;
    uint8_t q_back = 0;

    // Seed queue with player's pieces in WIN_START_ROW
    for (uint8_t col = 0; col < BOARD_COLS; ++col) {
        uint8_t idx = (uint8_t)(WIN_START_ROW * BOARD_COLS + col);
        piece_type_t piece = board_get_piece(board, WIN_START_ROW, col);
        if (board_get_piece_owner(piece) == player) {
            queue[q_back++] = idx;
            visited[idx] = 1;
        }
    }

    // BFS traversal - no parent tracking needed for win detection
    while (q_front < q_back) {
        uint8_t current = queue[q_front++];
        uint8_t current_row = current / BOARD_COLS;

        if (current_row == WIN_END_ROW) {
            return true;  // Win found!
        }

        uint8_t current_col = current % BOARD_COLS;

        // Check all 8 directions for adjacent cells
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

            piece_type_t neighbor_piece = board_get_piece(board, (uint8_t)next_row, (uint8_t)next_col);
            if (board_get_piece_owner(neighbor_piece) != player) {
                continue;
            }

            visited[neighbor_idx] = 1;
            queue[q_back++] = neighbor_idx;
        }
    }

    return false;
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

    // Short-circuit: Check if player has pieces on each row between WIN_START_ROW and WIN_END_ROW
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        bool has_piece_on_row = false;
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            if (board_get_piece_owner(board_get_piece(board, row, col)) == player) {
                has_piece_on_row = true;
                break;
            }
        }
        if (!has_piece_on_row) {
            return false;
        }
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
        piece_type_t piece = board_get_piece(board, WIN_START_ROW, col);
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

            piece_type_t neighbor_piece = board_get_piece(board, (uint8_t)next_row, (uint8_t)next_col);
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
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                // Check all 8 directions
                for (uint8_t dir = 0; dir < 8; ++dir) {
                    int8_t new_row = (int8_t)row + kDirRow[dir];
                    int8_t new_col = (int8_t)col + kDirCol[dir];
                    
                    if (new_row >= 0 && new_row < BOARD_ROWS &&
                        new_col >= 0 && new_col < BOARD_COLS) {
                        if (board_can_move(board, row, col, (uint8_t)new_row, (uint8_t)new_col, NULL)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void board_switch_turn(board_t *board) {
    board->current_player = (board->current_player == PLAYER_WHITE) ? 
        PLAYER_BLACK : PLAYER_WHITE;
}

uint8_t board_count_pieces(const board_t *board, player_t player) {
    uint8_t count = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                count++;
            }
        }
    }
    return count;
}
