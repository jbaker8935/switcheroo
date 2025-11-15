

#include "../src/puzzle_data.h"
#include "../src/board.h"
#include "../src/text_display.h"
#include "../src/video.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(__llvm_mos__)
#include "../src/platform_f256.h"
#elif defined(AI_AGENT_HOST_TEST)
#include <stdio.h>
#include <stdlib.h>
#endif

enum {
    PUZZLE_ID_BYTES = 32u,
    PUZZLE_MAX_PIECES = 16u,
    PUZZLE_PIECE_BYTES = PUZZLE_MAX_PIECES * 2u,
    PUZZLE_MAX_SOLUTION_MOVES = 9u,
    PUZZLE_SOLUTION_WORDS = PUZZLE_MAX_SOLUTION_MOVES * 2u,
    PUZZLE_SOLUTION_BYTES = PUZZLE_SOLUTION_WORDS * 2u,
    PUZZLE_HEADER_BYTES = 2u,
    PUZZLE_RECORD_BYTES = PUZZLE_ID_BYTES + 2u + 1u + 1u + 1u +
    PUZZLE_PIECE_BYTES + 1u + PUZZLE_SOLUTION_BYTES
};

static char s_puzzle_id_buffer[PUZZLE_ID_BYTES];
static uint8_t s_puzzle_piece_buffer[PUZZLE_PIECE_BYTES];
static uint16_t s_puzzle_solution_buffer[PUZZLE_SOLUTION_WORDS];

static puzzle_t s_puzzle_cache = {
    .id = s_puzzle_id_buffer,
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 1u,
    .is_solved = false,
    .piece_count = 0u,
    .pieces = s_puzzle_piece_buffer,
    .solution_length = 0u,
    .solution = s_puzzle_solution_buffer
};

static uint16_t s_rule_counts[NUMBER_OF_SWAP_RULES] = {0u};

typedef struct {
    swap_rule_t rule;
    uint16_t filtered_index;
    uint16_t actual_index;
} puzzle_index_cache_t;

static puzzle_index_cache_t s_last_index_lookup = {
    .rule = SWAP_RULE_CLASSIC,
    .filtered_index = UINT16_MAX,
    .actual_index = 0u
};

static swap_rule_t s_current_puzzle_swap_rule = SWAP_RULE_CLASSIC;

static puzzle_collection_t s_puzzle_collection = {
    .count = 0u,
    .puzzles = NULL
};

static bool s_header_loaded = false;
static bool s_puzzle_cache_valid = false;
static uint16_t s_puzzle_cache_index = 0u;

static uint8_t s_solved_bitset[PUZZLE_SOLVED_BYTES] = {0u};
static uint16_t s_solved_bit_count = 0u;
static size_t s_solved_bitset_bytes = 0u;
static bool s_solved_bitset_ready = false;

#if defined(AI_AGENT_HOST_TEST)
static uint8_t *s_host_catalog_data = NULL;
static size_t s_host_catalog_size = 0u;
static const char *const s_host_catalog_candidates[] = {
    "assets/generated/puzzle_data.bin",
    "../assets/generated/puzzle_data.bin"
};

static void puzzle_catalog_host_load(void) {
    if (s_host_catalog_data != NULL) {
        return;
    }

    const size_t candidate_count = sizeof(s_host_catalog_candidates) / sizeof(s_host_catalog_candidates[0]);

    for (size_t i = 0u; i < candidate_count; ++i) {
        const char *path = s_host_catalog_candidates[i];
        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            continue;
        }
        
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            continue;
        }
        long size = ftell(file);
        if (size <= 0) {
            fclose(file);
            continue;
        }
        if (fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            continue;
        }
        
        uint8_t *buffer = (uint8_t *)malloc((size_t)size);
        if (buffer == NULL) {
            fclose(file);
            continue;
        }
        
        size_t read = fread(buffer, 1, (size_t)size, file);
        fclose(file);
        if (read != (size_t)size) {
            free(buffer);
            continue;
        }
        
        s_host_catalog_data = buffer;
        s_host_catalog_size = (size_t)size;
        break;
    }
}
#endif

static void puzzle_catalog_init_solved_bits(uint16_t count) {
    s_solved_bit_count = (count <= PUZZLE_SOLVED_CAPACITY) ? count : PUZZLE_SOLVED_CAPACITY;
    s_solved_bitset_bytes = (size_t)((s_solved_bit_count + 7u) / 8u);
    if (s_solved_bitset_bytes > sizeof(s_solved_bitset)) {
        s_solved_bitset_bytes = sizeof(s_solved_bitset);
    }
    memset(s_solved_bitset, 0, sizeof(s_solved_bitset));
    s_solved_bitset_ready = true;
#if defined(__llvm_mos__)
    if (count > PUZZLE_SOLVED_CAPACITY) {
        print_puzzle_debug("PUZ BITSET TRUNC", "INCREASE CAPACITY");
    }
#else
    (void)count;
#endif
}

