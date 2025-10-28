/**
 * @file ai_agent.c
 * @brief Specification-compliant heuristic AI agent for F256 Switcharoo.
 */
#include "../src/ai_agent.h"

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

#define T0_PEND 0xD660
#define T0_MASK 0xD66C

#define T0_CTR \
    0xD650  // master control register for timer0, write.b0=ticks b1=reset b2=set
            // to last value of VAL b3=set count up, clear count down
#define T0_STAT \
    0xD650  // master control register for timer0, read bit0 set = reached target
            // val

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
#define AI_HINT_GOAL_WEIGHT 180
#define AI_MAX_ORDERED_MOVES 32

#define AI_HINT_TRACE_CAPACITY 64

static uint32_t s_ai_rng_state = 0xC0FFEEu;

static void ai_random_seed_internal(uint32_t seed) {
    if (seed == 0u) {
        seed = 1u;
    }
    s_ai_rng_state = seed;
}

void ai_agent_set_random_seed(uint32_t seed) {
    ai_random_seed_internal(seed);
}

static uint32_t ai_random_next(void) {
    s_ai_rng_state = s_ai_rng_state * 1664525u + 1013904223u;
    return s_ai_rng_state;
}

static uint32_t ai_random_range(uint32_t limit) {
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

typedef struct {
    int16_t swap;
    int16_t block;
    int16_t goal;
    int16_t total;
} ai_hint_eval_components_t;

typedef struct {
    ai_hint_eval_record_t entries[AI_HINT_TRACE_CAPACITY];
    uint8_t count;
    bool enabled;
} ai_hint_trace_state_t;

static ai_hint_trace_state_t s_hint_trace;

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
    move_t move;
    int16_t evaluation;
    bool immediate_win_self;
    bool immediate_win_opponent;
    bool opponent_win_next_move;
    bool opponent_forced_win;
} ai_evaluated_move_t;

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

