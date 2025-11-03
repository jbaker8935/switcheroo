/**
 * @file board.h
 * @brief Game board model and move validation for F256 Switcharoo
 * 
 * Implements the 8x4 board with piece placement, move rules, and win detection.
 * Per requirements.md: Board state, swap mechanics, and connectivity checks.
 */

#ifndef GAME_BOARD_H
#define GAME_BOARD_H

#include <string.h>

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
// Using 0-based indexing. Game rules say "rows 2 and 7" in 1-based numbering.
#define WIN_START_ROW 1
#define WIN_END_ROW 6

// Maximum moves in history
#define MAX_MOVE_HISTORY 40

#define NUM_STARTING_LAYOUTS 4

/**
 * Piece types on the board (bit-based for efficiency)
 * Bit 0: White, Bit 1: Black, Bit 2: Swapped
 */
typedef uint8_t piece_type_t;
#define PIECE_NONE 0x00
#define PIECE_WHITE_NORMAL 0x01
#define PIECE_WHITE_SWAPPED 0x05
#define PIECE_BLACK_NORMAL 0x02
#define PIECE_BLACK_SWAPPED 0x06

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
 * Struct-of-arrays representation for efficient move storage on 6502
 * This layout optimizes for the 6502's absolute indexed addressing mode
 * by keeping fields of the same type contiguous in memory.
 */
typedef struct {
    uint8_t from_row[32];    // Max moves per position
    uint8_t from_col[32];
    uint8_t to_row[32];
    uint8_t to_col[32];
    move_type_t type[32];
    player_t player[32];
    uint8_t count;           // Number of valid moves in arrays
} move_array_t;

// Accessor functions for SOA move array - optimized for 6502 absolute indexed addressing
static inline uint8_t move_array_get_from_row(const move_array_t *moves, uint8_t index) {
    return moves->from_row[index];
}
static inline uint8_t move_array_get_from_col(const move_array_t *moves, uint8_t index) {
    return moves->from_col[index];
}
static inline uint8_t move_array_get_to_row(const move_array_t *moves, uint8_t index) {
    return moves->to_row[index];
}
static inline uint8_t move_array_get_to_col(const move_array_t *moves, uint8_t index) {
    return moves->to_col[index];
}
static inline move_type_t move_array_get_type(const move_array_t *moves, uint8_t index) {
    return moves->type[index];
}
static inline player_t move_array_get_player(const move_array_t *moves, uint8_t index) {
    return moves->player[index];
}

// Setter functions for SOA move array
static inline void move_array_set_from_row(move_array_t *moves, uint8_t index, uint8_t value) {
    moves->from_row[index] = value;
}
static inline void move_array_set_from_col(move_array_t *moves, uint8_t index, uint8_t value) {
    moves->from_col[index] = value;
}
static inline void move_array_set_to_row(move_array_t *moves, uint8_t index, uint8_t value) {
    moves->to_row[index] = value;
}
static inline void move_array_set_to_col(move_array_t *moves, uint8_t index, uint8_t value) {
    moves->to_col[index] = value;
}
static inline void move_array_set_type(move_array_t *moves, uint8_t index, move_type_t value) {
    moves->type[index] = value;
}
static inline void move_array_set_player(move_array_t *moves, uint8_t index, player_t value) {
    moves->player[index] = value;
}

// Utility functions
static inline uint8_t move_array_get_count(const move_array_t *moves) {
    return moves->count;
}
static inline void move_array_set_count(move_array_t *moves, uint8_t count) {
    moves->count = count;
}

// Convert SOA move to traditional struct (for compatibility)
static inline void move_array_get_move(const move_array_t *moves, uint8_t index, move_t *out_move) {
    out_move->from_row = moves->from_row[index];
    out_move->from_col = moves->from_col[index];
    out_move->to_row = moves->to_row[index];
    out_move->to_col = moves->to_col[index];
    out_move->type = moves->type[index];
    out_move->player = moves->player[index];
}

