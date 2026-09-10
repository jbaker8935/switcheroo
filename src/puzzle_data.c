

#include "puzzle_data.h"
#include "board.h"
#include "text_display.h"
#include "sram_assets.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "platform_f256.h"
#include "overlay_config.h"

enum {
    PUZZLE_SIGNATURE_BYTES = PUZZLE_CATALOG_SIGNATURE_BYTES,
    PUZZLE_COUNT_BYTES = 2u,
    PUZZLE_ID_BYTES = 32u,
    PUZZLE_MAX_PIECES = 16u,
    PUZZLE_PIECE_BYTES = PUZZLE_MAX_PIECES * 2u,
    PUZZLE_MAX_SOLUTION_MOVES = 9u,
    PUZZLE_SOLUTION_WORDS = PUZZLE_MAX_SOLUTION_MOVES * 2u,
    PUZZLE_SOLUTION_BYTES = PUZZLE_SOLUTION_WORDS * 2u,
    PUZZLE_HEADER_BYTES = PUZZLE_SIGNATURE_BYTES + PUZZLE_COUNT_BYTES,
    PUZZLE_RECORD_BYTES = PUZZLE_ID_BYTES + 2u + 1u + 1u + 1u +
    PUZZLE_PIECE_BYTES + 1u + PUZZLE_SOLUTION_BYTES
};

static char s_puzzle_id_buffer[PUZZLE_ID_BYTES];
static uint8_t s_puzzle_piece_buffer[PUZZLE_PIECE_BYTES];
static uint16_t s_puzzle_solution_buffer[PUZZLE_SOLUTION_WORDS];

static puzzle_t s_puzzle_cache = {
    s_puzzle_id_buffer,
    SWAP_RULE_CLASSIC,
    1u,
    false,
    0u,
    s_puzzle_piece_buffer,
    0u,
    s_puzzle_solution_buffer
};

static uint16_t s_rule_counts[NUMBER_OF_SWAP_RULES] = {0u};

typedef struct {
    swap_rule_t rule;
    uint16_t filtered_index;
    uint16_t actual_index;
} puzzle_index_cache_t;

static puzzle_index_cache_t s_last_index_lookup = {
    SWAP_RULE_CLASSIC,
    UINT16_MAX,
    0u
};

static swap_rule_t s_current_puzzle_swap_rule = SWAP_RULE_CLASSIC;

static puzzle_collection_t s_puzzle_collection = {
    0u,
    NULL
};

static bool s_header_loaded = false;
static sig64_t s_puzzle_catalog_signature_value;
static bool s_puzzle_cache_valid = false;
static uint16_t s_puzzle_cache_index = 0u;

static uint8_t s_solved_bitset[PUZZLE_SOLVED_BYTES] = {0u};
static uint16_t s_solved_bit_count = 0u;
static size_t s_solved_bitset_bytes = 0u;
static bool s_solved_bitset_ready = false;


#pragma code(ovl13_code)
static void FAR_puzzle_catalog_init_solved_bits(uint16_t count) {
    s_solved_bit_count = (count <= PUZZLE_SOLVED_CAPACITY) ? count : PUZZLE_SOLVED_CAPACITY;
    s_solved_bitset_bytes = (size_t)((s_solved_bit_count + 7u) / 8u);
    if (s_solved_bitset_bytes > sizeof(s_solved_bitset)) {
        s_solved_bitset_bytes = sizeof(s_solved_bitset);
    }
    memset(s_solved_bitset, 0, sizeof(s_solved_bitset));
    s_solved_bitset_ready = true;
    if (count > PUZZLE_SOLVED_CAPACITY) {
        print_puzzle_debug("PUZ BITSET TRUNC", "INCREASE CAPACITY");
    }
}
#pragma code(code)

static void puzzle_catalog_init_solved_bits(uint16_t count) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_puzzle_catalog_init_solved_bits(count);
    POKE(OVERLAY_MMU_REG, saved);
}