static inline bool puzzle_catalog_solved_bit_get(uint16_t index) {
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

static inline void puzzle_catalog_solved_bit_set(uint16_t index, bool solved) {
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

static inline uint8_t puzzle_catalog_read_byte(uint32_t offset) {
    #if defined(__llvm_mos__)
    return platform_far_read_byte(SRAM_PUZZLE_CATALOG + offset);
    #elif defined(AI_AGENT_HOST_TEST)
    puzzle_catalog_host_load();
    if (s_host_catalog_data == NULL || offset >= s_host_catalog_size) {
        return 0u;
    }
    return s_host_catalog_data[offset];
    #else
    (void)offset;
    return 0u;
    #endif
}

static inline uint16_t puzzle_catalog_read_word(uint32_t offset) {
    #if defined(__llvm_mos__)
    return platform_far_read_word(SRAM_PUZZLE_CATALOG + offset);
    #elif defined(AI_AGENT_HOST_TEST)
    puzzle_catalog_host_load();
    if (s_host_catalog_data == NULL || (offset + 1u) >= s_host_catalog_size) {
        return 0u;
    }
    return (uint16_t)s_host_catalog_data[offset] |
    ((uint16_t)s_host_catalog_data[offset + 1u] << 8);
    #else
    (void)offset;
    return 0u;
    #endif
}

static inline void puzzle_catalog_reset_index_cache(void) {
    s_last_index_lookup.filtered_index = UINT16_MAX;
    s_last_index_lookup.actual_index = 0u;
    s_last_index_lookup.rule = s_current_puzzle_swap_rule;
}

static swap_rule_t puzzle_catalog_rule_for_index(uint16_t index) {
    const uint32_t record_offset = PUZZLE_HEADER_BYTES + (uint32_t)index * PUZZLE_RECORD_BYTES;
    const uint16_t swap_rule_value = puzzle_catalog_read_word(record_offset + PUZZLE_ID_BYTES);
    return (swap_rule_value <= SWAP_RULE_SWAPPED_CLEARS_OWN) ? (swap_rule_t)swap_rule_value : SWAP_RULE_CLASSIC;
}

static void FAR12_puzzle_catalog_ensure_header(void);

#pragma clang optimize off
__attribute__((noinline))
static void puzzle_catalog_ensure_header(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 12;
    FAR12_puzzle_catalog_ensure_header();
    *(volatile unsigned char *)0x000d = ___mmu;
    return;
}

#pragma clang optimize on

__attribute__((noinline, section(".block12")))

static void FAR12_puzzle_catalog_ensure_header(void) {
    if (s_header_loaded) {
        return;
    }

    const uint16_t count = (uint16_t)puzzle_catalog_read_byte(0u) |
        ((uint16_t)puzzle_catalog_read_byte(1u) << 8);
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

static bool FAR12_puzzle_catalog_map_filtered_index(swap_rule_t rule, uint16_t filtered_index, uint16_t *out_actual_index);

#pragma clang optimize off
__attribute__((noinline))
static bool puzzle_catalog_map_filtered_index(swap_rule_t rule, uint16_t filtered_index, uint16_t *out_actual_index) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 12;
    bool result = FAR12_puzzle_catalog_map_filtered_index(rule, filtered_index, out_actual_index);
    *(volatile unsigned char *)0x000d = ___mmu;
    return result;
}

#pragma clang optimize on

__attribute__((noinline, section(".block12")))

static bool FAR12_puzzle_catalog_map_filtered_index(swap_rule_t rule, uint16_t filtered_index, uint16_t *out_actual_index) {
    FAR12_puzzle_catalog_ensure_header();

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


#if defined(AI_AGENT_HOST_TEST)
    puzzle_catalog_host_load();
    if (s_host_catalog_data != NULL && is_solved_offset < s_host_catalog_size) {
        s_host_catalog_data[is_solved_offset] = 1u;
    }
#endif
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
size_t puzzle_catalog_serialize_solved(uint8_t *buffer, size_t max_bytes) {
    puzzle_catalog_ensure_header();
    if (!buffer || !s_solved_bitset_ready) {
        return 0u;
    }

    const size_t needed_bytes = s_solved_bitset_bytes;
    if (needed_bytes == 0u || max_bytes < needed_bytes) {
        return 0u;
    }

    memcpy(buffer, s_solved_bitset, needed_bytes);
    return needed_bytes;
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


