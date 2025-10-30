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
    0xD650  // master control register for timer0, write.b0=ticks b1=reset b2=set to last value of VAL b3=set count up, clear count down
#define T0_STAT \
    0xD650  // master control register for timer0, read bit0 set = reached target val

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

static void ai_timer0_reset(void) {
    POKE(T0_CTR, CTR_CLEAR);
    POKE(T0_CTR, CTR_UPDOWN | CTR_ENABLE);
    POKE(T0_PEND, 0x10);
    POKE(T0_CMP_CTR, T0_CMP_CTR_RECLEAR);
    POKE(T0_CMP_L, 0xFF);
    POKE(T0_CMP_M, 0xFF);
    POKE(T0_CMP_H, 0xFF);
}

static uint32_t ai_timer0_read(void) {
    uint32_t value_h = (uint32_t)PEEK(T0_VAL_H) << 16;
    uint32_t value_m = (uint32_t)PEEK(T0_VAL_M) << 8;
    uint32_t value_l = (uint32_t)PEEK(T0_VAL_L);
    return value_h | value_m | value_l;
}

static void ai_format_diag(char *dest, const char *label, uint32_t value) {
    uint8_t i = 0;
    while (label[i] != '\0' && i < 23) {
        dest[i] = label[i];
        ++i;
    }
    if (i < 23) {
        dest[i++] = ' ';
    }
    if (value == 0u) {
        dest[i++] = '0';
    } else {
        char digits[10];
        uint8_t count = 0;
        while (value != 0u && count < sizeof(digits)) {
            digits[count++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        while (count > 0 && i < 25) {
            dest[i++] = digits[--count];
        }
    }
    dest[i] = '\0';
}

static void ai_print_diagnostics(uint32_t nodes, uint32_t ticks, bool enabled) {
    if (!enabled) {
        print_formatted_text(1, 45, "");
        print_formatted_text(1, 46, "");
        return;
    }

    char buf_nodes[26];
    char buf_ticks[26];
    ai_format_diag(buf_nodes, "AI NODES:", nodes);
    ai_format_diag(buf_ticks, "AI TICKS:", ticks);
    print_formatted_text(1, 45, buf_nodes);
    print_formatted_text(1, 46, buf_ticks);
}
#else
static void ai_timer0_reset(void) {}

static uint32_t ai_timer0_read(void) {
    return 0u;
}

static void ai_print_diagnostics(uint32_t nodes, uint32_t ticks, bool enabled) {
    (void)nodes;
    (void)ticks;
    (void)enabled;
}
#endif

#define AI_SCORE_WIN 30000
#define AI_SCORE_LOSS (-AI_SCORE_WIN)
#define AI_SCORE_MAX 32000

static uint16_t s_ai_rng_state = 0xC0FEu;

static void ai_random_seed_internal(uint16_t seed) {
    if (seed == 0u) {
        seed = 1u;
    }
#if defined(__llvm_mos__)
    randomSeed(seed);
#else
    s_ai_rng_state = seed;
#endif
}

void ai_agent_set_random_seed(uint16_t seed) {
    ai_random_seed_internal(seed);
}

static uint16_t ai_random_next(void) {
#if defined(__llvm_mos__)
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
static const uint8_t kPieceOwnerLUT[5] = {PLAYER_NONE, PLAYER_WHITE, PLAYER_WHITE, PLAYER_BLACK, PLAYER_BLACK};
static const uint8_t kPieceIsNormalLUT[5] = {0u, 1u, 0u, 1u, 0u};

typedef struct {
    move_t move;
    int16_t order_score;
} ai_ordered_move_t;

typedef struct {
    uint8_t row_mask;
    uint8_t row_links;
    uint8_t branching_nodes;
} ai_connection_metrics_t;

static bool s_zobrist_ready = false;
static uint16_t s_zobrist_board[BOARD_CELLS][5];
static uint16_t s_zobrist_player[2];
static ai_eval_breakdown_t s_last_breakdown;

static const ai_eval_weights_t kRuleWeights[4] = {
    {88, 58, 36, 44, 12},  // Classic
    {84, 54, 32, 44, 12},  // Clears Own
    {72, 52, 50, 38, 16},  // Swapped Clears
    {72, 50, 46, 38, 16}   // Swapped Clears Own
};

static inline void ai_board_copy(board_t *dest, const board_t *src) {
    memcpy(dest, src, sizeof(board_t));
}

static uint8_t ai_popcount(uint8_t value) {
    value = (value & 0x55u) + ((value >> 1) & 0x55u);
    value = (value & 0x33u) + ((value >> 2) & 0x33u);
    return (uint8_t)((value + (value >> 4)) & 0x0Fu);
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

static void ai_init_zobrist(void) {
    if (s_zobrist_ready) {
        return;
    }

    uint32_t seed = 0x9E3779B9u;
    for (uint8_t cell = 0; cell < BOARD_CELLS; ++cell) {
        for (uint8_t piece = 0; piece < 5; ++piece) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            s_zobrist_board[cell][piece] = (uint16_t)(seed ^ (seed >> 16));
        }
    }
    for (uint8_t p = 0; p < 2; ++p) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        s_zobrist_player[p] = (uint16_t)(seed ^ (seed >> 16));
    }

    s_zobrist_ready = true;
}

static uint32_t ai_hash_board(const board_t *board) {
    ai_init_zobrist();
    uint32_t hash = 0;

    // Search for an opponent move that leaves no immediate-safe reply for the AI.
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (piece != PIECE_NONE) {
                uint8_t idx = (uint8_t)(row * BOARD_COLS + col);
                hash ^= (uint32_t)s_zobrist_board[idx][piece];
            }
        }
    }

    if (board->current_player == PLAYER_WHITE || board->current_player == PLAYER_BLACK) {
        hash ^= (uint32_t)s_zobrist_player[board->current_player];
    }

    return hash;
}

