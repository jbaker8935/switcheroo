/**
 * @file board.h
 * @brief Game board model and move validation for F256 Switcharoo
 * 
 * Implements the 8x4 board with piece placement, move rules, and win detection.
 * Per requirements.md: Board state, swap mechanics, and connectivity checks.
 */

#ifndef GAME_BOARD_H
#define GAME_BOARD_H

#include <stdint.h>
#ifdef AI_AGENT_HOST_TEST
#include <stdbool.h>
#endif

#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "platform_f256.h"
#endif

// Swap rule modes
#define NUMBER_OF_SWAP_RULES 4
typedef enum {
    SWAP_RULE_CLASSIC = 0,           // Empty move clears all swapped
    SWAP_RULE_CLEARS_OWN = 1,       // Empty move clears player's swapped
    SWAP_RULE_SWAPPED_CLEARS = 2,   // Swapped piece to empty clears all
    SWAP_RULE_SWAPPED_CLEARS_OWN = 3// Swapped piece to empty clears own
} swap_rule_t;

// Board dimensions
#define BOARD_ROWS 8
#define BOARD_COLS 4
#define BOARD_CELLS (BOARD_ROWS * BOARD_COLS)

// Win condition rows (inclusive)
// Note: Using 0-based indexing. Game rules say "rows 2 and 7" in 1-based numbering.
#define WIN_START_ROW 1
#define WIN_END_ROW 6

// Maximum moves in history
#define MAX_MOVE_HISTORY 40

#define NUM_STARTING_LAYOUTS 4

/**
 * Piece types on the board
 */
typedef enum {
    PIECE_NONE = 0,
    PIECE_WHITE_NORMAL = 1,
    PIECE_WHITE_SWAPPED = 2,
    PIECE_BLACK_NORMAL = 3,
    PIECE_BLACK_SWAPPED = 4
} piece_type_t;

/**
 * Player identification
 */
typedef enum {
    PLAYER_WHITE = 0,
    PLAYER_BLACK = 1,
    PLAYER_NONE = 2
} player_t;

/**
 * Move types
 */
typedef enum {
    MOVE_TYPE_EMPTY = 0,    // Move to empty cell
    MOVE_TYPE_SWAP = 1      // Swap with opponent piece
} move_type_t;

/**
 * Move structure
 */
typedef struct {
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
    move_type_t type;
    player_t player;
} move_t;

/**
 * Board cell
 */
typedef struct {
    piece_type_t piece;
} board_cell_t;

/**
 * Board state
 */
typedef struct {
    board_cell_t cells[BOARD_ROWS][BOARD_COLS];
    player_t current_player;
    uint16_t move_count;
    move_t history[MAX_MOVE_HISTORY];
    uint8_t history_count;
    uint8_t layout_id; // current starting layout id
} board_t;

/**
 * Win path information
 */
typedef struct {
    bool has_path;
    uint8_t path_cells[BOARD_CELLS];  // Indices of cells in winning path
    uint8_t path_length;
    player_t winner;
} win_path_t;

// Board initialization and reset
void board_init(board_t *board);
void board_reset(board_t *board);
void board_set_starting_layout(board_t *board, uint8_t layout_id);

// Piece queries
piece_type_t board_get_piece(const board_t *board, uint8_t row, uint8_t col);
void board_set_piece(board_t *board, uint8_t row, uint8_t col, piece_type_t piece);
static inline player_t board_get_piece_owner(piece_type_t piece) {
    static const player_t owners[5] = { PLAYER_NONE, PLAYER_WHITE, PLAYER_WHITE, PLAYER_BLACK, PLAYER_BLACK };
    return owners[piece];
}
static inline bool board_is_piece_swapped(piece_type_t piece) {
    return piece == PIECE_WHITE_SWAPPED || piece == PIECE_BLACK_SWAPPED;
}
static inline bool board_is_piece_normal(piece_type_t piece) {
    return piece == PIECE_WHITE_NORMAL || piece == PIECE_BLACK_NORMAL;
}

// Move validation
static inline bool board_is_valid_cell(uint8_t row, uint8_t col) {
    return row < BOARD_ROWS && col < BOARD_COLS;
}
bool board_is_adjacent(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2);
bool board_can_move(const board_t *board, uint8_t from_row, uint8_t from_col, 
                    uint8_t to_row, uint8_t to_col, move_type_t *out_type);
uint8_t board_get_legal_moves(const board_t *board, uint8_t row, uint8_t col, 
                               move_t *moves, uint8_t max_moves);

// Move execution
bool board_execute_move(board_t *board, const move_t *move, uint8_t swap_rule);
void board_undo_last_move(board_t *board);

// Win detection
bool board_check_win_fast(const board_t *board, player_t player);
bool board_check_win_with_path(const board_t *board, player_t player, win_path_t *out_path);
bool board_check_win(const board_t *board, player_t player, win_path_t *out_path);
bool board_has_legal_moves(const board_t *board, player_t player);

// Utility
void board_switch_turn(board_t *board);
uint8_t board_count_pieces(const board_t *board, player_t player);
void board_clear_all_swapped(board_t *board);

#endif // GAME_BOARD_H
