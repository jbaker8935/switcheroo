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
#include "game_state.h"
#include "board.h"
#include <stdint.h>

// Solved-bit persistence constants (increase if the catalog expands beyond 600 entries)
#define PUZZLE_SOLVED_CAPACITY 600u
#define PUZZLE_SOLVED_BYTES 75u
#define PUZZLE_CATALOG_SIGNATURE_BYTES 8u


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
typedef struct puzzle_t {
    const char *id;                    // Puzzle identifier string
    swap_rule_t swap_rule;             // Swap rule for this puzzle
    uint8_t difficulty;                // Difficulty level
    bool is_solved;                    // Whether the puzzle is solved
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

// Get puzzle by filtered index. Returned pointer remains valid until the
// next call.
const puzzle_t *get_puzzle_by_index(uint16_t filtered_index);

// Set the current swap rule for puzzle filtering
void set_current_puzzle_swap_rule(swap_rule_t rule);

// Get the current swap rule for puzzle filtering
swap_rule_t get_current_puzzle_swap_rule(void);

// Convert swap rule string to enum
swap_rule_t swap_rule_from_string(const char *str);

// Convert swap rule enum to string
const char *swap_rule_to_string(swap_rule_t rule);

// Apply puzzle starting position to board
void apply_puzzle_position(board_t *board, const puzzle_t *puzzle);

// Display puzzle solution moves
void display_puzzle_solution(const puzzle_t *puzzle);

// Mark the puzzle at the current swap-rule filtered index as solved
void mark_puzzle_solved(uint16_t filtered_index);

// Serialize the solved state of all puzzles (1 bit per puzzle, packed into bytes)
size_t puzzle_catalog_serialize_solved(uint8_t *buffer, size_t max_bytes);

// Deserialize the solved state of all puzzles from a buffer (1 bit per puzzle, packed into bytes)
uint8_t puzzle_catalog_deserialize_solved(const uint8_t *buffer, size_t length);

// Retrieve the current puzzle catalog signature (8 bytes, little-endian).
typedef struct { uint8_t bytes[8]; } sig64_t;
sig64_t puzzle_catalog_signature(void);

// Clear the in-memory solved bitset.
void puzzle_catalog_clear_solved_state(void);

// Invalidate cached catalog metadata to force a reload on next access.
void puzzle_catalog_invalidate_cache(void);

#endif // PUZZLE_DATA_H