// Set SOA move from traditional struct
static inline void move_array_set_move(move_array_t *moves, uint8_t index, const move_t *move) {
    moves->from_row[index] = move->from_row;
    moves->from_col[index] = move->from_col;
    moves->to_row[index] = move->to_row;
    moves->to_col[index] = move->to_col;
    moves->type[index] = move->type;
    moves->player[index] = move->player;
}

/**
 * Board cell
 */
typedef uint8_t board_cell_t;

/**
 * Board state
 */
/**
 * AI-optimized board state (stripped for fast copying during search)
 */
typedef struct {
    board_cell_t cells[BOARD_ROWS][BOARD_COLS];
    uint8_t swapped_count;
    uint8_t white_swapped_count;
    uint8_t black_swapped_count;
    uint8_t white_winning_rows;
    uint8_t black_winning_rows;
    uint16_t move_count;
} board_t;

/**
 * Board context for additional state
 */
typedef struct {
    player_t current_player;
    move_t history[MAX_MOVE_HISTORY];
    uint8_t history_count;
    uint8_t layout_id; // current starting layout id
    player_t last_moving_player;
} board_context_t;

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
static inline piece_type_t board_get_piece_unchecked(const board_t *board, uint8_t row, uint8_t col) {
    return board->cells[row][col];
}
void board_set_piece(board_t *board, uint8_t row, uint8_t col, piece_type_t piece);
static inline player_t board_get_piece_owner(piece_type_t piece) {
    if (piece & 0x01) return PLAYER_WHITE;
    if (piece & 0x02) return PLAYER_BLACK;
    return PLAYER_NONE;
}
static inline bool board_is_piece_swapped(piece_type_t piece) {
    return (piece & 0x04) != 0;
}
static inline bool board_is_piece_normal(piece_type_t piece) {
    return (piece & 0x04) == 0;
}

// Move validation
static inline bool board_is_valid_cell(uint8_t row, uint8_t col) {
    return row < BOARD_ROWS && col < BOARD_COLS;
}
// Inline for performance: adjacency check
static inline bool board_is_adjacent(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    int8_t dr = (int8_t)(r2 - r1);
    int8_t dc = (int8_t)(c2 - c1);
    return (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1 && (dr != 0 || dc != 0));
}
bool board_can_move(const board_t *board, player_t current_player, uint8_t from_row, uint8_t from_col, 
                    uint8_t to_row, uint8_t to_col, move_type_t *out_type);
uint8_t board_get_legal_moves(const board_t *board, player_t current_player, uint8_t row, uint8_t col, 
                               move_t *moves, uint8_t max_moves);

// SOA version for better 6502 performance
uint8_t board_get_legal_moves_soa(const board_t *board, player_t current_player, uint8_t row, uint8_t col,
                                  move_array_t *out_moves);

// Move execution
bool board_execute_move(board_t *board, board_context_t *context, const move_t *move, uint8_t swap_rule);
bool board_execute_move_without_history(board_t *board, board_context_t *context, const move_t *move, uint8_t swap_rule);
void board_undo_last_move(board_t *board);

// Win detection
bool board_check_win_fast(const board_t *board, player_t player);
bool board_check_win_with_path(const board_t *board, player_t player, win_path_t *out_path);
bool board_check_win(const board_t *board, player_t player, win_path_t *out_path);

// AI board utilities
static inline void board_copy(board_t *dest, const board_t *src) {
    memcpy(dest, src, sizeof(board_t));
}
bool board_can_move_unchecked(const board_t *board, player_t current_player, uint8_t from_row, uint8_t from_col,
                                 uint8_t to_row, uint8_t to_col, move_type_t *out_type);
uint8_t board_get_legal_moves_soa(const board_t *board, player_t current_player, uint8_t row, uint8_t col,
                                  move_array_t *out_moves);


// Utility
void board_switch_turn(board_context_t *context);
uint8_t board_count_pieces(const board_t *board, player_t player);
void board_clear_all_swapped(board_t *board);
void board_update_winning_row_counts(board_t *board);

#endif // GAME_BOARD_H
