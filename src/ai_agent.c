/**
 * @file ai_agent.c
 * @brief Specification-compliant heuristic AI agent for F256 Switcharoo.
 */
#include "../src/ai_agent.h"
#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "../src/board.h"
#include "../src/input.h"
#include "../src/text_display.h"

#ifdef AI_AGENT_ENABLE_TIMER
#include <time.h>
#endif

#if defined(__llvm_mos__) && !defined(AI_AGENT_ENABLE_TIMER0_DIAGNOSTICS)
#define AI_AGENT_ENABLE_TIMER0_DIAGNOSTICS
#endif

#if defined(__llvm_mos__)
#define T0_PEND 0xD660
#define T0_MASK 0xD66C

#define T0_CTR \
    0xD650  // master control register for timer0, write.b0=ticks b1=reset b2=set to last value of VAL b3=set count up,
            // clear count down
#define T0_STAT 0xD650  // master control register for timer0, read bit0 set = reached target val

#define CTR_INTEN 0x80  // present only for timer1? or timer0 as well?
#define CTR_ENABLE 0x01
#define CTR_CLEAR 0x02
#define CTR_LOAD 0x04
#define CTR_UPDOWN 0x08

#define T0_VAL_L 0xD651  // current 24 bit value of the timer
#define T0_VAL_M 0xD652
#define T0_VAL_H 0xD653

#define T0_CMP_CTR 0xD654  // b0: t0 returns 0 on reaching target. b1: CMP = last value written to T0_VAL
#define T0_CMP_L 0xD655    // 24 bit target value for comparison
#define T0_CMP_M 0xD656
#define T0_CMP_H 0xD657

#define T0_CMP_CTR_RECLEAR 0x01
#define T0_CMP_CTR_RELOAD 0x02

#endif

#ifdef AI_AGENT_ENABLE_TIMER0_DIAGNOSTICS
extern void print_formatted_text(uint8_t x, uint8_t y, const char *text);

// static void ai_timer0_reset(void) {
//     POKE(T0_CTR, CTR_CLEAR);
//     POKE(T0_CTR, CTR_UPDOWN | CTR_ENABLE);
//     POKE(T0_PEND, 0x10);
//     POKE(T0_CMP_CTR, T0_CMP_CTR_RECLEAR);
//     POKE(T0_CMP_L, 0xFF);
//     POKE(T0_CMP_M, 0xFF);
//     POKE(T0_CMP_H, 0xFF);
// }

// static uint32_t ai_timer0_read(void) {
//     uint32_t value_h = (uint32_t)PEEK(T0_VAL_H) << 16;
//     uint32_t value_m = (uint32_t)PEEK(T0_VAL_M) << 8;
//     uint32_t value_l = (uint32_t)PEEK(T0_VAL_L);
//     return value_h | value_m | value_l;
// }

// static void ai_format_diag(char *dest, const char *label, uint32_t value) {
//     uint8_t i = 0;
//     while (label[i] != '\0' && i < 23) {
//         dest[i] = label[i];
//         ++i;
//     }
//     if (i < 23) {
//         dest[i++] = ' ';
//     }
//     if (value == 0u) {
//         dest[i++] = '0';
//     } else {
//         char digits[10];
//         uint8_t count = 0;
//         while (value != 0u && count < sizeof(digits)) {
//             digits[count++] = (char)('0' + (value % 10u));
//             value /= 10u;
//         }
//         while (count > 0 && i < 25) {
//             dest[i++] = digits[--count];
//         }
//     }
//     dest[i] = '\0';
// }

// static void ai_print_diagnostics(uint32_t nodes, uint32_t ticks, bool enabled) {
//     if (!enabled) {
//         print_formatted_text(1, 45, "");
//         print_formatted_text(1, 46, "");
//         return;
//     }

//     char buf_nodes[26];
//     char buf_ticks[26];
//     ai_format_diag(buf_nodes, "AI NODES:", nodes);
//     ai_format_diag(buf_ticks, "AI TICKS:", ticks);
//     print_formatted_text(1, 45, buf_nodes);
//     print_formatted_text(1, 46, buf_ticks);
// }
#else
// static void ai_timer0_reset(void) {}

// static uint32_t ai_timer0_read(void) {
//     return 0u;
// }

// static void ai_print_diagnostics(uint32_t nodes, uint32_t ticks, bool enabled) {
//     (void)nodes;
//     (void)ticks;
//     (void)enabled;
// }
#endif

#define AI_SCORE_WIN 30000
#define AI_SCORE_LOSS (-AI_SCORE_WIN)
#define AI_SCORE_MAX 32000

#ifdef AI_AGENT_HOST_TEST

static uint16_t s_ai_rng_state = 0xC0FEu;

#endif

static void ai_random_seed_internal(uint16_t seed) {
    if (seed == 0u) {
        seed = 1u;
    }
#ifndef AI_AGENT_HOST_TEST
    randomSeed(seed);
#else
    s_ai_rng_state = seed;
#endif
}

void ai_agent_set_random_seed(uint16_t seed) {
    ai_random_seed_internal(seed);
}

static uint16_t ai_random_next(void) {
#ifndef AI_AGENT_HOST_TEST
    return randomRead();
#else
    s_ai_rng_state = (uint16_t)(s_ai_rng_state * 1664525u + 1013904223u);
    return s_ai_rng_state;
#endif
}

static uint16_t ai_random_range(uint16_t limit) {
    if (limit == 0u) {
        return 0u;
    }
    return ai_random_next() % limit;
}

static bool ai_random_chance(uint8_t percentage) {
    if (percentage == 0u) {
        return false;
    }
    if (percentage >= 100u) {
        return true;
    }
    return (ai_random_next() % 100u) < percentage;
}

ai_blunder_type_t ai_allowed_blunder_type(ai_difficulty_t difficulty);

static inline int32_t ai_signed_multiply(int16_t lhs, int16_t rhs) {
#if defined(__llvm_mos__)
    return mathSignedMultiply(lhs, rhs);
#else
    return (int32_t)lhs * (int32_t)rhs;
#endif
}

