/**
 * @file board.c
 * @brief Game board model implementation for F256 Switcharoo
 * 
 * Implements board state management, move validation, and win detection
 * using Union-Find for connectivity checks per design.md.
 */

#include "../src/board.h"
#include <string.h>

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

piece_type_t board_get_piece(const board_t *board, uint8_t row, uint8_t col) {
    if (!board_is_valid_cell(row, col)) {
        return PIECE_NONE;
    }
    return board->cells[row][col].piece;
}

void board_set_piece(board_t *board, uint8_t row, uint8_t col, piece_type_t piece) {
    if (board_is_valid_cell(row, col)) {
        board->cells[row][col].piece = piece;
    }
}

player_t board_get_piece_owner(piece_type_t piece) {
    switch (piece) {
        case PIECE_WHITE_NORMAL:
        case PIECE_WHITE_SWAPPED:
            return PLAYER_WHITE;
        case PIECE_BLACK_NORMAL:
        case PIECE_BLACK_SWAPPED:
            return PLAYER_BLACK;
        default:
            return PLAYER_NONE;
    }
}

bool board_is_piece_swapped(piece_type_t piece) {
    return piece == PIECE_WHITE_SWAPPED || piece == PIECE_BLACK_SWAPPED;
}

bool board_is_piece_normal(piece_type_t piece) {
    return piece == PIECE_WHITE_NORMAL || piece == PIECE_BLACK_NORMAL;
}

bool board_is_valid_cell(uint8_t row, uint8_t col) {
    return row < BOARD_ROWS && col < BOARD_COLS;
}