static bool puzzle_catalog_solved_bit_get(uint16_t index) {
    if (!s_solved_bitset_ready || index >= s_solved_bit_count || s_solved_bitset_bytes == 0u) {
        return false;
    }
    const uint16_t byte_index = index >> 3;
    if (byte_index >= s_solved_bitset_bytes) {
        return false;
    }
    const uint8_t mask = (uint8_t)(1u << (index & 7u));
    return (s_solved_bitset[byte_index] & mask) != 0u;
}

static void puzzle_catalog_solved_bit_set(uint16_t index, bool solved) {
    if (!s_solved_bitset_ready || index >= s_solved_bit_count || s_solved_bitset_bytes == 0u) {
        return;
    }
    const uint16_t byte_index = index >> 3;
    if (byte_index >= s_solved_bitset_bytes) {
        return;
    }
    const uint8_t mask = (uint8_t)(1u << (index & 7u));
    if (solved) {
        s_solved_bitset[byte_index] |= mask;
    } else {
        s_solved_bitset[byte_index] &= (uint8_t)~mask;
    }
    if (s_puzzle_cache_valid && s_puzzle_cache_index == index) {
        s_puzzle_cache.is_solved = solved;
    }
}

static uint8_t puzzle_catalog_read_byte(uint32_t offset) {
    return platform_far_read_byte(SRAM_PUZZLE_CATALOG + offset);
}

static uint16_t puzzle_catalog_read_word(uint32_t offset) {
    return platform_far_read_word(SRAM_PUZZLE_CATALOG + offset);
}

static void puzzle_catalog_reset_index_cache(void) {
    s_last_index_lookup.filtered_index = UINT16_MAX;
    s_last_index_lookup.actual_index = 0u;
    s_last_index_lookup.rule = s_current_puzzle_swap_rule;
}

static swap_rule_t puzzle_catalog_rule_for_index(uint16_t index) {
    const uint32_t record_offset = PUZZLE_HEADER_BYTES + (uint32_t)index * PUZZLE_RECORD_BYTES;
    const uint16_t swap_rule_value = puzzle_catalog_read_word(record_offset + PUZZLE_ID_BYTES);
    return (swap_rule_value <= SWAP_RULE_SWAPPED_CLEARS_OWN) ? (swap_rule_t)swap_rule_value : SWAP_RULE_CLASSIC;
}

#pragma code(ovl13_code)
static void FAR_puzzle_catalog_ensure_header(void) {
    if (s_header_loaded) {
        return;
    }

    for (uint8_t i = 0u; i < PUZZLE_SIGNATURE_BYTES; ++i) {
        s_puzzle_catalog_signature_value.bytes[i] = puzzle_catalog_read_byte(i);
    }

    const uint16_t count = (uint16_t)puzzle_catalog_read_byte(PUZZLE_SIGNATURE_BYTES) |
        ((uint16_t)puzzle_catalog_read_byte(PUZZLE_SIGNATURE_BYTES + 1u) << 8);
    s_puzzle_collection.count = count;
    s_puzzle_collection.puzzles = NULL;

    if (!s_solved_bitset_ready) {
        puzzle_catalog_init_solved_bits(count);
    }

    memset(s_rule_counts, 0, sizeof(s_rule_counts));
    for (uint16_t i = 0u; i < count; ++i) {
        const swap_rule_t rule = puzzle_catalog_rule_for_index(i);
        if (rule < NUMBER_OF_SWAP_RULES) {
            ++s_rule_counts[rule];
        }
    }

    puzzle_catalog_reset_index_cache();
    s_header_loaded = true;
}
#pragma code(code)

static void puzzle_catalog_ensure_header(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_puzzle_catalog_ensure_header();
    POKE(OVERLAY_MMU_REG, saved);
}