static const int8_t kAdjRow[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
static const int8_t kAdjCol[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const uint8_t kPieceOwnerLUT[7] = {PLAYER_NONE, PLAYER_WHITE, PLAYER_BLACK, PLAYER_NONE,
                                          PLAYER_NONE, PLAYER_WHITE, PLAYER_BLACK};

typedef struct {
    move_t move;
    int16_t order_score;
} ai_ordered_move_t;

typedef struct {
    uint8_t row_mask;
    uint8_t row_links;
    uint8_t branching_nodes;
} ai_connection_metrics_t;

static ai_eval_breakdown_t s_last_breakdown;

// Global progress callback variables
static ai_progress_callback_t s_progress_callback = NULL;
static void *s_progress_user_data = NULL;

static const ai_eval_weights_t kRuleWeights[4] = {
    {88, 58, 36, 44, 12},  // Classic
    {84, 54, 32, 44, 12},  // Clears Own
    {72, 52, 50, 38, 16},  // Swapped Clears
    {72, 50, 46, 38, 16}   // Swapped Clears Own
};

static void ai_board_copy(board_t *dest, const board_t *src) {
#pragma unroll 8
    for (uint8_t i = 0; i < sizeof(board_t); ++i) {
        ((uint8_t *)dest)[i] = ((const uint8_t *)src)[i];
    }
}

static const uint8_t kNibblePopcount[16] = {0u, 1u, 1u, 2u, 1u, 2u, 2u, 3u, 1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u};

static uint8_t ai_popcount(uint8_t value) {
    return (uint8_t)(kNibblePopcount[value & 0x0Fu] + kNibblePopcount[(value >> 4) & 0x0Fu]);
}

static uint8_t ai_find_root(uint8_t *parent, uint8_t x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

static void ai_union_cells(uint8_t *parent, uint8_t a, uint8_t b) {
    uint8_t root_a = ai_find_root(parent, a);
    uint8_t root_b = ai_find_root(parent, b);
    if (root_a != root_b) {
        parent[root_a] = root_b;
    }
}

static uint8_t ai_count_swapped_for_player(const board_t *board, player_t player) {
    if (!board) {
        return 0u;
    }
    if (player == PLAYER_WHITE) {
        return board->white_swapped_count;
    }
    if (player == PLAYER_BLACK) {
        return board->black_swapped_count;
    }
    return 0u;
}

static bool ai_board_has_swapped_for_player(const board_t *board, player_t player) {
    return ai_count_swapped_for_player(board, player) > 0u;
}

static bool ai_board_has_any_swapped(const board_t *board) {
    return board && board->swapped_count > 0u;
}

static bool ai_move_clears_swapped(const board_t *board, const move_t *move, swap_rule_t rule) {
    if (!move || move->type != MOVE_TYPE_EMPTY) {
        return false;
    }

    piece_type_t moving_piece = board_get_piece_unchecked(board, move->from_row, move->from_col);
    player_t mover = board_get_piece_owner(moving_piece);
    bool mover_swapped = board_is_piece_swapped(moving_piece);

    switch (rule) {
        case SWAP_RULE_CLASSIC:
            return ai_board_has_any_swapped(board);
        case SWAP_RULE_CLEARS_OWN:
            return ai_board_has_swapped_for_player(board, mover);
        case SWAP_RULE_SWAPPED_CLEARS:
            return mover_swapped && ai_board_has_any_swapped(board);
        case SWAP_RULE_SWAPPED_CLEARS_OWN:
            return mover_swapped && ai_board_has_swapped_for_player(board, mover);
        default:
            return ai_board_has_any_swapped(board);
    }
}

void FAR9_ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out);

#if defined(AI_AGENT_HOST_TEST)

void ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out) {
    FAR9_ai_compute_connection_metrics(board, player, out);
}

#else

#pragma clang optimize off
__attribute__((noinline))

void ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    FAR9_ai_compute_connection_metrics(board, player, out);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) void FAR9_ai_compute_connection_metrics(const board_t *board,
                                                                                      player_t player,
                                                                                      ai_connection_metrics_t *out) {
    uint8_t parent[BOARD_CELLS];
    uint8_t row_mask[BOARD_CELLS];
    uint8_t branching[BOARD_CELLS];
    bool owned[BOARD_CELLS];

    for (uint8_t idx = 0; idx < BOARD_CELLS; ++idx) {
        parent[idx] = idx;
        row_mask[idx] = 0;
        branching[idx] = 0;
        owned[idx] = false;
    }

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            uint8_t idx = (uint8_t)(row * BOARD_COLS + col);
            owned[idx] = (board_get_piece_owner(piece) == player);
        }
    }

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            uint8_t idx = (uint8_t)(row * BOARD_COLS + col);
            if (!owned[idx]) {
                continue;
            }
            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kAdjRow[dir];
                int8_t new_col = (int8_t)col + kAdjCol[dir];
                if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                    uint8_t adj_idx = (uint8_t)(new_row * BOARD_COLS + new_col);
                    if (owned[adj_idx]) {
                        ai_union_cells(parent, idx, adj_idx);
                    }
                }
            }
        }
    }

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            uint8_t idx = (uint8_t)(row * BOARD_COLS + col);
            if (!owned[idx]) {
                continue;
            }
            uint8_t root = ai_find_root(parent, idx);
            if (row >= WIN_START_ROW && row <= WIN_END_ROW) {
                uint8_t bit = (uint8_t)(row - WIN_START_ROW);
                row_mask[root] |= (uint8_t)(1u << bit);
            }

            uint8_t friendly_neighbors = 0;
            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kAdjRow[dir];
                int8_t new_col = (int8_t)col + kAdjCol[dir];
                if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                    piece_type_t neighbor = board_get_piece_unchecked(board, (uint8_t)new_row, (uint8_t)new_col);
                    if (board_get_piece_owner(neighbor) == player) {
                        friendly_neighbors++;
                    }
                }
            }
            if (friendly_neighbors >= 3 && branching[root] < 12) {
                branching[root]++;
            }
        }
    }

    out->row_mask = 0;
    out->row_links = 0;
    out->branching_nodes = 0;

    bool counted[BOARD_CELLS];
    memset(counted, 0, sizeof(counted));

    for (uint8_t idx = 0; idx < BOARD_CELLS; ++idx) {
        if (!owned[idx]) {
            continue;
        }
        uint8_t root = ai_find_root(parent, idx);
        if (counted[root]) {
            continue;
        }
        counted[root] = true;
        uint8_t mask = row_mask[root];
        out->row_mask |= mask;
        for (uint8_t bit = 0; bit + 1 < (WIN_END_ROW - WIN_START_ROW + 1); ++bit) {
            if ((mask & (1u << bit)) && (mask & (1u << (bit + 1)))) {
                out->row_links++;
            }
        }
        out->branching_nodes = (uint8_t)(out->branching_nodes + branching[root]);
    }
}

uint8_t FAR9_ai_count_bridge_potential(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_count_bridge_potential(const board_t *board, player_t player) {
    return FAR9_ai_count_bridge_potential(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t ai_count_bridge_potential(const board_t *board, player_t player) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_count_bridge_potential(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) uint8_t FAR9_ai_count_bridge_potential(const board_t *board,
                                                                                     player_t player) {
    uint8_t bridges = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            if (board_get_piece_unchecked(board, row, col) != PIECE_NONE) {
                continue;
            }
            uint8_t friendly = 0;
            uint8_t directional_pairs = 0;
            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kAdjRow[dir];
                int8_t new_col = (int8_t)col + kAdjCol[dir];
                if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                    piece_type_t neighbor = board_get_piece_unchecked(board, (uint8_t)new_row, (uint8_t)new_col);
                    if (board_get_piece_owner(neighbor) == player) {
                        friendly++;
                        if (dir % 2 == 1) {
                            directional_pairs++;
                        }
                    }
                }
            }
            if (friendly >= 2) {
                bridges++;
            }
            if (directional_pairs >= 2) {
                bridges++;
            }
        }
    }
    return bridges;
}

uint8_t FAR9_ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    return FAR9_ai_measure_swap_pressure(board, player, rule);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_measure_swap_pressure(board, player, rule);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9")))

uint8_t FAR9_ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    (void)rule;
    int16_t pressure = 0;
    piece_type_t swapped = (player == PLAYER_WHITE) ? PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
    piece_type_t friendly_normal = (player == PLAYER_WHITE) ? PIECE_WHITE_NORMAL : PIECE_BLACK_NORMAL;
    piece_type_t opponent_normal = (player == PLAYER_WHITE) ? PIECE_BLACK_NORMAL : PIECE_WHITE_NORMAL;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (piece == swapped) {
                pressure += 3;
                bool has_support = false;
                for (uint8_t dir = 0; dir < 8; ++dir) {
                    int8_t new_row = (int8_t)row + kAdjRow[dir];
                    int8_t new_col = (int8_t)col + kAdjCol[dir];
                    if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                        piece_type_t neighbor = board_get_piece_unchecked(board, (uint8_t)new_row, (uint8_t)new_col);
                        player_t owner = board_get_piece_owner(neighbor);
                        if (owner == player) {
                            has_support = true;
                        } else if (neighbor == opponent_normal) {
                            pressure += 2;
                        }
                    }
                }
                if (!has_support) {
                    pressure -= 2;
                }
            } else if (piece == friendly_normal) {
                for (uint8_t dir = 0; dir < 8; ++dir) {
                    int8_t new_row = (int8_t)row + kAdjRow[dir];
                    int8_t new_col = (int8_t)col + kAdjCol[dir];
                    if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                        piece_type_t neighbor = board_get_piece_unchecked(board, (uint8_t)new_row, (uint8_t)new_col);
                        if (neighbor == opponent_normal) {
                            pressure += 1;
                        }
                    }
                }
            }
        }
    }

    if (pressure < 0) {
        pressure = 0;
    }
    if (pressure > 255) {
        pressure = 255;
    }
    return (uint8_t)pressure;
}

