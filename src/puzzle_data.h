/**
 * @file puzzle_data.h
 * @brief Compact puzzle data representation for F256 Switcharoo
 *
 * Streams puzzle definitions from the high-memory binary catalog emitted by
 * `scripts/convert_puzzles.py`, keeping low-memory usage bounded to a single
 * record at a time.
 */

#ifndef PUZZLE_DATA_H
#define PUZZLE_DATA_H

#include "platform_f256.h"
#include "../src/game_state.h"
#include "../src/board.h"
#include <stdint.h>

// Compact piece representation (4 bits total)
// Bits: [swapped:1][player:1][col:2]
#define PIECE_PACK(player, col, swapped) (((swapped) << 3) | ((player) << 2) | (col))
#define PIECE_UNPACK_SWAPPED(packed) ((packed) >> 3)
#define PIECE_UNPACK_PLAYER(packed) (((packed) >> 2) & 0x1)
#define PIECE_UNPACK_COL(packed) ((packed) & 0x3)

// Compact move representation (16 bits total)
// Position encoding: [row:3][col:2] (6 bits total, row 0-7, col 0-3)
#define POS_PACK(row, col) (((row) << 2) | (col))
#define POS_UNPACK_ROW(pos) ((pos) >> 2)
#define POS_UNPACK_COL(pos) ((pos) & 0x3)

// Move encoding: [priority:3][to_pos:6][from_pos:6][type:1]
// type: 0=swap, 1=empty_move
#define MOVE_PACK_SWAP(from_row, from_col, to_row, to_col, priority) \
    (((uint16_t)(priority) << 13) | ((uint16_t)POS_PACK(to_row, to_col) << 7) | ((uint16_t)POS_PACK(from_row, from_col) << 1) | 0)
#define MOVE_PACK_EMPTY(row, col, priority) \
    (((uint16_t)(priority) << 13) | ((uint16_t)POS_PACK(row, col) << 7) | ((uint16_t)0 << 1) | 1)
#define MOVE_UNPACK_TYPE(packed) ((packed) & 0x1)
#define MOVE_UNPACK_FROM_POS(packed) (((packed) >> 1) & 0x3F)
#define MOVE_UNPACK_TO_POS(packed) (((packed) >> 7) & 0x3F)
#define MOVE_UNPACK_POS(packed) (((packed) >> 7) & 0x3F)
#define MOVE_UNPACK_PRIORITY(packed) (((packed) >> 13) & 0x7)

// Puzzle data structure
typedef struct {
    const char *id;                    // Puzzle identifier string
    swap_rule_t swap_rule;             // Swap rule for this puzzle
    uint8_t difficulty;                // Difficulty level
    bool is_solved;                     // Whether the puzzle is solved
    uint8_t piece_count;               // Number of pieces in starting position
    const uint8_t *pieces;             // Packed pieces: [row][packed_piece]...
    uint8_t solution_length;           // Number of moves in solution
    const uint16_t *solution;          // Packed moves: [player][packed_move]...
} puzzle_t;

// Puzzle collection
typedef struct {
    uint16_t count;
    const puzzle_t **puzzles;  // Legacy pointer array (unused in streaming mode)
} puzzle_collection_t;

// Get the global puzzle collection
const puzzle_collection_t *get_puzzle_collection(void);

// Get puzzle by index. Returned pointer remains valid until the next call.
const puzzle_t *get_puzzle_by_index(uint8_t index);

// Convert swap rule string to enum
swap_rule_t swap_rule_from_string(const char *str);

// Convert swap rule enum to string
const char *swap_rule_to_string(swap_rule_t rule);

// Apply puzzle starting position to board
void apply_puzzle_position(board_t *board, const puzzle_t *puzzle);

// Display puzzle solution moves
void display_puzzle_solution(const puzzle_t *puzzle);

void mark_puzzle_solved(uint16_t index);

#endif // PUZZLE_DATA_H