#pragma code(ovl13_code)
static bool FAR_puzzle_catalog_map_filtered_index(swap_rule_t rule, uint16_t filtered_index, uint16_t *out_actual_index) {
    puzzle_catalog_ensure_header();

    if (rule >= NUMBER_OF_SWAP_RULES || filtered_index >= s_rule_counts[rule]) {
        return false;
    }

    uint16_t start_actual = 0u;
    uint16_t matched = 0u;

    if (s_last_index_lookup.filtered_index != UINT16_MAX && s_last_index_lookup.rule == rule) {
        if (s_last_index_lookup.filtered_index == filtered_index) {
            *out_actual_index = s_last_index_lookup.actual_index;
            return true;
        }
        if (filtered_index > s_last_index_lookup.filtered_index) {
            start_actual = s_last_index_lookup.actual_index + 1u;
            matched = s_last_index_lookup.filtered_index + 1u;
        }
    }

    for (uint16_t actual = start_actual; actual < s_puzzle_collection.count; ++actual) {
        if (puzzle_catalog_rule_for_index(actual) == rule) {
            if (matched == filtered_index) {
                *out_actual_index = actual;
                s_last_index_lookup.rule = rule;
                s_last_index_lookup.filtered_index = filtered_index;
                s_last_index_lookup.actual_index = actual;
                return true;
            }
            ++matched;
        }
    }

    return false;
}
#pragma code(code)

static bool puzzle_catalog_map_filtered_index(swap_rule_t rule, uint16_t filtered_index, uint16_t *out_actual_index) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    bool result = FAR_puzzle_catalog_map_filtered_index(rule, filtered_index, out_actual_index);
    POKE(OVERLAY_MMU_REG, saved);
    return result;
}

void set_current_puzzle_swap_rule(swap_rule_t rule) {
    s_current_puzzle_swap_rule = rule;
    puzzle_catalog_reset_index_cache();
}

swap_rule_t get_current_puzzle_swap_rule(void) {
    return s_current_puzzle_swap_rule;
}

static bool puzzle_catalog_load_record(uint16_t index) {
    puzzle_catalog_ensure_header();
    if (index >= s_puzzle_collection.count) {
        return false;
    }
    
    const uint32_t record_offset = PUZZLE_HEADER_BYTES +
    (uint32_t)index * PUZZLE_RECORD_BYTES;
    
    memset(s_puzzle_id_buffer, 0, sizeof(s_puzzle_id_buffer));
    for (uint8_t i = 0u; i < PUZZLE_ID_BYTES; ++i) {
        s_puzzle_id_buffer[i] = (char)puzzle_catalog_read_byte(record_offset + i);
    }
    s_puzzle_id_buffer[PUZZLE_ID_BYTES - 1u] = '\0';
    
    uint16_t swap_rule_value = puzzle_catalog_read_word(
        record_offset + PUZZLE_ID_BYTES
    );
    if (swap_rule_value > SWAP_RULE_SWAPPED_CLEARS_OWN) {
        s_puzzle_cache.swap_rule = SWAP_RULE_CLASSIC;
    } else {
        s_puzzle_cache.swap_rule = (swap_rule_t)swap_rule_value;
    }
    
    s_puzzle_cache.difficulty = puzzle_catalog_read_byte(
        record_offset + PUZZLE_ID_BYTES + 2u
    );
    
    uint8_t piece_count = puzzle_catalog_read_byte(
        record_offset + PUZZLE_ID_BYTES + 4u
    );
    if (piece_count > PUZZLE_MAX_PIECES) {
        piece_count = PUZZLE_MAX_PIECES;
    }
    s_puzzle_cache.piece_count = piece_count;
    
    memset(s_puzzle_piece_buffer, 0, sizeof(s_puzzle_piece_buffer));
    const uint32_t pieces_offset = record_offset + PUZZLE_ID_BYTES + 5u;
    for (uint8_t i = 0u; i < PUZZLE_PIECE_BYTES; ++i) {
        s_puzzle_piece_buffer[i] = puzzle_catalog_read_byte(pieces_offset + i);
    }
    
    uint8_t solution_length = puzzle_catalog_read_byte(
        pieces_offset + PUZZLE_PIECE_BYTES
    );
    if (solution_length > PUZZLE_MAX_SOLUTION_MOVES) {
        solution_length = PUZZLE_MAX_SOLUTION_MOVES;
    }
    s_puzzle_cache.solution_length = solution_length;
    
    memset(s_puzzle_solution_buffer, 0, sizeof(s_puzzle_solution_buffer));
    const uint32_t solution_offset = pieces_offset + PUZZLE_PIECE_BYTES + 1u;
    for (uint8_t i = 0u; i < PUZZLE_SOLUTION_WORDS; ++i) {
        s_puzzle_solution_buffer[i] = puzzle_catalog_read_word(
            solution_offset + (uint32_t)i * 2u
        );
    }
    
    s_puzzle_cache.is_solved = puzzle_catalog_solved_bit_get(index);

    s_puzzle_cache_valid = true;
    s_puzzle_cache_index = index;
    
    return true;
}