uint8_t FAR9_ai_measure_blocking(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_measure_blocking(const board_t *board, player_t player) {
    return FAR9_ai_measure_blocking(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t ai_measure_blocking(const board_t *board, player_t player) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_measure_blocking(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9")))

uint8_t FAR9_ai_measure_blocking(const board_t *board, player_t player) {
    player_t opponent = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    uint8_t blocking = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            player_t owner = board_get_piece_owner(piece);
            if (owner != player) {
                continue;
            }

            if (row >= WIN_START_ROW && row <= WIN_END_ROW && (col == 1 || col == 2)) {
                blocking += 2;
            }

            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kAdjRow[dir];
                int8_t new_col = (int8_t)col + kAdjCol[dir];
                if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                    piece_type_t neighbor = board_get_piece_unchecked(board, (uint8_t)new_row, (uint8_t)new_col);
                    if (board_get_piece_owner(neighbor) == opponent) {
                        blocking++;
                    }
                }
            }
        }
    }
    return blocking;
}

uint16_t FAR9_ai_measure_mobility(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint16_t ai_measure_mobility(const board_t *board, player_t player) {
    return FAR9_ai_measure_mobility(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint16_t ai_measure_mobility(const board_t *board, player_t player) {
    uint16_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_measure_mobility(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9")))

uint16_t FAR9_ai_measure_mobility(const board_t *board, player_t player) {
    board_t scratch;
    ai_board_copy(&scratch, board);

    uint16_t mobility = 0;
    move_array_t soa_moves;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            mobility += board_get_legal_moves_soa(&scratch, player, row, col, &soa_moves);
        }
    }

    return mobility;
}

static int16_t ai_clamp_score(int32_t value) {
    if (value > AI_SCORE_MAX) {
        return AI_SCORE_MAX;
    }
    if (value < -AI_SCORE_MAX) {
        return -AI_SCORE_MAX;
    }
    return (int16_t)value;
}

bool ai_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule) {
    // Early guard: if player occupies fewer than 5 winning rows, no immediate win possible
    uint8_t occupied_rows = 0;
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                occupied_rows++;
                break;
            }
        }
    }
    if (occupied_rows < 5) {
        return false;
    }

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(board, player, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t candidate;
                move_array_get_move(&soa_moves, i, &candidate);

                board_t test;
                ai_board_copy(&test, board);
                board_context_t test_context = {.current_player = player};
                if (!board_execute_move_without_history(&test, &test_context, &candidate, rule)) {
                    continue;
                }

                if (board_check_win_fast(&test, player)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// Determine whether executing `move` guarantees the AI an immediate win on its
// following turn assuming the opponent plays optimally.
bool ai_board_creates_forced_immediate_win_postmove(const board_t *after_ai, player_t mover, player_t ai_player,
                                                           swap_rule_t rule) {
    if (!after_ai) {
        return false;
    }

    board_context_t opponent_context = {.current_player = mover};
    board_switch_turn(&opponent_context);
    player_t opponent = opponent_context.current_player;

    bool opponent_has_moves = false;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(after_ai, row, col);
            if (board_get_piece_owner(piece) != opponent) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(after_ai, opponent, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t response_move;
                move_array_get_move(&soa_moves, i, &response_move);
                opponent_has_moves = true;

                board_t after_opponent;
                ai_board_copy(&after_opponent, after_ai);
                board_context_t response_context = {.current_player = opponent};
                if (!board_execute_move_without_history(&after_opponent, &response_context, &response_move, rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_opponent, opponent)) {
                    return false;
                }

                board_switch_turn(&response_context);

                if (!ai_immediate_win_available(&after_opponent, ai_player, rule)) {
                    return false;
                }
            }
        }
    }

    if (!opponent_has_moves) {
        return true;
    }

    return true;
}

static bool ai_all_replies_allow_opponent_immediate_win_from_ordered(const ai_ordered_moves_t *ordered,
                                                                     uint8_t generated);
#if defined(AI_AGENT_HOST_TEST)
static bool ai_all_replies_allow_opponent_immediate_win(const board_t *board, player_t current_player,
                                                        const ai_config_t *config);

static bool ai_move_creates_forced_immediate_win(const board_t *board, player_t current_player, const move_t *move,
                                                 const ai_config_t *config, player_t ai_player) {
    if (!config || !move) {
        return false;
    }

    board_t after_ai;
    ai_board_copy(&after_ai, board);
    board_context_t dummy_context = {.current_player = current_player};
    if (!board_execute_move_without_history(&after_ai, &dummy_context, move, config->swap_rule)) {
        return false;
    }

    if (board_check_win_fast(&after_ai, ai_player)) {
        return true;
    }

    return ai_board_creates_forced_immediate_win_postmove(&after_ai, current_player, ai_player, config->swap_rule);
}

static bool ai_move_allows_opponent_immediate_win(const board_t *board, player_t current_player, const move_t *move,
                                                  const ai_config_t *config, player_t ai_player) {
    if (!config || !move) {
        return false;
    }

    board_t after_ai;
    ai_board_copy(&after_ai, board);
    board_context_t move_context = {.current_player = current_player};
    if (!board_execute_move_without_history(&after_ai, &move_context, move, config->swap_rule)) {
        return false;
    }

    player_t opponent = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    board_context_t after_context = {.current_player = current_player};
    board_switch_turn(&after_context);

    {
        return ai_immediate_win_available(&after_ai, opponent, config->swap_rule);
    }
}

#endif

#if defined(AI_AGENT_HOST_TEST)
bool ai_agent_detect_unavoidable_loss(const board_t *board, player_t current_player, const ai_config_t *config) {
    return ai_all_replies_allow_opponent_immediate_win(board, current_player, config);
}

bool ai_agent_move_creates_forced_immediate_win(const board_t *board, player_t current_player, const move_t *move,
                                                const ai_config_t *config, player_t ai_player) {
    return ai_move_creates_forced_immediate_win(board, current_player, move, config, ai_player);
}

bool ai_agent_move_allows_opponent_immediate_win(const board_t *board, player_t current_player, const move_t *move,
                                                 const ai_config_t *config, player_t ai_player) {
    return ai_move_allows_opponent_immediate_win(board, current_player, move, config, ai_player);
}
#endif

bool FAR9_ai_forcing_move_available(const board_t *board, player_t current_player, player_t player, swap_rule_t rule);

#if defined(AI_AGENT_HOST_TEST)

bool ai_forcing_move_available(const board_t *board, player_t current_player, player_t player, swap_rule_t rule) {
    return FAR9_ai_forcing_move_available(board, current_player, player, rule);
}

#else