static bool ai_same_move(const move_t *a, const move_t *b) {
    return a->from_row == b->from_row && a->from_col == b->from_col && a->to_row == b->to_row &&
           a->to_col == b->to_col && a->type == b->type;
}

static uint8_t ai_count_goal_rows_for_player(const board_t *board, player_t player) {
    uint8_t rows = 0;
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        bool has_piece = false;
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                has_piece = true;
                break;
            }
        }
        if (has_piece) {
            ++rows;
        }
    }
    return rows;
}

static uint8_t ai_count_swapped_for_player(const board_t *board, player_t player) {
    uint8_t count = 0u;
    piece_type_t swapped = (player == PLAYER_WHITE) ? PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            if (board_get_piece_unchecked(board, row, col) == swapped) {
                ++count;
            }
        }
    }
    return count;
}

static bool ai_board_has_swapped_for_player(const board_t *board, player_t player) {
    return ai_count_swapped_for_player(board, player) > 0u;
}

static bool ai_board_has_any_swapped(const board_t *board) {
    return ai_board_has_swapped_for_player(board, PLAYER_WHITE) || ai_board_has_swapped_for_player(board, PLAYER_BLACK);
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

void
ai_compute_connection_metrics(const board_t *board, player_t player,
                              ai_connection_metrics_t *out)
{
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

uint8_t
ai_count_bridge_potential(const board_t *board, player_t player) {
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

uint8_t
ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
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

uint8_t
FAR9_ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
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

uint8_t
ai_measure_blocking(const board_t *board, player_t player) {
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

uint8_t
FAR9_ai_measure_blocking(const board_t *board, player_t player) {
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

uint16_t
ai_measure_mobility(const board_t *board, player_t player) {
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

uint16_t
FAR9_ai_measure_mobility(const board_t *board, player_t player) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = player;

    uint16_t mobility = 0;
    move_array_t soa_moves;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            mobility += board_get_legal_moves_soa(&scratch, row, col, &soa_moves);
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


bool ai_immediate_win_available(const board_t *board,
                                                                                   player_t player, swap_rule_t rule) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = player;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(&scratch, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t candidate;
                move_array_get_move(&soa_moves, i, &candidate);

                board_t test;
                ai_board_copy(&test, &scratch);
                if (!board_execute_move_without_history(&test, &candidate, rule)) {
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
static bool ai_move_creates_forced_immediate_win(const board_t *board, const move_t *move, const ai_config_t *config,
                                                 player_t ai_player) {
    if (!config || !move) {
        return false;
    }

    board_t after_ai;
    ai_board_copy(&after_ai, board);
    if (!board_execute_move_without_history(&after_ai, move, config->swap_rule)) {
        return false;
    }

    if (board_check_win_fast(&after_ai, ai_player)) {
        return true;
    }

    board_t opponent_state;
    ai_board_copy(&opponent_state, &after_ai);
    board_switch_turn(&opponent_state);

    player_t opponent = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    opponent_state.current_player = opponent;

    bool opponent_has_moves = false;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&opponent_state, row, col);
            if (board_get_piece_owner(piece) != opponent) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(&opponent_state, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t response_move;
                move_array_get_move(&soa_moves, i, &response_move);
                opponent_has_moves = true;

                board_t after_opponent;
                ai_board_copy(&after_opponent, &opponent_state);
                if (!board_execute_move_without_history(&after_opponent, &response_move, config->swap_rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_opponent, opponent)) {
                    return false;
                }

                board_switch_turn(&after_opponent);
                after_opponent.current_player = ai_player;

                if (!ai_immediate_win_available(&after_opponent, ai_player, config->swap_rule)) {
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

static bool ai_move_allows_opponent_immediate_win(const board_t *board,
                                                  const move_t *move,
                                                  const ai_config_t *config,
                                                  player_t ai_player) {
    if (!config || !move) {
        return false;
    }

    board_t after_ai;
    ai_board_copy(&after_ai, board);
    if (!board_execute_move_without_history(&after_ai, move, config->swap_rule)) {
        return false;
    }

    player_t opponent = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    board_switch_turn(&after_ai);
    after_ai.current_player = opponent;

    return ai_immediate_win_available(&after_ai, opponent, config->swap_rule);
}

static bool ai_all_replies_allow_opponent_immediate_win(const board_t *board, const ai_config_t *config) {
    if (!board || !config) {
        return false;
    }

    board_t scratch;
    ai_board_copy(&scratch, board);
    player_t to_move = scratch.current_player;
    player_t opponent = (to_move == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    bool has_move = false;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&scratch, row, col);
            if (board_get_piece_owner(piece) != to_move) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(&scratch, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t candidate;
                move_array_get_move(&soa_moves, i, &candidate);
                has_move = true;

                board_t after_move;
                ai_board_copy(&after_move, &scratch);
                if (!board_execute_move_without_history(&after_move, &candidate, config->swap_rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_move, to_move)) {
                    return false;
                }

                board_switch_turn(&after_move);
                after_move.current_player = opponent;

                if (!ai_immediate_win_available(&after_move, opponent, config->swap_rule)) {
                    return false;
                }
            }
        }
    }

    if (!has_move) {
        return false;
    }

    return true;
}

#if defined(AI_AGENT_HOST_TEST)
bool ai_agent_detect_unavoidable_loss(const board_t *board, const ai_config_t *config) {
    return ai_all_replies_allow_opponent_immediate_win(board, config);
}

bool ai_agent_move_creates_forced_immediate_win(const board_t *board,
                                                const move_t *move,
                                                const ai_config_t *config,
                                                player_t ai_player) {
    return ai_move_creates_forced_immediate_win(board, move, config, ai_player);
}

bool ai_agent_move_allows_opponent_immediate_win(const board_t *board,
                                                 const move_t *move,
                                                 const ai_config_t *config,
                                                 player_t ai_player) {
    return ai_move_allows_opponent_immediate_win(board, move, config, ai_player);
}
#endif

bool FAR9_ai_forcing_move_available(const board_t *board, player_t player, swap_rule_t rule);

#if defined(AI_AGENT_HOST_TEST)

bool ai_forcing_move_available(const board_t *board, player_t player, swap_rule_t rule) {
    return FAR9_ai_forcing_move_available(board, player, rule);
}

#else

#pragma clang optimize off
__attribute__((noinline))

bool
ai_forcing_move_available(const board_t *board, player_t player, swap_rule_t rule)
{
    bool return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_forcing_move_available(board, player, rule);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) bool FAR9_ai_forcing_move_available(const board_t *board, player_t player,
                                                                                  swap_rule_t rule) {
    if (!board || player == PLAYER_NONE) {
        return false;
    }

    player_t opponent = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    board_t scratch;
    ai_board_copy(&scratch, board);

    bool last_move_by_player = (scratch.history_count > 0u && scratch.history[0].player == player);

    // If the most recent move was made by the player under consideration, we are looking at
    // the state immediately after their move. The opponent now moves first, so ensure every
    // opponent reply leaves an immediate win for the player on the following turn.
    if (last_move_by_player) {
        board_t opponent_state;
        ai_board_copy(&opponent_state, &scratch);
        opponent_state.current_player = opponent;

    move_array_t response_moves;

        for (uint8_t resp_row = 0; resp_row < BOARD_ROWS; ++resp_row) {
            for (uint8_t resp_col = 0; resp_col < BOARD_COLS; ++resp_col) {
                piece_type_t resp_piece = board_get_piece_unchecked(&opponent_state, resp_row, resp_col);
                if (board_get_piece_owner(resp_piece) != opponent) {
                    continue;
                }

                uint8_t response_count = board_get_legal_moves_soa(&opponent_state, resp_row, resp_col, &response_moves);
                for (uint8_t resp_idx = 0; resp_idx < response_count; ++resp_idx) {
                    move_t response_move;
                    move_array_get_move(&response_moves, resp_idx, &response_move);

                    board_t after_opponent;
                    ai_board_copy(&after_opponent, &opponent_state);
                    if (!board_execute_move_without_history(&after_opponent, &response_move, rule)) {
                        continue;
                    }

                    if (board_check_win_fast(&after_opponent, opponent)) {
                        return false;
                    }

                    board_switch_turn(&after_opponent);
                    after_opponent.current_player = player;

                    if (!ai_immediate_win_available(&after_opponent, player, rule)) {
                        return false;
                    }
                }
            }
        }

    // If the opponent has no legal replies, the win is trivially forced.
    return true;
    }

    // Otherwise, the specified player is about to move. Search for a move that guarantees
    // an immediate win on the following turn regardless of the opponent's response.
    scratch.current_player = player;

    ai_config_t forcing_config;
    memset(&forcing_config, 0, sizeof(forcing_config));
    forcing_config.swap_rule = rule;

    move_array_t candidate_moves;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            uint8_t move_count = board_get_legal_moves_soa(&scratch, row, col, &candidate_moves);
            for (uint8_t move_idx = 0; move_idx < move_count; ++move_idx) {
                move_t candidate;
                move_array_get_move(&candidate_moves, move_idx, &candidate);

                board_t after_move;
                ai_board_copy(&after_move, &scratch);
                if (!board_execute_move_without_history(&after_move, &candidate, rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_move, player)) {
                    continue;
                }

                if (ai_move_creates_forced_immediate_win(&scratch, &candidate, &forcing_config, player)) {
                    return true;
                }
            }
        }
    }

    return false;
}
uint8_t FAR10_ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_moves_t *out_moves);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_moves_t *out_moves) {
    return FAR10_ai_generate_moves(board, config, out_moves);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_moves_t *out_moves) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 10;
    return_value = FAR10_ai_generate_moves(board, config, out_moves);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block10"))) uint8_t FAR10_ai_generate_moves(const board_t *board,
                                                                             const ai_config_t *config,
                                                                             ai_ordered_moves_t *out_moves) {
    if (!board || !config || !out_moves) {
        return 0u;
    }

    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = board->current_player;

    uint8_t count = 0;
    const player_t current = scratch.current_player;
    swap_rule_t swap_rule = config->swap_rule;
    bool forcing_check = config->enable_forcing_check && (config->ai_player == current);
    move_array_t soa_moves;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        board_cell_t *row_cells = scratch.cells[row];
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t moving_piece = row_cells[col].piece;
            if ((player_t)kPieceOwnerLUT[moving_piece] != current) {
                continue;
            }

            bool mover_swapped = board_is_piece_swapped(moving_piece);
            uint8_t num_moves = board_get_legal_moves_soa(&scratch, row, col, &soa_moves);

            for (uint8_t i = 0; i < num_moves; ++i) {
                move_t move;
                move.from_row = row;
                move.from_col = col;
                move.to_row = move_array_get_to_row(&soa_moves, i);
                move.to_col = move_array_get_to_col(&soa_moves, i);
                move.type = move_array_get_type(&soa_moves, i);
                move.player = current;

                int16_t score = 0;

                if (move.type == MOVE_TYPE_SWAP) {
                    score += 900;
                    score += mover_swapped ? 220 : 500;
                    if (move.to_col == 1 || move.to_col == 2) {
                        score += 150;
                    }
                } else {
                    bool clears_swapped = ai_move_clears_swapped(&scratch, &move, swap_rule);
                    if (clears_swapped) {
                        score -= 1400;
                    } else if (mover_swapped) {
                        score += 220;
                    }
                }

                if (move.to_col == 1 || move.to_col == 2) {
                    score += 350;
                }

                if (current == PLAYER_WHITE) {
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

                if (forcing_check &&
                    ai_move_creates_forced_immediate_win(board, &move, config, config->ai_player)) {
                    score += 8000;
                }

                if (config->ai_player == current &&
                    ai_move_allows_opponent_immediate_win(board, &move, config, config->ai_player)) {
                    score -= 12000;
                }

                uint8_t slot_index = 0xFFu;  // Invalid index
                if (count < AI_MAX_ORDERED_MOVES) {
                    slot_index = count++;
                } else {
                    uint8_t min_index = 0u;
                    int16_t min_score = out_moves->order_scores[0];
                    for (uint8_t j = 1u; j < count; ++j) {
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

    for (uint8_t i = 1; i < count; ++i) {
        move_t key_move = {
            .from_row = out_moves->moves.from_rows[i],
            .from_col = out_moves->moves.from_cols[i],
            .to_row = out_moves->moves.to_rows[i],
            .to_col = out_moves->moves.to_cols[i],
            .type = out_moves->moves.types[i],
            .player = out_moves->moves.players[i]
        };
        int16_t key_score = out_moves->order_scores[i];
        uint8_t j = i;
        while (j > 0 && out_moves->order_scores[j - 1] < key_score) {
            out_moves->moves.from_rows[j] = out_moves->moves.from_rows[j - 1];
            out_moves->moves.from_cols[j] = out_moves->moves.from_cols[j - 1];
            out_moves->moves.to_rows[j] = out_moves->moves.to_rows[j - 1];
            out_moves->moves.to_cols[j] = out_moves->moves.to_cols[j - 1];
            out_moves->moves.types[j] = out_moves->moves.types[j - 1];
            out_moves->moves.players[j] = out_moves->moves.players[j - 1];
            out_moves->order_scores[j] = out_moves->order_scores[j - 1];
            --j;
        }
        out_moves->moves.from_rows[j] = key_move.from_row;
        out_moves->moves.from_cols[j] = key_move.from_col;
        out_moves->moves.to_rows[j] = key_move.to_row;
        out_moves->moves.to_cols[j] = key_move.to_col;
        out_moves->moves.types[j] = key_move.type;
        out_moves->moves.players[j] = key_move.player;
        out_moves->order_scores[j] = key_score;
    }

    return count;
}

int16_t FAR9_ai_agent_evaluate_internal(const board_t *board, player_t perspective, const ai_config_t *config,
                                        ai_eval_breakdown_t *breakdown);

#if defined(AI_AGENT_HOST_TEST)

int16_t ai_agent_evaluate_internal(const board_t *board, player_t perspective, const ai_config_t *config,
                                   ai_eval_breakdown_t *breakdown) {
    return FAR9_ai_agent_evaluate_internal(board, perspective, config, breakdown);
}

#else

#pragma clang optimize off
__attribute__((noinline))

int16_t
ai_agent_evaluate_internal(const board_t *board, player_t perspective, const ai_config_t *config,
                           ai_eval_breakdown_t *breakdown) {
    int16_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_agent_evaluate_internal(board, perspective, config, breakdown);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) int16_t FAR9_ai_agent_evaluate_internal(const board_t *board,
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

    if (ai_immediate_win_available(board, board->current_player, config->swap_rule)) {
        player_t winner = board->current_player;
        if (winner == perspective) {
            return AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
        } else {
            return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
        }
    }

    // Check for forcing moves if enabled
    if (config->enable_forcing_check && FAR9_ai_forcing_move_available(board, opponent, config->swap_rule)) {
        // Opponent has a forcing move, very bad for us
        return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF) + 1000;  // Heavy penalty
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

bool FAR10_ai_agent_find_best_move_impl(const board_t *board, const ai_config_t *config, move_t *out_move);

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

void ai_agent_set_progress_callback(ai_config_t *config, ai_progress_callback_t callback, void *user_data) {
    if (!config) {
        return;
    }
    config->progress_callback = callback;
    config->progress_user_data = user_data;
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


static uint8_t FAR10_ai_evaluate_moves(board_t *root, const ai_config_t *config, ai_evaluated_moves_t *evaluated,
                                 uint32_t *out_nodes);
#if defined(AI_AGENT_HOST_TEST)

static uint8_t ai_evaluate_moves(board_t *root, const ai_config_t *config, ai_evaluated_moves_t *evaluated,
                                 uint32_t *out_nodes) {
    return FAR10_ai_evaluate_moves(root, config, evaluated, out_nodes);
}

#else

#pragma clang optimize off
__attribute__((noinline)) static uint8_t ai_evaluate_moves(board_t *root, const ai_config_t *config, ai_evaluated_moves_t *evaluated,
                                 uint32_t *out_nodes) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    bool ret;
    *(volatile unsigned char *)0x000d = 10;
    ret = FAR10_ai_evaluate_moves(root, config, evaluated, out_nodes);
    *(volatile unsigned char *)0x000d = ___mmu;
    return ret;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block10"))) static uint8_t FAR10_ai_evaluate_moves(board_t *root, const ai_config_t *config, ai_evaluated_moves_t *evaluated,
                                 uint32_t *out_nodes) {
    if (!root || !config || !evaluated) {
        return 0u;
    }

    ai_ordered_moves_t ordered;
    uint8_t generated = FAR10_ai_generate_moves(root, config, &ordered);
    uint8_t count = 0u;
    player_t opponent = (config->ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // First pass: identify immediate wins to optimize evaluation
    bool has_immediate_wins = false;
    for (uint8_t i = 0u; i < generated && count < AI_MAX_ORDERED_MOVES; ++i) {
        move_t move = {
            .from_row = ordered.moves.from_rows[i],
            .from_col = ordered.moves.from_cols[i],
            .to_row = ordered.moves.to_rows[i],
            .to_col = ordered.moves.to_cols[i],
            .type = ordered.moves.types[i],
            .player = ordered.moves.players[i]
        };
        board_t child;
        ai_board_copy(&child, root);
        if (!board_execute_move_without_history(&child, &move, config->swap_rule)) {
            continue;
        }

        evaluated->moves.from_rows[count] = move.from_row;
        evaluated->moves.from_cols[count] = move.from_col;
        evaluated->moves.to_rows[count] = move.to_row;
        evaluated->moves.to_cols[count] = move.to_col;
        evaluated->moves.types[count] = move.type;
        evaluated->moves.players[count] = move.player;
        evaluated->immediate_wins_self[count] = board_check_win_fast(&child, config->ai_player);
        evaluated->immediate_wins_opponent[count] = board_check_win_fast(&child, opponent);

        if (evaluated->immediate_wins_self[count]) {
            has_immediate_wins = true;
        }

        ++count;
    }

    // Second pass: evaluate moves (only immediate wins if any exist, otherwise all)
    uint8_t eval_count = 0u;
    for (uint8_t i = 0u; i < count; ++i) {
        // Skip evaluation of non-immediate wins if immediate wins exist
        if (has_immediate_wins && !evaluated->immediate_wins_self[i]) {
            // Set a neutral evaluation for non-immediate wins when immediate wins exist
            evaluated->evaluations[i] = 0;
            evaluated->opponent_wins_next_move[i] = false;
            evaluated->opponent_forced_wins[i] = false;
            continue;
        }

        board_t child;
        ai_board_copy(&child, root);
        move_t move = {
            .from_row = evaluated->moves.from_rows[i],
            .from_col = evaluated->moves.from_cols[i],
            .to_row = evaluated->moves.to_rows[i],
            .to_col = evaluated->moves.to_cols[i],
            .type = evaluated->moves.types[i],
            .player = evaluated->moves.players[i]
        };
        if (!board_execute_move_without_history(&child, &move, config->swap_rule)) {
            continue;  // Should not happen
        }

        // Check if this move creates a forced win for the AI player (FORCED_WIN_B)
        evaluated->forced_wins_self[i] = false;
        if (config->difficulty == AI_DIFFICULTY_EXPERT && config->enable_forcing_check && !evaluated->immediate_wins_self[i]) {
            evaluated->forced_wins_self[i] = ai_forcing_move_available(&child, config->ai_player, config->swap_rule);
        }

        board_switch_turn(&child);
        child.current_player = opponent;

        evaluated->opponent_wins_next_move[i] = ai_immediate_win_available(&child, opponent, config->swap_rule);
        evaluated->opponent_forced_wins[i] = false;
        if (config->enable_forcing_check && !evaluated->opponent_wins_next_move[i]) {
            evaluated->opponent_forced_wins[i] = ai_forcing_move_available(&child, opponent, config->swap_rule);
        }

        evaluated->evaluations[i] = ai_agent_evaluate_internal(&child, config->ai_player, config, NULL);

        ++eval_count;
        if (out_nodes) {
            ++(*out_nodes);
        }
    }

    return count;
}

// Evaluate a single move and fill the ai_evaluated_move_t struct
// This is used for debugging and testing individual moves
void ai_evaluate_single_move(const board_t *board, const move_t *move, const ai_config_t *config, ai_evaluated_move_t *result) {
    if (!board || !move || !config || !result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->move = *move;

    player_t opponent = (config->ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Create child board after the move
    board_t child;
    ai_board_copy(&child, board);
    if (!board_execute_move_without_history(&child, move, config->swap_rule)) {
        return;  // Invalid move
    }

    // Check immediate wins
    result->immediate_win_self = board_check_win_fast(&child, config->ai_player);
    result->immediate_win_opponent = board_check_win_fast(&child, opponent);

    // Check forced win (only for EXPERT and if not immediate win)
    result->forced_win_self = false;
    if (config->difficulty == AI_DIFFICULTY_EXPERT && config->enable_forcing_check && !result->immediate_win_self) {
        result->forced_win_self = ai_forcing_move_available(&child, config->ai_player, config->swap_rule);
    }

    // Switch to opponent's turn for further evaluation
    board_switch_turn(&child);
    child.current_player = opponent;

    // Check if opponent can win immediately
    result->opponent_win_next_move = ai_immediate_win_available(&child, opponent, config->swap_rule);

    // Check if opponent has forced win
    result->opponent_forced_win = false;
    if (config->enable_forcing_check && !result->opponent_win_next_move) {
        result->opponent_forced_win = ai_forcing_move_available(&child, opponent, config->swap_rule);
    }

    // Evaluate the position
    result->evaluation = ai_agent_evaluate_internal(&child, config->ai_player, config, NULL);
}



static bool FAR10_ai_choose_move_from_evaluated(const ai_config_t *config,
                                          const ai_evaluated_moves_t *evaluated,
                                          uint8_t count,
                                          move_t *out_move,
                                          bool *applied_blunder);
#if defined(AI_AGENT_HOST_TEST)

static bool ai_choose_move_from_evaluated(const ai_config_t *config,
                                          const ai_evaluated_moves_t *evaluated,
                                          uint8_t count,
                                          move_t *out_move,
                                          bool *applied_blunder) {
    return FAR10_ai_choose_move_from_evaluated(config, evaluated, count, out_move, applied_blunder);
}

#else

#pragma clang optimize off
__attribute__((noinline)) static bool ai_choose_move_from_evaluated(const ai_config_t *config,
                                          const ai_evaluated_moves_t *evaluated,
                                          uint8_t count,
                                          move_t *out_move,
                                          bool *applied_blunder) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    bool ret;
    *(volatile unsigned char *)0x000d = 10;
    ret = FAR10_ai_choose_move_from_evaluated(config, evaluated, count, out_move, applied_blunder);
    *(volatile unsigned char *)0x000d = ___mmu;
    return ret;
}
#pragma clang optimize on

#endif


__attribute__((noinline, section(".block10"))) static bool FAR10_ai_choose_move_from_evaluated(const ai_config_t *config,
                                          const ai_evaluated_moves_t *evaluated,
                                          uint8_t count,
                                          move_t *out_move,
                                          bool *applied_blunder) {
    if (!config || !evaluated || !out_move || count == 0u) {
        return false;
    }

    if (applied_blunder) {
        *applied_blunder = false;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        if (evaluated->immediate_wins_self[i]) {
            *out_move = (move_t){
                .from_row = evaluated->moves.from_rows[i],
                .from_col = evaluated->moves.from_cols[i],
                .to_row = evaluated->moves.to_rows[i],
                .to_col = evaluated->moves.to_cols[i],
                .type = evaluated->moves.types[i],
                .player = evaluated->moves.players[i]
            };
            return true;
        }
    }

    // Check for FORCED_WIN_B moves (EXPERT only)
    if (config->difficulty == AI_DIFFICULTY_EXPERT) {
        for (uint8_t i = 0u; i < count; ++i) {
            if (evaluated->forced_wins_self[i]) {
                *out_move = (move_t){
                    .from_row = evaluated->moves.from_rows[i],
                    .from_col = evaluated->moves.from_cols[i],
                    .to_row = evaluated->moves.to_rows[i],
                    .to_col = evaluated->moves.to_cols[i],
                    .type = evaluated->moves.types[i],
                    .player = evaluated->moves.players[i]
                };
                return true;
            }
        }
    }

    uint8_t safe_indices[AI_MAX_ORDERED_MOVES];
    uint8_t safe_count = 0u;
    uint8_t blunder_indices[AI_MAX_ORDERED_MOVES];
    uint8_t blunder_count = 0u;

    bool blunder_enabled = config->blunder_enabled && config->blunder_chance_pct > 0u &&
                           config->difficulty != AI_DIFFICULTY_EXPERT && config->blunder_type != AI_BLUNDER_NONE;
    bool use_hint = config->use_hint_profile;

    for (uint8_t i = 0u; i < count; ++i) {
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
            *out_move = (move_t){
                .from_row = evaluated->moves.from_rows[blunder_idx],
                .from_col = evaluated->moves.from_cols[blunder_idx],
                .to_row = evaluated->moves.to_rows[blunder_idx],
                .to_col = evaluated->moves.to_cols[blunder_idx],
                .type = evaluated->moves.types[blunder_idx],
                .player = evaluated->moves.players[blunder_idx]
            };
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

        *out_move = (move_t){
            .from_row = evaluated->moves.from_rows[chosen_index],
            .from_col = evaluated->moves.from_cols[chosen_index],
            .to_row = evaluated->moves.to_rows[chosen_index],
            .to_col = evaluated->moves.to_cols[chosen_index],
            .type = evaluated->moves.types[chosen_index],
            .player = evaluated->moves.players[chosen_index]
        };
        return true;
    }

    if (blunder_enabled && blunder_count > 0u && ai_random_chance(config->blunder_chance_pct)) {
        uint8_t choice = (uint8_t)ai_random_range(blunder_count);
        uint8_t blunder_idx = blunder_indices[choice];
        *out_move = (move_t){
            .from_row = evaluated->moves.from_rows[blunder_idx],
            .from_col = evaluated->moves.from_cols[blunder_idx],
            .to_row = evaluated->moves.to_rows[blunder_idx],
            .to_col = evaluated->moves.to_cols[blunder_idx],
            .type = evaluated->moves.types[blunder_idx],
            .player = evaluated->moves.players[blunder_idx]
        };
        if (applied_blunder) {
            *applied_blunder = true;
        }
        return true;
    }

    uint8_t fallback = (uint8_t)ai_random_range(count);
    *out_move = (move_t){
        .from_row = evaluated->moves.from_rows[fallback],
        .from_col = evaluated->moves.from_cols[fallback],
        .to_row = evaluated->moves.to_rows[fallback],
        .to_col = evaluated->moves.to_cols[fallback],
        .type = evaluated->moves.types[fallback],
        .player = evaluated->moves.players[fallback]
    };
    return true;
}

bool FAR10_ai_agent_find_best_move_impl(const board_t *board, const ai_config_t *config, move_t *out_move);

#if defined(AI_AGENT_HOST_TEST)

bool ai_agent_find_best_move_impl(const board_t *board, const ai_config_t *config, move_t *out_move) {
    return FAR10_ai_agent_find_best_move_impl(board, config, out_move);
}

#else

#pragma clang optimize off
__attribute__((noinline)) bool ai_agent_find_best_move_impl(const board_t *board, const ai_config_t *config,
                                                            move_t *out_move) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    bool ret;
    *(volatile unsigned char *)0x000d = 10;
    ret = FAR10_ai_agent_find_best_move_impl(board, config, out_move);
    *(volatile unsigned char *)0x000d = ___mmu;
    return ret;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block10"))) bool FAR10_ai_agent_find_best_move_impl(const board_t *board,
                                                                                       const ai_config_t *config,
                                                                                       move_t *out_move) {
    if (!board || !config || !out_move) {
        return false;
    }

    board_t root;
    ai_board_copy(&root, board);
    root.current_player = board->current_player;

    ai_timer0_reset();

    ai_config_t tuned = *config;
    tuned.ai_player = root.current_player;

    ai_evaluated_moves_t evaluated;
    uint32_t nodes_recorded = 0u;
    uint8_t evaluated_count = FAR10_ai_evaluate_moves(&root, &tuned, &evaluated, &nodes_recorded);

    bool applied_blunder = false;
    move_t chosen_move = {0};
    bool move_found = FAR10_ai_choose_move_from_evaluated(&tuned, &evaluated, evaluated_count, &chosen_move, &applied_blunder);

    if (!move_found) {
        ai_ordered_moves_t fallback_moves;
        uint8_t fallback_count = FAR10_ai_generate_moves(&root, &tuned, &fallback_moves);
        if (fallback_count > 0u) {
            uint8_t idx = (uint8_t)ai_random_range(fallback_count);
            chosen_move = (move_t){
                .from_row = fallback_moves.moves.from_rows[idx],
                .from_col = fallback_moves.moves.from_cols[idx],
                .to_row = fallback_moves.moves.to_rows[idx],
                .to_col = fallback_moves.moves.to_cols[idx],
                .type = fallback_moves.moves.types[idx],
                .player = fallback_moves.moves.players[idx]
            };
            move_found = true;
        }
    }

    uint32_t elapsed_ticks = ai_timer0_read();
    ai_print_diagnostics(nodes_recorded, elapsed_ticks, config->diagnostics_enabled);

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
        analysed.current_player = tuned.ai_player;
    if (board_execute_move_without_history(&analysed, out_move, tuned.swap_rule)) {
            board_switch_turn(&analysed);
            ai_eval_breakdown_t breakdown;
            ai_agent_evaluate_internal(&analysed, tuned.ai_player, &tuned, &breakdown);
            s_last_breakdown = breakdown;
        } else {
            s_last_breakdown = (ai_eval_breakdown_t){0};
        }
    } else {
        s_last_breakdown = (ai_eval_breakdown_t){0};
    }

    return true;
}

bool ai_agent_find_best_move(const board_t *board, const ai_config_t *config, move_t *out_move) {
    if (!board || !config || !out_move) {
        return false;
    }
    return ai_agent_find_best_move_impl(board, config, out_move);
}

int16_t ai_agent_evaluate_board(const board_t *board, player_t player, const ai_config_t *config) {
    if (!board || !config) {
        return 0;
    }

    return ai_agent_evaluate_internal(board, player, config, NULL);
}

void ai_agent_get_last_breakdown(ai_eval_breakdown_t *out) {
    if (!out) {
        return;
    }
    *out = s_last_breakdown;
}