// Sets is_solved to true for a given swap-rule filtered puzzle index
void mark_puzzle_solved(uint16_t filtered_index) {
    uint16_t actual_index;
    if (!puzzle_catalog_map_filtered_index(s_current_puzzle_swap_rule, filtered_index, &actual_index)) {
        return;
    }

    // Load the puzzle record for the given actual index
    // if (!puzzle_catalog_load_record(actual_index)) {
    //     return;
    // }
    
    puzzle_catalog_solved_bit_set(actual_index, true);


}

const puzzle_collection_t *get_puzzle_collection(void) {
    puzzle_catalog_ensure_header();
    static puzzle_collection_t filtered_collection = {0, NULL};
    if (s_current_puzzle_swap_rule < NUMBER_OF_SWAP_RULES) {
        filtered_collection.count = s_rule_counts[s_current_puzzle_swap_rule];
    } else {
        filtered_collection.count = 0u;
    }
    return &filtered_collection;
}

const puzzle_t *get_puzzle_by_index(uint16_t filtered_index) {
    uint16_t actual_index;
    if (!puzzle_catalog_map_filtered_index(s_current_puzzle_swap_rule, filtered_index, &actual_index)) {
        return NULL;
    }
    
    if (s_puzzle_cache_valid && s_puzzle_cache_index == actual_index) {
        s_puzzle_cache.is_solved = puzzle_catalog_solved_bit_get(actual_index);
        return &s_puzzle_cache;
    }
    
    if (!puzzle_catalog_load_record(actual_index)) {
        return NULL;
    }

    return &s_puzzle_cache;
}

swap_rule_t swap_rule_from_string(const char *str) {
    if (strcmp(str, "classic") == 0) {
        return SWAP_RULE_CLASSIC;
    }
    if (strcmp(str, "clears_own") == 0) {
        return SWAP_RULE_CLEARS_OWN;
    }
    if (strcmp(str, "swapped_clears") == 0) {
        return SWAP_RULE_SWAPPED_CLEARS;
    }
    if (strcmp(str, "swapped_clears_own") == 0) {
        return SWAP_RULE_SWAPPED_CLEARS_OWN;
    }
    return SWAP_RULE_CLASSIC;
}

const char *swap_rule_to_string(swap_rule_t rule) {
    switch (rule) {
        case SWAP_RULE_CLASSIC:
            return "CLASSIC";
        case SWAP_RULE_CLEARS_OWN:
            return "CLEARS_OWN";
        case SWAP_RULE_SWAPPED_CLEARS:
            return "SWAPPED_CLEARS";
        case SWAP_RULE_SWAPPED_CLEARS_OWN:
            return "SWAPPED_CLEARS_OWN";
        default:
            return "UNKNOWN";
    }
}