#pragma clang optimize off
__attribute__((noinline)) bool ai_forcing_move_available(const board_t *board, player_t current_player, player_t player,
                                                         swap_rule_t rule) {
    bool return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_forcing_move_available(board, current_player, player, rule);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) bool FAR9_ai_forcing_move_available(const board_t *board,
                                                                                  player_t current_player,
                                                                                  player_t player, swap_rule_t rule) {
    if (!board || player == PLAYER_NONE) {
        return false;
    }

    player_t ai_player = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    board_t base_state;
    ai_board_copy(&base_state, board);

    move_t candidate_moves[AI_MAX_ORDERED_MOVES];
    move_t response_moves[AI_MAX_ORDERED_MOVES];
    const uint8_t buffer_capacity = (uint8_t)(sizeof(candidate_moves) / sizeof(candidate_moves[0]));

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&base_state, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            uint8_t move_count = board_get_legal_moves(&base_state, player, row, col, candidate_moves, buffer_capacity);
            for (uint8_t move_idx = 0; move_idx < move_count; ++move_idx) {
                if (candidate_moves[move_idx].type != MOVE_TYPE_SWAP) {
                    continue;
                }

                board_t after_opponent;
                ai_board_copy(&after_opponent, &base_state);
                board_context_t move_ctx = {.current_player = player};
                if (!board_execute_move_without_history(&after_opponent, &move_ctx, &candidate_moves[move_idx], rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_opponent, player)) {
                    return true;
                }

                board_switch_turn(&move_ctx);
                /* Defender (ai_player) now to move */
                bool defender_has_safe_move = false;

                for (uint8_t resp_row = 0; resp_row < BOARD_ROWS && !defender_has_safe_move; ++resp_row) {
                    for (uint8_t resp_col = 0; resp_col < BOARD_COLS && !defender_has_safe_move; ++resp_col) {
                        piece_type_t resp_piece = board_get_piece_unchecked(&after_opponent, resp_row, resp_col);
                        if (board_get_piece_owner(resp_piece) != ai_player) {
                            continue;
                        }

                        uint8_t response_count = board_get_legal_moves(&after_opponent, ai_player, resp_row, resp_col,
                                                                       response_moves, buffer_capacity);
                        for (uint8_t resp_idx = 0; resp_idx < response_count; ++resp_idx) {
                            board_t after_ai;
                            ai_board_copy(&after_ai, &after_opponent);
                            board_context_t resp_ctx = {.current_player = ai_player};
                            if (!board_execute_move_without_history(&after_ai, &resp_ctx, &response_moves[resp_idx],
                                                                    rule)) {
                                continue;
                            }

                            if (board_check_win_fast(&after_ai, player)) {
                                /* Defender's reply gave player an immediate win; try other replies */
                                continue;
                            }

                            board_switch_turn(&resp_ctx);
                            resp_ctx.current_player = player;

                            if (!ai_immediate_win_available(&after_ai, player, rule)) {
                                defender_has_safe_move = true;
                                break;
                            }
                        }
                    }
                }

                if (!defender_has_safe_move) {
                    return true;
                }
            }
        }
    }
    return false;
}
uint8_t FAR10_ai_generate_moves(const board_t *board, player_t current_player, const ai_config_t *config,
                                ai_ordered_moves_t *out_moves);
static bool ai_all_replies_allow_opponent_immediate_win_from_ordered(const ai_ordered_moves_t *ordered,
                                                                     uint8_t generated) {
    if (!ordered || generated == 0u) {
        return false;
    }

    for (uint8_t i = 0u; i < generated && i < AI_MAX_ORDERED_MOVES; ++i) {
        uint8_t slot = ordered->indices[i];
        if (slot >= AI_MAX_ORDERED_MOVES) {
            continue;
        }

        uint8_t flags = ordered->flags[slot];
        if ((flags & AI_ORDER_FLAG_SELF_IMMEDIATE) != 0u) {
            return false;
        }
        if ((flags & AI_ORDER_FLAG_OPPONENT_IMMEDIATE) == 0u) {
            return false;
        }
    }

    return true;
}

#if defined(AI_AGENT_HOST_TEST)
static bool ai_all_replies_allow_opponent_immediate_win(const board_t *board, player_t current_player,
                                                        const ai_config_t *config) {
    if (!board || !config) {
        return false;
    }

    ai_ordered_moves_t ordered;
    uint8_t generated = FAR10_ai_generate_moves(board, current_player, config, &ordered);
    return ai_all_replies_allow_opponent_immediate_win_from_ordered(&ordered, generated);
}
#endif

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_generate_moves(const board_t *board, player_t current_player, const ai_config_t *config,
                          ai_ordered_moves_t *out_moves) {
    return FAR10_ai_generate_moves(board, current_player, config, out_moves);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t ai_generate_moves(const board_t *board, player_t current_player, const ai_config_t *config,
                          ai_ordered_moves_t *out_moves) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 10;
    return_value = FAR10_ai_generate_moves(board, current_player, config, out_moves);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block10"))) uint8_t FAR10_ai_generate_moves(const board_t *board,
                                                                               player_t current_player,
                                                                               const ai_config_t *config,
                                                                               ai_ordered_moves_t *out_moves) {
    if (!board || !config || !out_moves) {
        return 0u;
    }

    uint8_t count = 0;
    const player_t opponent = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    swap_rule_t swap_rule = config->swap_rule;
    bool immediate_found = false;
    move_array_t soa_moves;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t moving_piece = board->cells[row][col];
            if ((player_t)kPieceOwnerLUT[moving_piece] != current_player) {
                continue;
            }

            bool mover_swapped = board_is_piece_swapped(moving_piece);
            uint8_t num_moves = board_get_legal_moves_soa(board, current_player, row, col, &soa_moves);

            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t move;
                move.from_row = row;
                move.from_col = col;
                move.to_row = move_array_get_to_row(&soa_moves, i);
                move.to_col = move_array_get_to_col(&soa_moves, i);
                move.type = move_array_get_type(&soa_moves, i);
                move.player = current_player;

                int16_t score = 0;
                uint8_t move_flags = 0u;

                if (move.type == MOVE_TYPE_SWAP) {
                    score += 900;
                    score += mover_swapped ? 220 : 500;
                    if (move.to_col == 1 || move.to_col == 2) {
                        score += 150;
                    }
                } else {
                    bool clears_swapped = ai_move_clears_swapped(board, &move, swap_rule);
                    if (clears_swapped) {
                        score -= 1400;
                    } else if (mover_swapped) {
                        score += 220;
                    }
                }

                if (move.to_col == 1 || move.to_col == 2) {
                    score += 350;
                }

                if (current_player == PLAYER_WHITE) {
                    if (move.to_row < move.from_row) {
                        score += 280;
                    }
                } else {
                    if (move.to_row > move.from_row) {
                        score += 280;
                    }
                }

                if (move.type == MOVE_TYPE_EMPTY && move.to_row >= WIN_START_ROW && move.to_row <= WIN_END_ROW) {
                    score += 120;
                }

                // Apply move once for immediate outcome analysis
                board_t after_move;
                ai_board_copy(&after_move, board);
                board_context_t move_context = {.current_player = current_player};
                if (!board_execute_move_without_history(&after_move, &move_context, &move, swap_rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_move, opponent)) {
                    continue;
                }

                bool self_wins_now = board_check_win_fast(&after_move, current_player);
                if (self_wins_now) {
                    score += 25000;  // Favor immediate wins and stop enumeration

                    out_moves->moves.from_rows[0] = move.from_row;
                    out_moves->moves.from_cols[0] = move.from_col;
                    out_moves->moves.to_rows[0] = move.to_row;
                    out_moves->moves.to_cols[0] = move.to_col;
                    out_moves->moves.types[0] = move.type;
                    out_moves->moves.players[0] = move.player;
                    out_moves->order_scores[0] = score;
                    out_moves->flags[0] = AI_ORDER_FLAG_SELF_IMMEDIATE;
                    out_moves->indices[0] = 0u;
                    count = 1;
                    immediate_found = true;
                    goto finalize_ordering;
                }

                board_context_t opponent_context = {.current_player = current_player};
                board_switch_turn(&opponent_context);

                bool allows_opponent_win =
                    ai_immediate_win_available(&after_move, opponent, swap_rule);

                if (allows_opponent_win) {
                    score -= 12000;
                    move_flags |= AI_ORDER_FLAG_OPPONENT_IMMEDIATE;
                }

                uint8_t slot_index = 0xFFu;  // Invalid index
                if (count < AI_MAX_ORDERED_MOVES) {
                    slot_index = count++;
                } else {
                    /* We already filled the output buffer; find the current
                       minimum-scoring slot within the valid range and replace
                       it only if this move is better. Use AI_MAX_ORDERED_MOVES
                       as the bound to avoid reading beyond the array. */
                    uint8_t min_index = 0u;
                    int16_t min_score = out_moves->order_scores[0];
                    for (uint8_t j = 1u; j < AI_MAX_ORDERED_MOVES; ++j) {
                        if (out_moves->order_scores[j] < min_score) {
                            min_score = out_moves->order_scores[j];
                            min_index = j;
                        }
                    }
                    if (score > min_score) {
                        slot_index = min_index;
                    }
                }

                if (slot_index == 0xFFu) {
                    continue;
                }

                out_moves->flags[slot_index] = move_flags;
                out_moves->moves.from_rows[slot_index] = move.from_row;
                out_moves->moves.from_cols[slot_index] = move.from_col;
                out_moves->moves.to_rows[slot_index] = move.to_row;
                out_moves->moves.to_cols[slot_index] = move.to_col;
                out_moves->moves.types[slot_index] = move.type;
                out_moves->moves.players[slot_index] = move.player;
                out_moves->order_scores[slot_index] = score;
            }
        }
    }