bool board_is_adjacent(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    int8_t dr = (int8_t)(r2 - r1);
    int8_t dc = (int8_t)(c2 - c1);
    
    // Check if within 1 step in both dimensions
    return (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1 && (dr != 0 || dc != 0));
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

bool board_execute_move(board_t *board, const move_t *move, uint8_t swap_rule_val) {
    swap_rule_t swap_rule = (swap_rule_t)swap_rule_val;
    // Validate move
    move_type_t type;
    if (!board_can_move(board, move->from_row, move->from_col,
                        move->to_row, move->to_col, &type)) {
        // ERROR: Invalid move attempted! Optional diagnostics are disabled in release builds.
        extern void textGotoXY(uint8_t x, uint8_t y);
        textGotoXY(0, 7);
        return false;
    }
    
    // Verify move type matches
    if (type != move->type) {
        extern void textGotoXY(uint8_t x, uint8_t y);
        textGotoXY(0, 7);
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
                board_clear_all_swapped(board);
                break;
            case SWAP_RULE_CLEARS_OWN:
                // Clear only the moving player's swapped pieces
                {
                    player_t mover = board_get_piece_owner(from_piece);
                    if (mover == PLAYER_WHITE) {
                        // Clear white swapped pieces
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_WHITE_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_WHITE_NORMAL);
                                }
                            }
                        }
                    } else if (mover == PLAYER_BLACK) {
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_BLACK_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_BLACK_NORMAL);
                                }
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
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_WHITE_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_WHITE_NORMAL);
                                }
                            }
                        }
                    } else if (mover == PLAYER_BLACK) {
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_BLACK_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_BLACK_NORMAL);
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
    bool any_cleared = false;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);

            if (piece == PIECE_WHITE_SWAPPED) {
                board_set_piece(board, row, col, PIECE_WHITE_NORMAL);
                any_cleared = true;
            } else if (piece == PIECE_BLACK_SWAPPED) {
                board_set_piece(board, row, col, PIECE_BLACK_NORMAL);
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

// Union-Find helper for connectivity
static uint8_t find_root(uint8_t *parent, uint8_t x) {
    if (parent[x] != x) {
        parent[x] = find_root(parent, parent[x]); // Path compression
    }
    return parent[x];
}

static void union_cells(uint8_t *parent, uint8_t x, uint8_t y) {
    uint8_t root_x = find_root(parent, x);
    uint8_t root_y = find_root(parent, y);
    if (root_x != root_y) {
        parent[root_x] = root_y;
    }
}

bool board_check_win(const board_t *board, player_t player, win_path_t *out_path) {
    // Union-Find to detect connected components
    uint8_t parent[BOARD_CELLS];
    bool belongs_to_player[BOARD_CELLS];
    
    // Initialize
    for (uint8_t i = 0; i < BOARD_CELLS; ++i) {
        parent[i] = i;
        belongs_to_player[i] = false;
    }
    
    // Mark cells belonging to player
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            uint8_t idx = row * BOARD_COLS + col;
            belongs_to_player[idx] = (board_get_piece_owner(piece) == player);
        }
    }
    
    // Union adjacent cells belonging to same player
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            uint8_t idx = row * BOARD_COLS + col;
            if (!belongs_to_player[idx]) continue;
            
            // Check all 8 directions
            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kDirRow[dir];
                int8_t new_col = (int8_t)col + kDirCol[dir];
                
                if (new_row >= 0 && new_row < BOARD_ROWS && 
                    new_col >= 0 && new_col < BOARD_COLS) {
                    uint8_t adj_idx = (uint8_t)new_row * BOARD_COLS + (uint8_t)new_col;
                    if (belongs_to_player[adj_idx]) {
                        union_cells(parent, idx, adj_idx);
                    }
                }
            }
        }
    }
    
    // Check if any cell in row 2 connects to any cell in row 7
    for (uint8_t c1 = 0; c1 < BOARD_COLS; ++c1) {
        uint8_t idx1 = WIN_START_ROW * BOARD_COLS + c1;
        if (!belongs_to_player[idx1]) continue;
        
        for (uint8_t c2 = 0; c2 < BOARD_COLS; ++c2) {
            uint8_t idx2 = WIN_END_ROW * BOARD_COLS + c2;
            if (!belongs_to_player[idx2]) continue;
            
            if (find_root(parent, idx1) == find_root(parent, idx2)) {
                // Found winning connection
                if (out_path) {
                    out_path->has_path = true;
                    out_path->winner = player;
                    
                    // Debug: Print which rows connected
                    extern void textGotoXY(uint8_t x, uint8_t y);
                    textGotoXY(0, 9);
                    
                    /* Construct a connected path inside the winning component.
                       Use BFS from any start-row cell in the component to reach
                       any end-row cell in the component and reconstruct the path.
                    */
                    uint8_t root = find_root(parent, idx1);
                    out_path->path_length = 0;

                    // Build list of start and target indices within the component
                    uint8_t start_indices[BOARD_COLS];
                    uint8_t start_count = 0;
                    uint8_t target_indices[BOARD_COLS];
                    uint8_t target_count = 0;

                    for (uint8_t col = 0; col < BOARD_COLS; ++col) {
                        uint8_t sidx = WIN_START_ROW * BOARD_COLS + col;
                        if (belongs_to_player[sidx] && find_root(parent, sidx) == root) {
                            start_indices[start_count++] = sidx;
                        }
                        uint8_t tidx = WIN_END_ROW * BOARD_COLS + col;
                        if (belongs_to_player[tidx] && find_root(parent, tidx) == root) {
                            target_indices[target_count++] = tidx;
                        }
                    }

                    if (start_count == 0 || target_count == 0) {
                        // Fallback: no proper endpoints found, mark as generic path
                        out_path->path_length = 0;
                    } else {
                        // BFS over component cells
                        uint8_t queue[BOARD_CELLS];
                        uint8_t parent_idx[BOARD_CELLS];
                        for (uint8_t i = 0; i < BOARD_CELLS; ++i) parent_idx[i] = 0xFF;
                        uint8_t qh = 0, qt = 0;

                        // Enqueue all start nodes
                        for (uint8_t i = 0; i < start_count; ++i) {
                            uint8_t si = start_indices[i];
                            queue[qt++] = si;
                            parent_idx[si] = si; // root marker
                        }

                        int found_target = -1;
                        while (qh < qt) {
                            uint8_t cur = queue[qh++];
                            // Check if cur is a target
                            for (uint8_t ti = 0; ti < target_count; ++ti) {
                                if (cur == target_indices[ti]) {
                                    found_target = (int)cur;
                                    break;
                                }
                            }
                            if (found_target >= 0) break;

                            uint8_t row = cur / BOARD_COLS;
                            uint8_t col = cur % BOARD_COLS;

                            // Explore 8 neighbors
                            for (int8_t dr = -1; dr <= 1; ++dr) {
                                for (int8_t dc = -1; dc <= 1; ++dc) {
                                    if (dr == 0 && dc == 0) continue;
                                    int8_t nr = (int8_t)row + dr;
                                    int8_t nc = (int8_t)col + dc;
                                    if (nr < 0 || nr >= BOARD_ROWS || nc < 0 || nc >= BOARD_COLS) continue;
                                    uint8_t nidx = (uint8_t)nr * BOARD_COLS + (uint8_t)nc;
                                    if (!belongs_to_player[nidx]) continue;
                                    if (find_root(parent, nidx) != root) continue;
                                    if (parent_idx[nidx] != 0xFF) continue; // visited
                                    parent_idx[nidx] = cur;
                                    queue[qt++] = nidx;
                                }
                            }
                        }

                        if (found_target >= 0) {
                            // Reconstruct path from target back to a start node
                            uint8_t rev_path[BOARD_CELLS];
                            uint8_t rev_len = 0;
                            uint8_t cur = (uint8_t)found_target;
                            while (parent_idx[cur] != cur && rev_len < BOARD_CELLS) {
                                rev_path[rev_len++] = cur;
                                cur = parent_idx[cur];
                            }
                            // add the start node
                            rev_path[rev_len++] = cur;

                            // Reverse into out_path in start->target order
                            for (int i = (int)rev_len - 1; i >= 0; --i) {
                                out_path->path_cells[out_path->path_length++] = rev_path[i];
                            }
                        } else {
                            out_path->path_length = 0; // no path found
                        }
                    }
                }
                return true;
            }
        }
    }
    
    if (out_path) {
        out_path->has_path = false;
        out_path->path_length = 0;
    }
    return false;
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