void apply_puzzle_position(board_t *board, const puzzle_t *puzzle) {
    for (uint8_t row = 0u; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0u; col < BOARD_COLS; ++col) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }

    // Reset move counters for freshly loaded puzzle positions
    board->move_count = 0u;
    board->swapped_count = 0u;
    board->white_swapped_count = 0u;
    board->black_swapped_count = 0u;

    const uint8_t *pieces = puzzle->pieces;
    for (uint8_t index = 0u; index < puzzle->piece_count; ++index) {
        const uint8_t base = (uint8_t)(index * 2u);
        const uint8_t row = pieces[base];
        const uint8_t packed_piece = pieces[base + 1u];

        const uint8_t swapped = PIECE_UNPACK_SWAPPED(packed_piece);
        const uint8_t player = PIECE_UNPACK_PLAYER(packed_piece);
        const uint8_t col = PIECE_UNPACK_COL(packed_piece);

        piece_type_t type;
        if (player == PLAYER_WHITE) {
            type = swapped ? PIECE_WHITE_SWAPPED : PIECE_WHITE_NORMAL;
        } else {
            type = swapped ? PIECE_BLACK_SWAPPED : PIECE_BLACK_NORMAL;
        }

        board_set_piece(board, row, col, type);
    }

}


// Serialize the solved state of all puzzles (1 bit per puzzle, packed into bytes)
// Always fills max_bytes to maintain consistent file size across different puzzle sets.
// Active bits are copied from the internal bitset; remaining bytes are zeroed.
size_t puzzle_catalog_serialize_solved(uint8_t *buffer, size_t max_bytes) {
    puzzle_catalog_ensure_header();
    if (!buffer || !s_solved_bitset_ready || max_bytes == 0u) {
        return 0u;
    }

    const size_t copy_bytes = (s_solved_bitset_bytes <= max_bytes) ? s_solved_bitset_bytes : max_bytes;
    if (copy_bytes > 0u) {
        memcpy(buffer, s_solved_bitset, copy_bytes);
    }
    if (copy_bytes < max_bytes) {
        memset(buffer + copy_bytes, 0, max_bytes - copy_bytes);
    }
    return max_bytes;
}

// Deserialize the solved state of all puzzles from a buffer (1 bit per puzzle, packed into bytes)
uint8_t puzzle_catalog_deserialize_solved(const uint8_t *buffer, size_t length) {
    puzzle_catalog_ensure_header();
    if (!buffer || !s_solved_bitset_ready) {
        return 0u;
    }

    if (s_solved_bitset_bytes == 0u) {
        return 1u;
    }

    if (length < s_solved_bitset_bytes) {
        return 0u;
    }

    memcpy(s_solved_bitset, buffer, s_solved_bitset_bytes);
    if (s_solved_bitset_bytes < sizeof(s_solved_bitset)) {
        memset(s_solved_bitset + s_solved_bitset_bytes, 0, sizeof(s_solved_bitset) - s_solved_bitset_bytes);
    }

    return 1u;
}

sig64_t puzzle_catalog_signature(void) {
    puzzle_catalog_ensure_header();
    return s_puzzle_catalog_signature_value;
}

void puzzle_catalog_clear_solved_state(void) {
    if (!s_solved_bitset_ready) {
        return;
    }

    const size_t bytes = (s_solved_bitset_bytes <= sizeof(s_solved_bitset)) ? s_solved_bitset_bytes : sizeof(s_solved_bitset);
    if (bytes > 0u) {
        memset(s_solved_bitset, 0, bytes);
    }

    if (s_puzzle_cache_valid) {
        s_puzzle_cache.is_solved = false;
    }
}

void puzzle_catalog_invalidate_cache(void) {
    s_header_loaded = false;
    s_puzzle_cache_valid = false;
    s_puzzle_cache_index = 0u;
    memset(&s_puzzle_catalog_signature_value, 0, sizeof(s_puzzle_catalog_signature_value));
    s_solved_bitset_ready = false;
    s_solved_bit_count = 0u;
    s_solved_bitset_bytes = 0u;
    puzzle_catalog_reset_index_cache();
}