finalize_ordering:
    if (count == 0u) {
        return 0u;
    }

    if (immediate_found) {
        out_moves->indices[0] = 0u;
        return count;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        out_moves->indices[i] = i;
    }

    for (uint8_t i = 1u; i < count; ++i) {
        uint8_t key = out_moves->indices[i];
        int16_t key_score = out_moves->order_scores[key];
        uint8_t j = i;
        while (j > 0u && out_moves->order_scores[out_moves->indices[j - 1u]] < key_score) {
            out_moves->indices[j] = out_moves->indices[j - 1u];
            --j;
        }
        out_moves->indices[j] = key;
    }

    return count;
}

int16_t FAR9_ai_agent_evaluate_internal(const board_t *board, player_t current_player, player_t perspective,
                                        const ai_config_t *config, ai_eval_breakdown_t *breakdown);

#if defined(AI_AGENT_HOST_TEST)

int16_t ai_agent_evaluate_internal(const board_t *board, player_t current_player, player_t perspective,
                                   const ai_config_t *config, ai_eval_breakdown_t *breakdown) {
    return FAR9_ai_agent_evaluate_internal(board, current_player, perspective, config, breakdown);
}

#else

#pragma clang optimize off
__attribute__((noinline))

int16_t ai_agent_evaluate_internal(const board_t *board, player_t current_player, player_t perspective,
                                   const ai_config_t *config, ai_eval_breakdown_t *breakdown) {
    int16_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_agent_evaluate_internal(board, current_player, perspective, config, breakdown);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) int16_t FAR9_ai_agent_evaluate_internal(const board_t *board,
                                                                                      player_t current_player,
                                                                                      player_t perspective,
                                                                                      const ai_config_t *config,
                                                                                      ai_eval_breakdown_t *breakdown) {
    player_t opponent = (perspective == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;



    if (board_check_win_fast(board, perspective)) {
        return AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
    }
    if (board_check_win_fast(board, opponent)) {
        return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
    }

    // Check for forcing moves if enabled
    {
    if (ai_immediate_win_available(board, current_player, config->swap_rule)) {
            player_t winner = current_player;
            if (winner == perspective) {
                return AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
            } else {
                return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
            }
        }
    }

    // Check for forcing moves if enabled
    {
        if (config->enable_forcing_check &&
            FAR9_ai_forcing_move_available(board, current_player, opponent, config->swap_rule)) {
            // Opponent has a forcing move, very bad for us
            return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF) + 1000;  // Heavy penalty
        }
    }

    ai_connection_metrics_t conn_me;
    ai_connection_metrics_t conn_op;
    FAR9_ai_compute_connection_metrics(board, perspective, &conn_me);
    FAR9_ai_compute_connection_metrics(board, opponent, &conn_op);

    int16_t connection_me =
        (int16_t)(ai_popcount(conn_me.row_mask) * 12 + conn_me.row_links * 18 + conn_me.branching_nodes * 5);
    int16_t connection_op =
        (int16_t)(ai_popcount(conn_op.row_mask) * 12 + conn_op.row_links * 18 + conn_op.branching_nodes * 5);

    int16_t bridge_me = (int16_t)FAR9_ai_count_bridge_potential(board, perspective);
    int16_t bridge_op = (int16_t)FAR9_ai_count_bridge_potential(board, opponent);

    int16_t swap_me = (int16_t)FAR9_ai_measure_swap_pressure(board, perspective, config->swap_rule);
    int16_t swap_op = (int16_t)FAR9_ai_measure_swap_pressure(board, opponent, config->swap_rule);

    int16_t block_me = (int16_t)FAR9_ai_measure_blocking(board, perspective);
    int16_t block_op = (int16_t)FAR9_ai_measure_blocking(board, opponent);

    int16_t mobility_me = (int16_t)FAR9_ai_measure_mobility(board, perspective);
    int16_t mobility_op = (int16_t)FAR9_ai_measure_mobility(board, opponent);

    int32_t total = 0;
    int16_t diff_conn = (int16_t)(connection_me - connection_op);
    int16_t diff_bridge = (int16_t)(bridge_me - bridge_op);
    int16_t diff_swap = (int16_t)(swap_me - swap_op);
    int16_t diff_block = (int16_t)(block_me - block_op);
    int16_t diff_mobility = (int16_t)(mobility_me - mobility_op);

    int16_t contrib_conn = ai_clamp_score(ai_signed_multiply(config->weights.connection_progress, diff_conn));
    int16_t contrib_bridge = ai_clamp_score(ai_signed_multiply(config->weights.bridge_potential, diff_bridge));
    int16_t contrib_swap = ai_clamp_score(ai_signed_multiply(config->weights.swap_pressure, diff_swap));
    int16_t contrib_block = ai_clamp_score(ai_signed_multiply(config->weights.blocking_coverage, diff_block));
    int16_t contrib_mobility = ai_clamp_score(ai_signed_multiply(config->weights.mobility, diff_mobility));

    total += contrib_conn;
    total += contrib_bridge;
    total += contrib_swap;
    total += contrib_block;
    total += contrib_mobility;

    if (breakdown) {
        breakdown->connection_progress = contrib_conn;
        breakdown->bridge_potential = contrib_bridge;
        breakdown->swap_pressure = contrib_swap;
        breakdown->blocking_coverage = contrib_block;
        breakdown->mobility = contrib_mobility;
        breakdown->total = ai_clamp_score(total);
    }

    return ai_clamp_score(total);
}

void FAR8_ai_agent_init(ai_config_t *config, swap_rule_t swap_rule, ai_difficulty_t difficulty, player_t ai_player);

#if !defined(AI_AGENT_HOST_TEST)

#pragma clang optimize off
__attribute__((noinline)) void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule, ai_difficulty_t difficulty,
                                             player_t ai_player) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_ai_agent_init(config, swap_rule, difficulty, ai_player);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

#else

void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule, ai_difficulty_t difficulty, player_t ai_player) {
    FAR8_ai_agent_init(config, swap_rule, difficulty, ai_player);
}

#endif