void ai_board_copy(board_t *dest, const board_t *src) {
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
            piece_type_t piece = board_get_piece(board, row, col);
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

static void ai_hint_trace_record(ai_hint_trace_state_t *trace, const ai_hint_eval_components_t *components,
                                 const board_t *board, player_t perspective, uint8_t depth, uint8_t ply) {
    if (!trace || !trace->enabled || !components) {
        return;
    }
    if (trace->count >= AI_HINT_TRACE_CAPACITY) {
        return;
    }
    ai_hint_eval_record_t *entry = &trace->entries[trace->count++];
    entry->board_hash = ai_hash_board(board);
    entry->perspective = perspective;
    entry->total = components->total;
    entry->swap_contrib = components->swap;
    entry->block_contrib = components->block;
    entry->goal_contrib = components->goal;
    entry->depth = depth;
    entry->ply = ply;
}

static uint8_t ai_count_goal_rows_for_player(const board_t *board, player_t player) {
    uint8_t rows = 0;
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        bool has_piece = false;
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
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
            if (board_get_piece(board, row, col) == swapped) {
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

    piece_type_t moving_piece = board_get_piece(board, move->from_row, move->from_col);
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

void FAR8_ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out);

#if defined(AI_AGENT_HOST_TEST)

void ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out) {
    FAR8_ai_compute_connection_metrics(board, player, out);
}

#else

#pragma clang optimize off
__attribute__((noinline))

void
ai_compute_connection_metrics(const board_t *board, player_t player,
                              ai_connection_metrics_t *out)
{
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_ai_compute_connection_metrics(board, player, out);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8"))) void FAR8_ai_compute_connection_metrics(const board_t *board,
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
            piece_type_t piece = board_get_piece(board, row, col);
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
                    piece_type_t neighbor = board_get_piece(board, (uint8_t)new_row, (uint8_t)new_col);
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

uint8_t FAR8_ai_count_bridge_potential(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_count_bridge_potential(const board_t *board, player_t player) {
    return FAR8_ai_count_bridge_potential(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_count_bridge_potential(const board_t *board, player_t player) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    return_value = FAR8_ai_count_bridge_potential(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8"))) uint8_t FAR8_ai_count_bridge_potential(const board_t *board,
                                                                                     player_t player) {
    uint8_t bridges = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            if (board_get_piece(board, row, col) != PIECE_NONE) {
                continue;
            }
            uint8_t friendly = 0;
            uint8_t directional_pairs = 0;
            for (uint8_t dir = 0; dir < 8; ++dir) {
                int8_t new_row = (int8_t)row + kAdjRow[dir];
                int8_t new_col = (int8_t)col + kAdjCol[dir];
                if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                    piece_type_t neighbor = board_get_piece(board, (uint8_t)new_row, (uint8_t)new_col);
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

uint8_t FAR8_ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    return FAR8_ai_measure_swap_pressure(board, player, rule);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    return_value = FAR8_ai_measure_swap_pressure(board, player, rule);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8")))

uint8_t
FAR8_ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
    (void)rule;
    int16_t pressure = 0;
    piece_type_t swapped = (player == PLAYER_WHITE) ? PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
    piece_type_t friendly_normal = (player == PLAYER_WHITE) ? PIECE_WHITE_NORMAL : PIECE_BLACK_NORMAL;
    piece_type_t opponent_normal = (player == PLAYER_WHITE) ? PIECE_BLACK_NORMAL : PIECE_WHITE_NORMAL;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (piece == swapped) {
                pressure += 3;
                bool has_support = false;
                for (uint8_t dir = 0; dir < 8; ++dir) {
                    int8_t new_row = (int8_t)row + kAdjRow[dir];
                    int8_t new_col = (int8_t)col + kAdjCol[dir];
                    if (new_row >= 0 && new_row < BOARD_ROWS && new_col >= 0 && new_col < BOARD_COLS) {
                        piece_type_t neighbor = board_get_piece(board, (uint8_t)new_row, (uint8_t)new_col);
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
                        piece_type_t neighbor = board_get_piece(board, (uint8_t)new_row, (uint8_t)new_col);
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

uint8_t FAR8_ai_measure_blocking(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_measure_blocking(const board_t *board, player_t player) {
    return FAR8_ai_measure_blocking(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_measure_blocking(const board_t *board, player_t player) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    return_value = FAR8_ai_measure_blocking(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8")))

uint8_t
FAR8_ai_measure_blocking(const board_t *board, player_t player) {
    player_t opponent = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    uint8_t blocking = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
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
                    piece_type_t neighbor = board_get_piece(board, (uint8_t)new_row, (uint8_t)new_col);
                    if (board_get_piece_owner(neighbor) == opponent) {
                        blocking++;
                    }
                }
            }
        }
    }
    return blocking;
}

uint16_t FAR8_ai_measure_mobility(const board_t *board, player_t player);

#if defined(AI_AGENT_HOST_TEST)

uint16_t ai_measure_mobility(const board_t *board, player_t player) {
    return FAR8_ai_measure_mobility(board, player);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint16_t
ai_measure_mobility(const board_t *board, player_t player) {
    uint16_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    return_value = FAR8_ai_measure_mobility(board, player);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8")))

uint16_t
FAR8_ai_measure_mobility(const board_t *board, player_t player) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = player;

    uint16_t mobility = 0;
    move_t moves[8];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            mobility += board_get_legal_moves(&scratch, row, col, moves, 8);
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

bool FAR8_ai_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule);
#if defined(AI_AGENT_HOST_TEST)

bool ai_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule) {
    return FAR8_ai_immediate_win_available(board, player, rule);
}

#else

#pragma clang optimize off
__attribute__((noinline)) bool ai_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule) {
    bool return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    return_value = FAR8_ai_immediate_win_available(board, player, rule);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8"))) bool FAR8_ai_immediate_win_available(const board_t *board,
                                                                                   player_t player, swap_rule_t rule) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = player;

    move_t moves[AI_MAX_ORDERED_MOVES];
    uint8_t count = 0;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            count += board_get_legal_moves(&scratch, row, col, &moves[count], (uint8_t)(AI_MAX_ORDERED_MOVES - count));
        }
    }

    for (uint8_t i = 0; i < count; ++i) {
        board_t test;
        ai_board_copy(&test, &scratch);
        if (!board_execute_move(&test, &moves[i], rule)) {
            continue;
        }
        if (board_check_win_fast(&test, player)) {
            return true;
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
    if (!board_execute_move(&after_ai, move, config->swap_rule)) {
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

    move_t opponent_moves[AI_MAX_ORDERED_MOVES];
    uint8_t opponent_count = 0;
    const uint8_t buffer_capacity = (uint8_t)(sizeof(opponent_moves) / sizeof(opponent_moves[0]));

    for (uint8_t row = 0; row < BOARD_ROWS && opponent_count < buffer_capacity; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS && opponent_count < buffer_capacity; ++col) {
            piece_type_t piece = board_get_piece(&opponent_state, row, col);
            if (board_get_piece_owner(piece) != opponent) {
                continue;
            }

            opponent_count += board_get_legal_moves(&opponent_state, row, col, &opponent_moves[opponent_count],
                                                    (uint8_t)(buffer_capacity - opponent_count));
        }
    }

    if (opponent_count == 0u) {
        return true;
    }

    for (uint8_t i = 0; i < opponent_count; ++i) {
        board_t after_opponent;
        ai_board_copy(&after_opponent, &opponent_state);
        if (!board_execute_move(&after_opponent, &opponent_moves[i], config->swap_rule)) {
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
    if (!board_execute_move(&after_ai, move, config->swap_rule)) {
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

    move_t candidate_moves[AI_MAX_ORDERED_MOVES];
    uint8_t move_count = 0u;

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != to_move) {
                continue;
            }
            move_count += board_get_legal_moves(&scratch,
                                                row,
                                                col,
                                                &candidate_moves[move_count],
                                                (uint8_t)(AI_MAX_ORDERED_MOVES - move_count));
            if (move_count >= AI_MAX_ORDERED_MOVES) {
                move_count = AI_MAX_ORDERED_MOVES;
                break;
            }
        }
        if (move_count >= AI_MAX_ORDERED_MOVES) {
            break;
        }
    }

    if (move_count == 0u) {
        return false;
    }

    for (uint8_t i = 0; i < move_count; ++i) {
        board_t after_move;
        ai_board_copy(&after_move, &scratch);
        if (!board_execute_move(&after_move, &candidate_moves[i], config->swap_rule)) {
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
    player_t ai_player = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    board_t base_state;
    ai_board_copy(&base_state, board);
    base_state.current_player = player;

    move_t candidate_moves[AI_MAX_ORDERED_MOVES];
    move_t response_moves[AI_MAX_ORDERED_MOVES];
    const uint8_t buffer_capacity = (uint8_t)(sizeof(candidate_moves) / sizeof(candidate_moves[0]));

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&base_state, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            uint8_t move_count = board_get_legal_moves(&base_state, row, col, candidate_moves, buffer_capacity);
            for (uint8_t move_idx = 0; move_idx < move_count; ++move_idx) {
                board_t after_opponent;
                ai_board_copy(&after_opponent, &base_state);
                if (!board_execute_move(&after_opponent, &candidate_moves[move_idx], rule)) {
                    continue;
                }

                if (board_check_win_fast(&after_opponent, player)) {
                    return true;
                }

                board_switch_turn(&after_opponent);
                after_opponent.current_player = ai_player;

                bool defender_has_safe_move = false;

                for (uint8_t resp_row = 0; resp_row < BOARD_ROWS && !defender_has_safe_move; ++resp_row) {
                    for (uint8_t resp_col = 0; resp_col < BOARD_COLS && !defender_has_safe_move; ++resp_col) {
                        piece_type_t resp_piece = board_get_piece(&after_opponent, resp_row, resp_col);
                        if (board_get_piece_owner(resp_piece) != ai_player) {
                            continue;
                        }

                        uint8_t response_count =
                            board_get_legal_moves(&after_opponent, resp_row, resp_col, response_moves, buffer_capacity);
                        for (uint8_t resp_idx = 0; resp_idx < response_count; ++resp_idx) {
                            board_t after_ai;
                            ai_board_copy(&after_ai, &after_opponent);
                            if (!board_execute_move(&after_ai, &response_moves[resp_idx], rule)) {
                                continue;
                            }

                            if (board_check_win_fast(&after_ai, player)) {
                                continue;
                            }

                            board_switch_turn(&after_ai);
                            after_ai.current_player = player;

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
uint8_t FAR9_ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_move_t *out_moves);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_move_t *out_moves) {
    return FAR9_ai_generate_moves(board, config, out_moves);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_generate_moves(const board_t *board, const ai_config_t *config, ai_ordered_move_t *out_moves) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_generate_moves(board, config, out_moves);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) uint8_t FAR9_ai_generate_moves(const board_t *board,
                                                                             const ai_config_t *config,
                                                                             ai_ordered_move_t *out_moves) {
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

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t moving_piece = scratch.cells[row][col].piece;
            if ((player_t)kPieceOwnerLUT[moving_piece] != current) {
                continue;
            }

            bool mover_swapped = board_is_piece_swapped(moving_piece);

            for (uint8_t dir = 0; dir < 8; ++dir) {
                if (count >= AI_MAX_ORDERED_MOVES) {
                    goto finalize_moves;
                }

                int8_t next_row = (int8_t)row + kAdjRow[dir];
                int8_t next_col = (int8_t)col + kAdjCol[dir];
                if (next_row < 0 || next_row >= BOARD_ROWS || next_col < 0 || next_col >= BOARD_COLS) {
                    continue;
                }

                piece_type_t target_piece = scratch.cells[(uint8_t)next_row][(uint8_t)next_col].piece;
                move_type_t move_type;
                if (target_piece == PIECE_NONE) {
                    move_type = MOVE_TYPE_EMPTY;
                } else {
                    player_t target_owner = (player_t)kPieceOwnerLUT[target_piece];
                    if (target_owner == current || target_owner == PLAYER_NONE) {
                        continue;
                    }
                    if (!kPieceIsNormalLUT[target_piece]) {
                        continue;
                    }
                    move_type = MOVE_TYPE_SWAP;
                }

                ai_ordered_move_t *ordered = &out_moves[count];
                move_t *move = &ordered->move;
                move->from_row = row;
                move->from_col = col;
                move->to_row = (uint8_t)next_row;
                move->to_col = (uint8_t)next_col;
                move->type = move_type;
                move->player = current;

                int16_t score = 0;
                bool clears_swapped = ai_move_clears_swapped(&scratch, move, swap_rule);

                if (move_type == MOVE_TYPE_SWAP) {
                    score += 900;
                    if (!mover_swapped) {
                        score += 500;
                    } else {
                        score += 220;
                    }
                    if (move->to_col == 1 || move->to_col == 2) {
                        score += 150;
                    }
                } else {
                    if (clears_swapped) {
                        score -= 1400;
                    } else if (mover_swapped) {
                        score += 220;
                    }
                }
                if (move->to_col == 1 || move->to_col == 2) {
                    score += 350;
                }
                if (current == PLAYER_WHITE) {
                    if (move->to_row < move->from_row) {
                        score += 280;
                    }
                } else {
                    if (move->to_row > move->from_row) {
                        score += 280;
                    }
                }
                if (move_type == MOVE_TYPE_EMPTY && move->to_row >= WIN_START_ROW && move->to_row <= WIN_END_ROW) {
                    score += 120;
                }

                if (forcing_check && ai_move_creates_forced_immediate_win(board, move, config, config->ai_player)) {
                    score += 8000;
                }

                if (config->ai_player == current &&
                    ai_move_allows_opponent_immediate_win(board, move, config, config->ai_player)) {
                    score -= 12000;
                }

                ordered->order_score = score;
                count++;
            }
        }
    }

finalize_moves:

    for (uint8_t i = 1; i < count; ++i) {
        ai_ordered_move_t key = out_moves[i];
        int16_t value = key.order_score;
        uint8_t j = i;
        while (j > 0 && out_moves[j - 1].order_score < value) {
            out_moves[j] = out_moves[j - 1];
            --j;
        }
        out_moves[j] = key;
    }

    return count;
}

int16_t FAR9_ai_agent_evaluate_internal(const board_t *board, player_t perspective, const ai_config_t *config,
                                        ai_eval_breakdown_t *breakdown);

static void ai_hint_components_set(ai_hint_eval_components_t *components, int16_t swap_contrib, int16_t block_contrib,
                                   int16_t goal_contrib, int16_t total) {
    if (!components) {
        return;
    }
    components->swap = swap_contrib;
    components->block = block_contrib;
    components->goal = goal_contrib;
    components->total = total;
}

static int16_t ai_agent_evaluate_hint(const board_t *board, player_t perspective, const ai_config_t *config,
                                      ai_hint_eval_components_t *components) {
    player_t opponent = (perspective == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    if (board_check_win_fast(board, perspective)) {
        int16_t score = AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
        ai_hint_components_set(components, 0, 0, 0, score);
        return score;
    }
    if (board_check_win_fast(board, opponent)) {
        int16_t score = AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
        ai_hint_components_set(components, 0, 0, 0, score);
        return score;
    }

    if (ai_immediate_win_available(board, board->current_player, config->swap_rule)) {
        player_t winner = board->current_player;
        if (winner == perspective) {
            int16_t score = AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
            ai_hint_components_set(components, 0, 0, 0, score);
            return score;
        } else {
            int16_t score = AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
            ai_hint_components_set(components, 0, 0, 0, score);
            return score;
        }
    }

    int16_t swap_me = (int16_t)ai_measure_swap_pressure(board, perspective, config->swap_rule);
    int16_t swap_op = (int16_t)ai_measure_swap_pressure(board, opponent, config->swap_rule);

    int16_t block_me = (int16_t)ai_measure_blocking(board, perspective);
    int16_t block_op = (int16_t)ai_measure_blocking(board, opponent);

    int16_t swap_diff = (int16_t)(swap_me - swap_op);
    int16_t block_diff = (int16_t)(block_me - block_op);

    int16_t swap_contrib = ai_clamp_score(ai_signed_multiply(config->weights.swap_pressure, swap_diff));
    int16_t block_contrib = ai_clamp_score(ai_signed_multiply(config->weights.blocking_coverage, block_diff));

    int16_t goal_delta = (int16_t)ai_count_goal_rows_for_player(board, perspective) -
                         (int16_t)ai_count_goal_rows_for_player(board, opponent);
    int16_t goal_contrib = (int16_t)ai_signed_multiply(goal_delta, AI_HINT_GOAL_WEIGHT);

    int32_t total = (int32_t)swap_contrib + (int32_t)block_contrib + (int32_t)goal_contrib;
    int16_t clamped = ai_clamp_score(total);
    ai_hint_components_set(components, swap_contrib, block_contrib, goal_contrib, clamped);
    return clamped;
}

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
    if (config->enable_forcing_check && ai_forcing_move_available(board, opponent, config->swap_rule)) {
        // Opponent has a forcing move, very bad for us
        return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF) + 1000;  // Heavy penalty
    }

    ai_connection_metrics_t conn_me;
    ai_connection_metrics_t conn_op;
    ai_compute_connection_metrics(board, perspective, &conn_me);
    ai_compute_connection_metrics(board, opponent, &conn_op);

    int16_t connection_me =
        (int16_t)(ai_popcount(conn_me.row_mask) * 12 + conn_me.row_links * 18 + conn_me.branching_nodes * 5);
    int16_t connection_op =
        (int16_t)(ai_popcount(conn_op.row_mask) * 12 + conn_op.row_links * 18 + conn_op.branching_nodes * 5);

    int16_t bridge_me = (int16_t)ai_count_bridge_potential(board, perspective);
    int16_t bridge_op = (int16_t)ai_count_bridge_potential(board, opponent);

    int16_t swap_me = (int16_t)ai_measure_swap_pressure(board, perspective, config->swap_rule);
    int16_t swap_op = (int16_t)ai_measure_swap_pressure(board, opponent, config->swap_rule);

    int16_t block_me = (int16_t)ai_measure_blocking(board, perspective);
    int16_t block_op = (int16_t)ai_measure_blocking(board, opponent);

    int16_t mobility_me = (int16_t)ai_measure_mobility(board, perspective);
    int16_t mobility_op = (int16_t)ai_measure_mobility(board, opponent);

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

static void ai_sort_indices_by_evaluation(const ai_evaluated_move_t *evaluated, uint8_t *indices, uint8_t count) {
    for (uint8_t i = 1u; i < count; ++i) {
        uint8_t key = indices[i];
        int16_t value = evaluated[key].evaluation;
        uint8_t j = i;
        while (j > 0u && evaluated[indices[j - 1u]].evaluation < value) {
            indices[j] = indices[j - 1u];
            --j;
        }
        indices[j] = key;
    }
}

static uint8_t ai_evaluate_moves(board_t *root, const ai_config_t *config, ai_evaluated_move_t *evaluated,
                                 uint32_t *out_nodes) {
    if (!root || !config || !evaluated) {
        return 0u;
    }

    ai_ordered_move_t ordered[AI_MAX_ORDERED_MOVES];
    uint8_t generated = ai_generate_moves(root, config, ordered);
    uint8_t count = 0u;
    player_t opponent = (config->ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    bool use_hint_eval = config->use_hint_profile;
    bool record_hint = use_hint_eval && s_hint_trace.enabled;

    for (uint8_t i = 0u; i < generated && count < AI_MAX_ORDERED_MOVES; ++i) {
        board_t child;
        ai_board_copy(&child, root);
        if (!board_execute_move(&child, &ordered[i].move, config->swap_rule)) {
            continue;
        }

        ai_evaluated_move_t *slot = &evaluated[count];
        slot->move = ordered[i].move;
        slot->immediate_win_self = board_check_win_fast(&child, config->ai_player);
        slot->immediate_win_opponent = board_check_win_fast(&child, opponent);

        board_switch_turn(&child);
        child.current_player = opponent;

        slot->opponent_win_next_move = ai_immediate_win_available(&child, opponent, config->swap_rule);
        slot->opponent_forced_win = false;
        if (config->enable_forcing_check && !slot->opponent_win_next_move) {
            slot->opponent_forced_win = ai_forcing_move_available(&child, opponent, config->swap_rule);
        }

        ai_hint_eval_components_t components;
        ai_hint_eval_components_t *components_ptr = record_hint ? &components : NULL;

        slot->evaluation = use_hint_eval
                               ? ai_agent_evaluate_hint(&child, config->ai_player, config, components_ptr)
                               : ai_agent_evaluate_internal(&child, config->ai_player, config, NULL);

        if (components_ptr) {
            ai_hint_trace_record(&s_hint_trace, components_ptr, &child, config->ai_player, 0u, 1u);
        }

        ++count;
        if (out_nodes) {
            ++(*out_nodes);
        }
    }

    return count;
}

static bool ai_choose_move_from_evaluated(const ai_config_t *config,
                                          const ai_evaluated_move_t *evaluated,
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
        if (evaluated[i].immediate_win_self) {
            *out_move = evaluated[i].move;
            return true;
        }
    }

    uint8_t safe_indices[AI_MAX_ORDERED_MOVES];
    uint8_t safe_count = 0u;
    uint8_t blunder_indices[AI_MAX_ORDERED_MOVES];
    uint8_t blunder_count = 0u;

    bool use_hint = config->use_hint_profile;
    bool blunder_enabled = !use_hint && config->blunder_enabled && config->blunder_chance_pct > 0u &&
                           config->difficulty != AI_DIFFICULTY_EXPERT && config->blunder_type != AI_BLUNDER_NONE;

    for (uint8_t i = 0u; i < count; ++i) {
        const ai_evaluated_move_t *move = &evaluated[i];
        if (move->immediate_win_opponent) {
            continue;
        }

        bool disqualify = false;
        bool eligible_blunder = false;

        if (move->opponent_win_next_move) {
            disqualify = true;
            if (blunder_enabled && config->blunder_type == AI_BLUNDER_ALLOW_IMMEDIATE_WIN) {
                eligible_blunder = true;
            }
        } else if (move->opponent_forced_win && config->enable_forcing_check) {
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
            if (evaluated[idx].evaluation > evaluated[best_index].evaluation) {
                best_index = idx;
            }
        }

        if (blunder_enabled && blunder_count > 0u && ai_random_chance(config->blunder_chance_pct)) {
            uint8_t choice = (uint8_t)ai_random_range(blunder_count);
            *out_move = evaluated[blunder_indices[choice]].move;
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

        *out_move = evaluated[chosen_index].move;
        return true;
    }

    if (blunder_enabled && blunder_count > 0u && ai_random_chance(config->blunder_chance_pct)) {
        uint8_t choice = (uint8_t)ai_random_range(blunder_count);
        *out_move = evaluated[blunder_indices[choice]].move;
        if (applied_blunder) {
            *applied_blunder = true;
        }
        return true;
    }

    uint8_t fallback = (uint8_t)ai_random_range(count);
    *out_move = evaluated[fallback].move;
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

    if (!board_has_legal_moves(&root, root.current_player)) {
        uint32_t ticks = ai_timer0_read();
        ai_print_diagnostics(0u, ticks, config->diagnostics_enabled);
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    ai_config_t tuned = *config;
    tuned.ai_player = root.current_player;

    if (s_hint_trace.enabled) {
        s_hint_trace.count = 0u;
    }

    ai_evaluated_move_t evaluated[AI_MAX_ORDERED_MOVES];
    uint32_t nodes_recorded = 0u;
    uint8_t evaluated_count = ai_evaluate_moves(&root, &tuned, evaluated, &nodes_recorded);

    bool applied_blunder = false;
    move_t chosen_move = {0};
    bool move_found = ai_choose_move_from_evaluated(&tuned, evaluated, evaluated_count, &chosen_move, &applied_blunder);

    if (!move_found) {
        ai_ordered_move_t fallback_moves[AI_MAX_ORDERED_MOVES];
        uint8_t fallback_count = ai_generate_moves(&root, &tuned, fallback_moves);
        if (fallback_count > 0u) {
            uint8_t idx = (uint8_t)ai_random_range(fallback_count);
            chosen_move = fallback_moves[idx].move;
            move_found = true;
        }
    }

    uint32_t elapsed_ticks = ai_timer0_read();
    ai_print_diagnostics(nodes_recorded, elapsed_ticks, config->diagnostics_enabled);

    if (!move_found) {
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    if (applied_blunder) {
        print_made_blunder();
    }

    *out_move = chosen_move;

    if (config->diagnostics_enabled) {
        board_t analysed;
        ai_board_copy(&analysed, board);
        analysed.current_player = tuned.ai_player;
        if (board_execute_move(&analysed, out_move, tuned.swap_rule)) {
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

void ai_agent_hint_trace_enable(bool enabled) {
    s_hint_trace.enabled = enabled;
    if (!enabled) {
        s_hint_trace.count = 0u;
    }
}

void ai_agent_hint_trace_clear(void) {
    s_hint_trace.count = 0u;
}

uint8_t ai_agent_hint_trace_get(const ai_hint_eval_record_t **out_records) {
    if (out_records) {
        *out_records = s_hint_trace.entries;
    }
    return s_hint_trace.count;
}