__attribute__((noinline, section(".block8"))) void FAR8_ai_agent_init(ai_config_t *config, swap_rule_t swap_rule,
                                                                      ai_difficulty_t difficulty, player_t ai_player) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->swap_rule = swap_rule;
    config->difficulty = difficulty;
    config->ai_player = ai_player;
    config->weights = kRuleWeights[swap_rule % 4];
    config->diagnostics_enabled = false;
    config->enable_forcing_check = (difficulty >= AI_DIFFICULTY_STANDARD);
    config->use_hint_profile = false;
    config->random_top_k = 1u;
    config->random_epsilon_pct = 0u;
    config->blunder_enabled = false;
    config->blunder_chance_pct = 0u;
    config->blunder_type = ai_allowed_blunder_type(difficulty);

    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            config->enable_forcing_check = false;
            ai_agent_config_set_randomization(config, 3u, 20u);
            config->blunder_chance_pct = 20u;
            config->blunder_enabled = true;
            config->blunder_type = AI_BLUNDER_ALLOW_IMMEDIATE_WIN;
            break;
        case AI_DIFFICULTY_EASY:
            config->enable_forcing_check = false;
            ai_agent_config_set_randomization(config, 3u, 10u);
            config->blunder_chance_pct = 15u;
            config->blunder_enabled = true;
            config->blunder_type = AI_BLUNDER_ALLOW_IMMEDIATE_WIN;
            break;
        case AI_DIFFICULTY_STANDARD:
            ai_agent_config_set_randomization(config, 2u, 5u);
            config->blunder_chance_pct = 10u;
            config->blunder_enabled = true;
            config->blunder_type = AI_BLUNDER_ALLOW_FORCING_MOVE;
            break;
        case AI_DIFFICULTY_EXPERT:
        default:
            ai_agent_config_set_randomization(config, 1u, 0u);
            config->blunder_chance_pct = 0u;
            config->blunder_enabled = false;
            config->blunder_type = AI_BLUNDER_NONE;
            break;
    }

    s_last_breakdown = (ai_eval_breakdown_t){0};
}

void ai_agent_set_progress_callback(ai_progress_callback_t callback, void *user_data) {
    s_progress_callback = callback;
    s_progress_user_data = user_data;
}

// Call the global progress callback if set
void ai_agent_call_progress_callback(void) {
    if (s_progress_callback) {
        s_progress_callback(s_progress_user_data);
    }
}

ai_blunder_type_t ai_allowed_blunder_type(ai_difficulty_t difficulty) {
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
        case AI_DIFFICULTY_EASY:
            return AI_BLUNDER_ALLOW_IMMEDIATE_WIN;
        case AI_DIFFICULTY_STANDARD:
            return AI_BLUNDER_ALLOW_FORCING_MOVE;
        default:
            return AI_BLUNDER_NONE;
    }
}

void ai_agent_config_set_randomization(ai_config_t *config, uint8_t top_k, uint8_t epsilon_pct) {
    if (!config) {
        return;
    }
    if (top_k == 0u) {
        top_k = 1u;
    }
    config->random_top_k = top_k;
    config->random_epsilon_pct = (epsilon_pct > 100u) ? 100u : epsilon_pct;
}

void ai_agent_config_set_blunder(ai_config_t *config, bool enabled, ai_blunder_type_t type, uint8_t chance_pct) {
    if (!config) {
        return;
    }

    ai_blunder_type_t allowed = ai_allowed_blunder_type(config->difficulty);
    if (allowed == AI_BLUNDER_NONE) {
        config->blunder_type = AI_BLUNDER_NONE;
        config->blunder_enabled = false;
        config->blunder_chance_pct = 0u;
        return;
    }

    config->blunder_type = (type == allowed) ? type : allowed;
    uint8_t clamped = (chance_pct > 100u) ? 100u : chance_pct;
    config->blunder_chance_pct = clamped;
    config->blunder_enabled = enabled && clamped > 0u;
}

static void ai_sort_indices_by_evaluation(const ai_evaluated_moves_t *evaluated, uint8_t *indices, uint8_t count) {
    for (uint8_t i = 1u; i < count; ++i) {
        uint8_t key = indices[i];
        int16_t value = evaluated->evaluations[key];
        uint8_t j = i;
        while (j > 0u && evaluated->evaluations[indices[j - 1u]] < value) {
            indices[j] = indices[j - 1u];
            --j;
        }
        indices[j] = key;
    }
}

__attribute__((noinline, section(".block10"))) static uint8_t FAR10_ai_evaluate_moves(
    board_t *root, player_t current_player, const ai_config_t *config, const ai_ordered_moves_t *ordered,
    uint8_t generated, ai_evaluated_moves_t *evaluated, uint32_t *out_nodes) {
    if (!root || !config || !ordered || !evaluated) {
        return 0u;
    }

    uint8_t count = 0u;
    player_t opponent = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    for (uint8_t i = 0u; i < generated && count < AI_MAX_ORDERED_MOVES; ++i) {
        uint8_t move_slot = ordered->indices[i];
        if (move_slot >= AI_MAX_ORDERED_MOVES) {
            continue;
        }

        move_t move = {.from_row = ordered->moves.from_rows[move_slot],
                       .from_col = ordered->moves.from_cols[move_slot],
                       .to_row = ordered->moves.to_rows[move_slot],
                       .to_col = ordered->moves.to_cols[move_slot],
                       .type = ordered->moves.types[move_slot],
                       .player = ordered->moves.players[move_slot]};

        board_t child;
        ai_board_copy(&child, root);
        board_context_t move_context = {.current_player = current_player};
        if (!board_execute_move_without_history(&child, &move_context, &move, config->swap_rule)) {
            continue;
        }

        uint8_t move_flags = ordered->flags[move_slot];
        bool opponent_immediate = (move_flags & AI_ORDER_FLAG_OPPONENT_IMMEDIATE) != 0u;
        if (opponent_immediate) {
            continue;
        }

        bool self_immediate = (move_flags & AI_ORDER_FLAG_SELF_IMMEDIATE) != 0u;

        uint8_t target_index = count;
        if (self_immediate) {
            target_index = 0u;
        }

        evaluated->moves.from_rows[target_index] = move.from_row;
        evaluated->moves.from_cols[target_index] = move.from_col;
        evaluated->moves.to_rows[target_index] = move.to_row;
        evaluated->moves.to_cols[target_index] = move.to_col;
        evaluated->moves.types[target_index] = move.type;
        evaluated->moves.players[target_index] = move.player;
        evaluated->immediate_wins_opponent[target_index] = false;

        if (self_immediate) {
            evaluated->immediate_wins_self[target_index] = true;
            evaluated->opponent_wins_next_move[target_index] = false;
            evaluated->opponent_forced_wins[target_index] = false;
            evaluated->forced_wins_self[target_index] = false;
            evaluated->evaluations[target_index] = AI_SCORE_WIN - (int16_t)(root->move_count & 0x7FFF);
            if (out_nodes) {
                ++(*out_nodes);
            }
            count = 1u;
            break;
        }

        evaluated->immediate_wins_self[target_index] = false;

        board_context_t next_context = {.current_player = current_player};
        board_switch_turn(&next_context);
        player_t defender = next_context.current_player;

    bool opponent_win_next = ai_immediate_win_available(&child, opponent, config->swap_rule);
        evaluated->opponent_wins_next_move[target_index] = opponent_win_next;

        bool forced_self = false;
        if (config->enable_forcing_check && config->difficulty >= AI_DIFFICULTY_STANDARD) {
            forced_self =
                ai_board_creates_forced_immediate_win_postmove(&child, move.player, move.player, config->swap_rule);
        }
        evaluated->forced_wins_self[target_index] = forced_self;

        bool opponent_forced = false;
        if (config->enable_forcing_check && config->difficulty == AI_DIFFICULTY_EXPERT && !opponent_win_next) {
            opponent_forced = ai_forcing_move_available(&child, defender, opponent, config->swap_rule);
        }
        evaluated->opponent_forced_wins[target_index] = opponent_forced;

        ai_config_t eval_config = *config;
        eval_config.enable_forcing_check = false;

        int16_t eval_score = ai_agent_evaluate_internal(&child, defender, config->ai_player, &eval_config, NULL);

        if (config->enable_forcing_check && opponent_forced) {
            eval_score = AI_SCORE_LOSS + (int16_t)(child.move_count & 0x7FFF) + 1000;
        }

        evaluated->evaluations[target_index] = eval_score;

        if (out_nodes) {
            ++(*out_nodes);
        }

        ++count;
    }

    return count;
}

// Evaluate a single move and fill the ai_evaluated_move_t struct
// This is used for debugging and testing individual moves
void ai_evaluate_single_move(const board_t *board, player_t current_player, const move_t *move,
                             const ai_config_t *config, ai_evaluated_move_t *result) {
    if (!board || !move || !config || !result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->move = *move;

    player_t opponent = (config->ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Create child board after the move
    board_t child;
    ai_board_copy(&child, board);
    board_context_t move_context = {.current_player = current_player};
    if (!board_execute_move_without_history(&child, &move_context, move, config->swap_rule)) {
        return;  // Invalid move
    }

    // Check immediate wins
    result->immediate_win_self = board_check_win_fast(&child, config->ai_player);
    result->immediate_win_opponent = board_check_win_fast(&child, opponent);

    // Check forced win (STANDARD and EXPERT, skip if immediate win already found)
    result->forced_win_self = false;
    if (config->enable_forcing_check && config->difficulty >= AI_DIFFICULTY_STANDARD && !result->immediate_win_self) {
        result->forced_win_self =
            ai_board_creates_forced_immediate_win_postmove(&child, current_player, move->player, config->swap_rule);
    }

    // Switch to opponent's turn for further evaluation
    board_switch_turn(&move_context);

    // Check if opponent can win immediately
    {
        result->opponent_win_next_move = ai_immediate_win_available(&child, opponent, config->swap_rule);
    }

    // Check if opponent has forced win
    result->opponent_forced_win = false;
    if (config->enable_forcing_check && config->difficulty == AI_DIFFICULTY_EXPERT && !result->opponent_win_next_move) {
        result->opponent_forced_win =
            FAR9_ai_forcing_move_available(&child, move_context.current_player, opponent, config->swap_rule);
    }

    // Evaluate the position
    result->evaluation = ai_agent_evaluate_internal(&child, opponent, config->ai_player, config, NULL);
}

__attribute__((noinline, section(".block10"))) static bool FAR10_ai_choose_move_from_evaluated(
    const ai_config_t *config, const ai_evaluated_moves_t *evaluated, uint8_t count, move_t *out_move,
    bool *applied_blunder) {
    if (!config || !evaluated || !out_move || count == 0u) {
        return false;
    }

    if (applied_blunder) {
        *applied_blunder = false;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        if (evaluated->immediate_wins_self[i]) {
            *out_move = (move_t){.from_row = evaluated->moves.from_rows[i],
                                 .from_col = evaluated->moves.from_cols[i],
                                 .to_row = evaluated->moves.to_rows[i],
                                 .to_col = evaluated->moves.to_cols[i],
                                 .type = evaluated->moves.types[i],
                                 .player = evaluated->moves.players[i]};
            return true;
        }
    }

    uint8_t safe_indices[AI_MAX_ORDERED_MOVES];
    uint8_t safe_count = 0u;
    uint8_t blunder_indices[AI_MAX_ORDERED_MOVES];
    uint8_t blunder_count = 0u;

    bool use_hint = config->use_hint_profile;
    bool base_blunder_enabled = config->blunder_enabled && !use_hint && config->blunder_chance_pct > 0u &&
                                config->difficulty != AI_DIFFICULTY_EXPERT && config->blunder_type != AI_BLUNDER_NONE;

    // Track forced wins so STANDARD can optionally skip them on a blunder.
    bool forced_win_mask[AI_MAX_ORDERED_MOVES] = {false};
    uint8_t forced_win_indices[AI_MAX_ORDERED_MOVES];
    uint8_t forced_win_count = 0u;
    if (config->enable_forcing_check && config->difficulty >= AI_DIFFICULTY_STANDARD) {
        for (uint8_t i = 0u; i < count; ++i) {
            if (evaluated->forced_wins_self[i]) {
                forced_win_mask[i] = true;
                forced_win_indices[forced_win_count++] = i;
            }
        }
    }

    bool forced_win_blunder = false;
    if (forced_win_count > 0u) {
        uint8_t best_forced_index = forced_win_indices[0u];
        for (uint8_t i = 1u; i < forced_win_count; ++i) {
            uint8_t idx = forced_win_indices[i];
            if (evaluated->evaluations[idx] > evaluated->evaluations[best_forced_index]) {
                best_forced_index = idx;
            }
        }

        if (config->difficulty == AI_DIFFICULTY_EXPERT) {
            *out_move = (move_t){.from_row = evaluated->moves.from_rows[best_forced_index],
                                 .from_col = evaluated->moves.from_cols[best_forced_index],
                                 .to_row = evaluated->moves.to_rows[best_forced_index],
                                 .to_col = evaluated->moves.to_cols[best_forced_index],
                                 .type = evaluated->moves.types[best_forced_index],
                                 .player = evaluated->moves.players[best_forced_index]};
            return true;
        }

        if (config->difficulty == AI_DIFFICULTY_STANDARD) {
            bool skip_forced = base_blunder_enabled && ai_random_chance(config->blunder_chance_pct);
            if (!skip_forced) {
                *out_move = (move_t){.from_row = evaluated->moves.from_rows[best_forced_index],
                                     .from_col = evaluated->moves.from_cols[best_forced_index],
                                     .to_row = evaluated->moves.to_rows[best_forced_index],
                                     .to_col = evaluated->moves.to_cols[best_forced_index],
                                     .type = evaluated->moves.types[best_forced_index],
                                     .player = evaluated->moves.players[best_forced_index]};
                return true;
            }

            forced_win_blunder = true;
            if (applied_blunder) {
                *applied_blunder = true;
            }
        }
    }

    bool blunder_enabled = base_blunder_enabled && !forced_win_blunder;

    for (uint8_t i = 0u; i < count; ++i) {
        if (forced_win_blunder && forced_win_mask[i]) {
            continue;
        }

        if (evaluated->immediate_wins_opponent[i]) {
            continue;
        }

        bool disqualify = false;
        bool eligible_blunder = false;

        if (evaluated->opponent_wins_next_move[i]) {
            disqualify = true;
            if (blunder_enabled && config->blunder_type == AI_BLUNDER_ALLOW_IMMEDIATE_WIN) {
                eligible_blunder = true;
            }
        } else if (evaluated->opponent_forced_wins[i] && config->enable_forcing_check) {
            disqualify = true;
            if (blunder_enabled && config->blunder_type == AI_BLUNDER_ALLOW_FORCING_MOVE) {
                eligible_blunder = true;
            }
        }

        if (!disqualify) {
            safe_indices[safe_count++] = i;
        } else if (eligible_blunder) {
            blunder_indices[blunder_count++] = i;
        }
    }

    if (safe_count > 0u) {
        uint8_t best_index = safe_indices[0u];
        for (uint8_t i = 1u; i < safe_count; ++i) {
            uint8_t idx = safe_indices[i];
            if (evaluated->evaluations[idx] > evaluated->evaluations[best_index]) {
                best_index = idx;
            }
        }

        if (blunder_enabled && blunder_count > 0u && ai_random_chance(config->blunder_chance_pct)) {
            uint8_t choice = (uint8_t)ai_random_range(blunder_count);
            uint8_t blunder_idx = blunder_indices[choice];
            *out_move = (move_t){.from_row = evaluated->moves.from_rows[blunder_idx],
                                 .from_col = evaluated->moves.from_cols[blunder_idx],
                                 .to_row = evaluated->moves.to_rows[blunder_idx],
                                 .to_col = evaluated->moves.to_cols[blunder_idx],
                                 .type = evaluated->moves.types[blunder_idx],
                                 .player = evaluated->moves.players[blunder_idx]};
            if (applied_blunder) {
                *applied_blunder = true;
            }
            return true;
        }

        uint8_t chosen_index = best_index;

        if (!use_hint && config->random_top_k > 1u && config->random_epsilon_pct > 0u && safe_count > 1u &&
            ai_random_chance(config->random_epsilon_pct)) {
            uint8_t top_indices[AI_MAX_ORDERED_MOVES];
            for (uint8_t i = 0u; i < safe_count; ++i) {
                top_indices[i] = safe_indices[i];
            }
            ai_sort_indices_by_evaluation(evaluated, top_indices, safe_count);

            uint8_t limit = config->random_top_k;
            if (limit > safe_count) {
                limit = safe_count;
            }
            if (limit > 1u) {
                uint8_t choice = (uint8_t)ai_random_range(limit);
                chosen_index = top_indices[choice];
            }
        }

        *out_move = (move_t){.from_row = evaluated->moves.from_rows[chosen_index],
                             .from_col = evaluated->moves.from_cols[chosen_index],
                             .to_row = evaluated->moves.to_rows[chosen_index],
                             .to_col = evaluated->moves.to_cols[chosen_index],
                             .type = evaluated->moves.types[chosen_index],
                             .player = evaluated->moves.players[chosen_index]};
        return true;
    }

    // If no safe moves and no blunder moves, return false (no valid moves)
    return false;
}

bool FAR10_ai_agent_find_best_move_impl(const board_t *board, player_t current_player, const ai_config_t *config,
                                        move_t *out_move);

#if defined(AI_AGENT_HOST_TEST)

bool ai_agent_find_best_move_impl(const board_t *board, player_t current_player, const ai_config_t *config,
                                  move_t *out_move) {
    return FAR10_ai_agent_find_best_move_impl(board, current_player, config, out_move);
}

#else

__attribute__((noinline)) bool ai_agent_find_best_move_impl(const board_t *board, player_t current_player,
                                                            const ai_config_t *config, move_t *out_move) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    bool ret;
    *(volatile unsigned char *)0x000d = 10;
    ret = FAR10_ai_agent_find_best_move_impl(board, current_player, config, out_move);
    *(volatile unsigned char *)0x000d = ___mmu;
    return ret;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block10"))) bool FAR10_ai_agent_find_best_move_impl(const board_t *board,
                                                                                       player_t current_player,
                                                                                       const ai_config_t *config,
                                                                                       move_t *out_move) {
    if (!board || !config || !out_move) {
        return false;
    }

    board_t root;
    ai_board_copy(&root, board);

    ai_config_t tuned = *config;
    if (!tuned.use_hint_profile) {
        tuned.ai_player = current_player;
    }

    ai_ordered_moves_t ordered;
    uint8_t generated = FAR10_ai_generate_moves(&root, current_player, &tuned, &ordered);
    if (generated == 0u) {
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    if (tuned.enable_forcing_check && ai_all_replies_allow_opponent_immediate_win_from_ordered(&ordered, generated)) {
        tuned.enable_forcing_check = false;
    }

    ai_evaluated_moves_t evaluated;
    uint32_t nodes_recorded = 0u;
    uint8_t evaluated_count =
        FAR10_ai_evaluate_moves(&root, current_player, &tuned, &ordered, generated, &evaluated, &nodes_recorded);

    bool applied_blunder = false;
    move_t chosen_move = {0};
    bool move_found =
        FAR10_ai_choose_move_from_evaluated(&tuned, &evaluated, evaluated_count, &chosen_move, &applied_blunder);

    if (!move_found) {
        if (generated > 0u) {
            // Fallback: pick the first valid move (should not happen for STANDARD/EXPERT)
            for (uint8_t i = 0u; i < generated && !move_found; ++i) {
                uint8_t slot = ordered.indices[i];
                if (slot >= AI_MAX_ORDERED_MOVES) {
                    continue;
                }
                // For STANDARD and EXPERT, skip WIN_NEXT_MOVE_A moves
                if (tuned.difficulty >= AI_DIFFICULTY_STANDARD &&
                    (ordered.flags[slot] & AI_ORDER_FLAG_OPPONENT_IMMEDIATE) != 0u) {
                    continue;
                }
                move_t candidate = {.from_row = ordered.moves.from_rows[slot],
                                   .from_col = ordered.moves.from_cols[slot],
                                   .to_row = ordered.moves.to_rows[slot],
                                   .to_col = ordered.moves.to_cols[slot],
                                   .type = ordered.moves.types[slot],
                                   .player = ordered.moves.players[slot]};
                chosen_move = candidate;
                move_found = true;
            }
        }
    }

    if (!move_found) {
        /* As a final fallback (should not happen), attempt to pick the first legal move. */
        for (uint8_t i = 0u; i < generated && !move_found; ++i) {
            uint8_t slot = ordered.indices[i];
            if (slot >= AI_MAX_ORDERED_MOVES) {
                continue;
            }
            // Final fallback: accept any legal move, even WIN_NEXT_MOVE_A
            move_t candidate = {.from_row = ordered.moves.from_rows[slot],
                               .from_col = ordered.moves.from_cols[slot],
                               .to_row = ordered.moves.to_rows[slot],
                               .to_col = ordered.moves.to_cols[slot],
                               .type = ordered.moves.types[slot],
                               .player = ordered.moves.players[slot]};
            chosen_move = candidate;
            move_found = true;
        }
    }

    if (!move_found) {
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    if (applied_blunder && config->difficulty == AI_DIFFICULTY_LEARNING) {
        print_made_blunder();
    }

    *out_move = chosen_move;

    if (config->diagnostics_enabled) {
        board_t analysed;
        ai_board_copy(&analysed, board);
        board_context_t dummy_context = {.current_player = tuned.ai_player};
        if (board_execute_move_without_history(&analysed, &dummy_context, out_move, tuned.swap_rule)) {
            board_switch_turn(&dummy_context);
            ai_eval_breakdown_t breakdown;
            ai_agent_evaluate_internal(&analysed, dummy_context.current_player, tuned.ai_player, &tuned, &breakdown);
            s_last_breakdown = breakdown;
        } else {
            s_last_breakdown = (ai_eval_breakdown_t){0};
        }
    } else {
        s_last_breakdown = (ai_eval_breakdown_t){0};
    }

    return true;
}

bool ai_agent_find_best_move(const board_t *board, const board_context_t *context, const ai_config_t *config,
                             move_t *out_move) {
    if (!board || !context || !config || !out_move) {
        return false;
    }

    return ai_agent_find_best_move_impl(board, context->current_player, config, out_move);
}

int16_t ai_agent_evaluate_board(const board_t *board, player_t player, const ai_config_t *config) {
    if (!board || !config) {
        return 0;
    }

    return ai_agent_evaluate_internal(board, player, player, config, NULL);
}

void ai_agent_get_last_breakdown(ai_eval_breakdown_t *out) {
    if (!out) {
        return;
    }
    *out = s_last_breakdown;
}
