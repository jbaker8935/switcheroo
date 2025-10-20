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
#define AI_NODE_LIMIT_FALLBACK 16000U
#define AI_HINT_GOAL_WEIGHT 180
#define AI_MAX_ORDERED_MOVES 32
#define AI_MAX_DEPTH 8
#define AI_KILLER_PER_PLY 2
#define AI_TRANSPOSITION_SIZE 16
#define AI_TRANSPOSITION_MASK (AI_TRANSPOSITION_SIZE - 1)

#define AI_HINT_TRACE_CAPACITY 64

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

typedef enum { TT_FLAG_NONE = 0, TT_FLAG_EXACT = 1, TT_FLAG_LOWER = 2, TT_FLAG_UPPER = 3 } ai_tt_flag_t;

typedef struct {
    move_t move;
    int16_t order_score;
} ai_ordered_move_t;

typedef struct {
    uint8_t row_mask;
    uint8_t row_links;
    uint8_t branching_nodes;
} ai_connection_metrics_t;

typedef struct {
    uint32_t key;
    int16_t score;
    uint8_t depth;
    uint8_t flag;
    move_t move;
} ai_tt_entry_t;

typedef struct {
    const ai_config_t *config;
    uint32_t node_limit;
    uint32_t nodes;
    bool abort;
    bool use_hint_eval;
    move_t pv[AI_MAX_DEPTH];
    uint8_t pv_length;
    move_t killer_moves[AI_MAX_DEPTH][AI_KILLER_PER_PLY];
    uint8_t killer_count[AI_MAX_DEPTH];
#ifdef AI_AGENT_ENABLE_TIMER
    clock_t deadline;
#endif
    ai_hint_trace_state_t *hint_trace;
    uint8_t trace_depth;
    uint8_t trace_ply;
} ai_search_context_t;

#define AI_PROGRESS_CALLBACK_PERIOD 4u

static uint16_t s_progress_throttle = 0u;
static ai_search_context_t *s_active_search_ctx = NULL;
static uint8_t s_active_search_depth_hint = 1u;

static void ai_emit_throttled_progress(ai_search_context_t *ctx, uint8_t depth_remaining) {
    if (!ctx || !ctx->config || !ctx->config->progress_callback) {
        return;
    }

    s_active_search_depth_hint = (depth_remaining > 0u) ? depth_remaining : 1u;

    ++s_progress_throttle;
    if (s_progress_throttle < AI_PROGRESS_CALLBACK_PERIOD) {
        return;
    }

    s_progress_throttle = 0u;

    const ai_search_settings_t *search = &ctx->config->search;
    uint8_t reported_depth = depth_remaining;
    if (search->max_depth > 0u && depth_remaining <= search->max_depth) {
        uint8_t computed = (uint8_t)(search->max_depth - depth_remaining + 1u);
        if (computed == 0u) {
            computed = 1u;
        }
        reported_depth = computed;
    } else if (reported_depth == 0u) {
        reported_depth = 1u;
    }

    ctx->config->progress_callback(reported_depth, ctx->nodes, ctx->config->progress_user_data);
}

static void ai_emit_progress_with_active_context(void) {
    if (!s_active_search_ctx) {
        return;
    }

    ai_emit_throttled_progress(s_active_search_ctx, s_active_search_depth_hint);
}

static bool s_zobrist_ready = false;
static uint16_t s_zobrist_board[BOARD_CELLS][5];
static uint16_t s_zobrist_player[2];
static ai_tt_entry_t s_tt[AI_TRANSPOSITION_SIZE];
static ai_eval_breakdown_t s_last_breakdown;

static const ai_eval_weights_t kRuleWeights[4] = {
    {88, 58, 36, 44, 12},  // Classic
    {84, 54, 32, 44, 12},  // Clears Own
    {72, 52, 50, 38, 16},  // Swapped Clears
    {72, 50, 46, 38, 16}   // Swapped Clears Own
};

#if defined(AI_AGENT_HOST_TEST)

void ai_board_copy(board_t *dest, const board_t *src) {
    memcpy(dest, src, sizeof(board_t));
}

#else

void FAR8_ai_board_copy(board_t *dest, const board_t *src);

#pragma clang optimize off
__attribute__((noinline))

void
ai_board_copy(board_t *dest, const board_t *src)
{
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_ai_board_copy(dest, src);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

__attribute__((noinline, section(".block8"))) void FAR8_ai_board_copy(board_t *dest, const board_t *src) {
    memcpy(dest, src, sizeof(board_t));
}

#endif

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

static void ai_tt_clear(void) {
    memset(s_tt, 0, sizeof(s_tt));
}

static ai_tt_entry_t *ai_tt_lookup(uint32_t key) {
    uint32_t idx = (key ^ (key >> 11)) & AI_TRANSPOSITION_MASK;
    return &s_tt[idx];
}

static bool ai_same_move(const move_t *a, const move_t *b) {
    return a->from_row == b->from_row && a->from_col == b->from_col && a->to_row == b->to_row &&
           a->to_col == b->to_col && a->type == b->type;
}

static void ai_tt_store(uint32_t key, uint8_t depth, ai_tt_flag_t flag, int16_t score, const move_t *move) {
    ai_tt_entry_t *entry = ai_tt_lookup(key);
    if (entry->key == key && entry->depth > depth) {
        return;  // Keep deeper information
    }

    entry->key = key;
    entry->score = score;
    entry->depth = depth;
    entry->flag = (uint8_t)flag;
    entry->move = *move;
}

static void ai_hint_trace_record(const ai_search_context_t *ctx, const ai_hint_eval_components_t *components,
                                 const board_t *board, player_t perspective) {
    if (!ctx || !ctx->hint_trace || !ctx->hint_trace->enabled || !components) {
        return;
    }
    ai_hint_trace_state_t *trace = ctx->hint_trace;
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
    entry->depth = ctx->trace_depth;
    entry->ply = ctx->trace_ply;
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

static uint8_t ai_goal_row_pressure(const board_t *board) {
    uint8_t white_rows = ai_count_goal_rows_for_player(board, PLAYER_WHITE);
    uint8_t black_rows = ai_count_goal_rows_for_player(board, PLAYER_BLACK);
    return (white_rows > black_rows) ? white_rows : black_rows;
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

static uint8_t ai_count_move_volume(const board_t *board, player_t player) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = player;

    uint8_t total = 0u;
    move_t buffer[8];
    const uint8_t buffer_capacity = (uint8_t)(sizeof(buffer) / sizeof(buffer[0]));

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            total += board_get_legal_moves(&scratch, row, col, buffer, buffer_capacity);
            if (total >= AI_MAX_ORDERED_MOVES) {
                return AI_MAX_ORDERED_MOVES;
            }
        }
    }

    return total;
}

static uint8_t ai_select_dynamic_depth(const ai_config_t *config, uint8_t pressure) {
    if (pressure <= 3u) {
        return 0u;
    }
    if (pressure == 4u) {
        uint8_t cap = (config->search.base_depth < 2u) ? config->search.base_depth : 2u;
        return (cap == 0u) ? 1u : cap;
    }
    return config->search.max_depth;
}

static uint32_t ai_select_node_cap(const ai_config_t *config, uint8_t pressure) {
    uint32_t limit = config->search.node_limit ? config->search.node_limit : AI_NODE_LIMIT_FALLBACK;
    if (pressure == 4u && limit > 4000u) {
        return 4000u;
    }
    return limit;
}

static void ai_store_killer(ai_search_context_t *ctx, uint8_t ply, const move_t *move) {
    if (!ctx->config->search.use_killer_moves || ply >= AI_MAX_DEPTH) {
        return;
    }

    for (uint8_t i = 0; i < ctx->killer_count[ply]; ++i) {
        if (ai_same_move(&ctx->killer_moves[ply][i], move)) {
            return;
        }
    }

    if (ctx->killer_count[ply] < AI_KILLER_PER_PLY) {
        ctx->killer_moves[ply][ctx->killer_count[ply]++] = *move;
    } else {
        ctx->killer_moves[ply][1] = ctx->killer_moves[ply][0];
        ctx->killer_moves[ply][0] = *move;
    }
}

static bool ai_is_killer(const ai_search_context_t *ctx, uint8_t ply, const move_t *move) {
    if (!ctx->config->search.use_killer_moves || ply >= AI_MAX_DEPTH) {
        return false;
    }
    for (uint8_t i = 0; i < ctx->killer_count[ply]; ++i) {
        if (ai_same_move(&ctx->killer_moves[ply][i], move)) {
            return true;
        }
    }
    return false;
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
    ai_emit_progress_with_active_context();

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
        ai_emit_progress_with_active_context();
        board_t test;
        ai_board_copy(&test, &scratch);
        if (!board_execute_move(&test, &moves[i], rule)) {
            continue;
        }
        if (board_check_win(&test, player, NULL)) {
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

    if (board_check_win(&after_ai, ai_player, NULL)) {
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

        if (board_check_win(&after_opponent, opponent, NULL)) {
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
    *(volatile unsigned char *)0x000d = 8;
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

                if (board_check_win(&after_opponent, player, NULL)) {
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

                            if (board_check_win(&after_ai, player, NULL)) {
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

static bool ai_has_tactical_threat(const board_t *board, swap_rule_t rule) {
    player_t to_move = board->current_player;
    player_t opponent = (to_move == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    if (ai_immediate_win_available(board, to_move, rule)) {
        return true;
    }

    if (ai_immediate_win_available(board, opponent, rule)) {
        return true;
    }

    return false;
}

uint8_t FAR9_ai_generate_moves(const board_t *board, const ai_search_context_t *ctx, ai_ordered_move_t *out_moves,
                               uint8_t ply);

#if defined(AI_AGENT_HOST_TEST)

uint8_t ai_generate_moves(const board_t *board, const ai_search_context_t *ctx, ai_ordered_move_t *out_moves,
                          uint8_t ply) {
    return FAR9_ai_generate_moves(board, ctx, out_moves, ply);
}

#else

#pragma clang optimize off
__attribute__((noinline))

uint8_t
ai_generate_moves(const board_t *board, const ai_search_context_t *ctx, ai_ordered_move_t *out_moves, uint8_t ply) {
    uint8_t return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_generate_moves(board, ctx, out_moves, ply);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) uint8_t FAR9_ai_generate_moves(const board_t *board,
                                                                             const ai_search_context_t *ctx,
                                                                             ai_ordered_move_t *out_moves,
                                                                             uint8_t ply) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = board->current_player;

    uint8_t count = 0;
    const player_t current = scratch.current_player;

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
                bool clears_swapped = ai_move_clears_swapped(&scratch, move, ctx->config->swap_rule);

                if (ai_is_killer(ctx, ply, move)) {
                    score += 4000;
                }
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

                if (ctx && ctx->config && ctx->config->enable_forcing_check &&
                    ctx->config->ai_player == current &&
                    ai_move_creates_forced_immediate_win(board, move, ctx->config, ctx->config->ai_player)) {
                    score += 8000;
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

    if (board_check_win(board, perspective, NULL)) {
        int16_t score = AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
        ai_hint_components_set(components, 0, 0, 0, score);
        return score;
    }
    if (board_check_win(board, opponent, NULL)) {
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

    if (board_check_win(board, perspective, NULL)) {
        return AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
    }
    if (board_check_win(board, opponent, NULL)) {
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

bool FAR9_ai_select_move_heuristic(board_t *root, const ai_config_t *config, move_t *out_move, uint32_t *out_nodes);

#if defined(AI_AGENT_HOST_TEST)

bool ai_select_move_heuristic(board_t *root, const ai_config_t *config, move_t *out_move, uint32_t *out_nodes) {
    return FAR9_ai_select_move_heuristic(root, config, out_move, out_nodes);
}

#else

#pragma clang optimize off
__attribute__((noinline))

bool
ai_select_move_heuristic(board_t *root, const ai_config_t *config,
                         move_t *out_move,
                         uint32_t *out_nodes)
{
    bool return_value;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 9;
    return_value = FAR9_ai_select_move_heuristic(root, config, out_move, out_nodes);
    *(volatile unsigned char *)0x000d = ___mmu;
    return return_value;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block9"))) bool FAR9_ai_select_move_heuristic(board_t *root,
                                                                                 const ai_config_t *config,
                                                                                 move_t *out_move,
                                                                                 uint32_t *out_nodes) {
    ai_ordered_move_t moves[AI_MAX_ORDERED_MOVES];
    ai_search_context_t stub;
    memset(&stub, 0, sizeof(stub));
    stub.config = config;
    stub.use_hint_eval = config->use_hint_profile;
    if (s_hint_trace.enabled) {
        stub.hint_trace = &s_hint_trace;
    }

    uint8_t count = ai_generate_moves(root, &stub, moves, 0);
    if (count == 0u) {
        if (out_nodes) {
            *out_nodes = 0u;
        }
        return false;
    }

    player_t opponent = (config->ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    int16_t best_score = AI_SCORE_LOSS;
    move_t best_move = moves[0].move;
    bool has_move = false;
    uint32_t nodes = 0u;
    bool use_hint_eval = config->use_hint_profile;

    for (uint8_t i = 0; i < count; ++i) {
        board_t child;
        ai_board_copy(&child, root);
        if (!board_execute_move(&child, &moves[i].move, config->swap_rule)) {
            continue;
        }

        // Skip moves that allow opponent immediate win
        if (board_check_win(&child, opponent, NULL)) {
            continue;
        }

        bool forcing_move = false;
        if (config->enable_forcing_check && !use_hint_eval) {
            forcing_move = ai_move_creates_forced_immediate_win(root, &moves[i].move, config, config->ai_player);
        }

        ++nodes;

        if (board_check_win(&child, config->ai_player, NULL)) {
            if (out_nodes) {
                *out_nodes = nodes;
            }
            *out_move = moves[i].move;
            return true;
        }

        if (forcing_move) {
            if (out_nodes) {
                *out_nodes = nodes;
            }
            *out_move = moves[i].move;
            return true;
        }

        board_switch_turn(&child);

        ai_hint_eval_components_t heuristic_components;
        ai_hint_eval_components_t *heuristic_ptr = NULL;
        if (use_hint_eval && stub.hint_trace && stub.hint_trace->enabled) {
            stub.trace_depth = 0u;
            stub.trace_ply = 1u;
            heuristic_ptr = &heuristic_components;
        }

        int16_t score = use_hint_eval ? ai_agent_evaluate_hint(&child, config->ai_player, config, heuristic_ptr)
                                      : ai_agent_evaluate_internal(&child, config->ai_player, config, NULL);

        if (heuristic_ptr) {
            ai_hint_trace_record(&stub, heuristic_ptr, &child, config->ai_player);
        }
        if (!has_move || score > best_score) {
            best_score = score;
            best_move = moves[i].move;
            has_move = true;
        }
    }

    if (!has_move) {
        if (out_nodes) {
            *out_nodes = nodes;
        }
        return false;
    }

    if (out_nodes) {
        *out_nodes = nodes;
    }
    *out_move = best_move;
    return true;
}

static bool ai_search_should_abort(ai_search_context_t *ctx) {
    if (ctx->nodes >= ctx->node_limit) {
        ctx->abort = true;
        return true;
    }
#ifdef AI_AGENT_ENABLE_TIMER
    if (ctx->config->search.time_limit_ms && clock() >= ctx->deadline) {
        ctx->abort = true;
        return true;
    }
#endif
    return false;
}

static int16_t ai_eval_position(const board_t *board, player_t perspective, const ai_search_context_t *ctx) {
    if (ctx && ctx->use_hint_eval) {
        ai_hint_eval_components_t components;
        ai_hint_eval_components_t *components_ptr = NULL;
        if (ctx->hint_trace && ctx->hint_trace->enabled) {
            components_ptr = &components;
        }
        int16_t score = ai_agent_evaluate_hint(board, perspective, ctx->config, components_ptr);
        if (components_ptr) {
            ai_hint_trace_record(ctx, components_ptr, board, perspective);
        }
        return score;
    }
    return ai_agent_evaluate_internal(board, perspective, ctx->config, NULL);
}

int16_t FAR8_ai_negamax(board_t *board, ai_search_context_t *ctx, uint8_t depth, uint8_t ply, uint8_t extensions_used,
                        int16_t alpha, int16_t beta, move_t *out_move);

#if defined(AI_AGENT_HOST_TEST)

int16_t ai_negamax(board_t *board, ai_search_context_t *ctx, uint8_t depth, uint8_t ply, uint8_t extensions_used,
                   int16_t alpha, int16_t beta, move_t *out_move) {
    return FAR8_ai_negamax(board, ctx, depth, ply, extensions_used, alpha, beta, out_move);
}

#else

#pragma clang optimize off
__attribute__((noinline)) int16_t ai_negamax(board_t *board, ai_search_context_t *ctx, uint8_t depth, uint8_t ply,
                                             uint8_t extensions_used, int16_t alpha, int16_t beta, move_t *out_move) {
    int16_t result;
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    result = FAR8_ai_negamax(board, ctx, depth, ply, extensions_used, alpha, beta, out_move);
    *(volatile unsigned char *)0x000d = ___mmu;
    return result;
}
#pragma clang optimize on

#endif

__attribute__((noinline, section(".block8"))) int16_t FAR8_ai_negamax(board_t *board, ai_search_context_t *ctx,
                                                                      uint8_t depth, uint8_t ply,
                                                                      uint8_t extensions_used, int16_t alpha,
                                                                      int16_t beta, move_t *out_move) {
    if (ctx->abort) {
        return 0;
    }

    if (ai_search_should_abort(ctx)) {
        return 0;
    }

    ctx->nodes++;
    ai_emit_throttled_progress(ctx, depth);
    player_t to_move = board->current_player;
    player_t opponent = (to_move == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    if (board_check_win(board, opponent, NULL)) {
        return AI_SCORE_LOSS + (int16_t)ply;
    }
    if (board_check_win(board, to_move, NULL)) {
        return AI_SCORE_WIN - (int16_t)ply;
    }

    if (depth == 0) {
        if (extensions_used < ctx->config->search.max_extension &&
            ai_has_tactical_threat(board, ctx->config->swap_rule)) {
            depth = 1;
            extensions_used++;
        } else {
            ctx->trace_depth = depth;
            ctx->trace_ply = ply;
            return ai_eval_position(board, board->current_player, ctx);
        }
    }

    uint32_t hash = ai_hash_board(board);
    ai_tt_entry_t *tt_entry = NULL;
    if (ctx->config->search.use_transposition) {
        tt_entry = ai_tt_lookup(hash);
        if (tt_entry->key == hash && tt_entry->depth >= depth && tt_entry->flag != TT_FLAG_NONE) {
            if (tt_entry->flag == TT_FLAG_EXACT) {
                if (out_move) {
                    *out_move = tt_entry->move;
                }
                return tt_entry->score;
            } else if (tt_entry->flag == TT_FLAG_LOWER && tt_entry->score > alpha) {
                alpha = tt_entry->score;
            } else if (tt_entry->flag == TT_FLAG_UPPER && tt_entry->score < beta) {
                beta = tt_entry->score;
            }
            if (alpha >= beta) {
                if (out_move) {
                    *out_move = tt_entry->move;
                }
                return tt_entry->score;
            }
        }
    }

    ai_ordered_move_t moves[AI_MAX_ORDERED_MOVES];
    uint8_t move_count = ai_generate_moves(board, ctx, moves, ply);
    if (move_count == 0) {
        ctx->trace_depth = depth;
        ctx->trace_ply = ply;
        return ai_eval_position(board, board->current_player, ctx);
    }

    move_t best_move = moves[0].move;
    bool has_move = false;
    int16_t value = AI_SCORE_LOSS;
    int16_t original_alpha = alpha;

    for (uint8_t i = 0; i < move_count; ++i) {
        board_t child;
        ai_board_copy(&child, board);
        if (!board_execute_move(&child, &moves[i].move, ctx->config->swap_rule)) {
            continue;
        }

        if (board_check_win(&child, to_move, NULL)) {
            int16_t win_score = AI_SCORE_WIN - (int16_t)(ply + 1);
            if (win_score > value) {
                value = win_score;
                best_move = moves[i].move;
                has_move = true;
            }
            if (win_score > alpha) {
                alpha = win_score;
            }
            if (alpha >= beta) {
                ai_store_killer(ctx, ply, &moves[i].move);
                break;
            }
            continue;
        }

        board_switch_turn(&child);
        move_t response;
        int16_t score = (int16_t)(-ai_negamax(&child, ctx, (uint8_t)(depth - 1), (uint8_t)(ply + 1), extensions_used,
                                              (int16_t)(-beta), (int16_t)(-alpha), &response));
        if (ctx->abort) {
            return 0;
        }

#ifdef AI_AGENT_DEBUG_NEGAMAX
        if (ply == 0) {
            printf("DEBUG: Root move (%d,%d)->(%d,%d) got score %d\n", moves[i].move.from_row, moves[i].move.from_col,
                   moves[i].move.to_row, moves[i].move.to_col, score);
        }
#endif

        if (score > value || !has_move) {
            value = score;
            best_move = moves[i].move;
            has_move = true;
        }

        if (value > alpha) {
            alpha = value;
        }
        if (alpha >= beta) {
            ai_store_killer(ctx, ply, &moves[i].move);
            break;
        }
    }

    if (!has_move) {
        ctx->trace_depth = depth;
        ctx->trace_ply = ply;
        return ai_eval_position(board, board->current_player, ctx);
    }

    if (out_move) {
        *out_move = best_move;
    }

    if (ctx->config->search.use_transposition) {
        ai_tt_flag_t flag = TT_FLAG_EXACT;
        if (value <= original_alpha) {
            flag = TT_FLAG_UPPER;
        } else if (value >= beta) {
            flag = TT_FLAG_LOWER;
        }
        ai_tt_store(hash, depth, flag, value, &best_move);
    }

    return value;
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

    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            config->search.base_depth = 1;
            config->search.max_depth = 1;
            config->search.max_extension = 0;
            config->search.node_limit = 600;
            config->search.time_limit_ms = 0;
            config->search.use_iterative_deepening = false;
            config->search.use_transposition = false;
            config->search.use_move_ordering = true;
            config->search.use_killer_moves = false;
            break;
        case AI_DIFFICULTY_EASY:
            config->search.base_depth = 2;
            config->search.max_depth = 2;
            config->search.max_extension = 1;
            config->search.node_limit = 2000;
            config->search.time_limit_ms = 0;
            config->search.use_iterative_deepening = false;
            config->search.use_transposition = false;
            config->search.use_move_ordering = true;
            config->search.use_killer_moves = true;
            break;
        case AI_DIFFICULTY_STANDARD:
            config->search.base_depth = 4;
            config->search.max_depth = 4;
            config->search.max_extension = 2;
            config->search.node_limit = 9000;
            config->search.time_limit_ms = 0;
            config->search.use_iterative_deepening = true;
            config->search.use_transposition = true;
            config->search.use_move_ordering = true;
            config->search.use_killer_moves = true;
            break;
        case AI_DIFFICULTY_EXPERT:
        default:
            config->search.base_depth = 4;
            config->search.max_depth = 6;
            config->search.max_extension = 2;
            config->search.node_limit = 14000;
            config->search.time_limit_ms = 0;
            config->search.use_iterative_deepening = true;
            config->search.use_transposition = true;
            config->search.use_move_ordering = true;
            config->search.use_killer_moves = true;
            break;
    }

    ai_tt_clear();
    s_last_breakdown = (ai_eval_breakdown_t){0};
}

void ai_agent_set_progress_callback(ai_config_t *config, ai_progress_callback_t callback, void *user_data) {
    if (!config) {
        return;
    }
    config->progress_callback = callback;
    config->progress_user_data = user_data;
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
    board_t root;
    player_t to_move;

    ai_board_copy(&root, board);
    to_move = board->current_player;
    root.current_player = to_move;

    ai_timer0_reset();
    s_progress_throttle = 0u;

    if (!board_has_legal_moves(&root, to_move)) {
        uint32_t ticks = ai_timer0_read();
        ai_print_diagnostics(0u, ticks, config->diagnostics_enabled);
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    ai_config_t tuned = *config;
    tuned.ai_player = to_move;
    uint8_t pressure = ai_goal_row_pressure(&root);
    uint8_t dynamic_depth = ai_select_dynamic_depth(config, pressure);
    uint8_t move_volume = ai_count_move_volume(&root, to_move);

#ifdef AI_AGENT_DEBUG_NEGAMAX
    printf("DEBUG: move_volume=%d, initial_dynamic_depth=%d, pressure=%d\n", move_volume, dynamic_depth, pressure);
#endif

    // Check if this is puzzle hint mode (use lightweight profile)
    bool is_puzzle_hint = tuned.use_hint_profile;

    if (dynamic_depth > 0u) {
        if (move_volume >= 18u) {
            if (config->difficulty > AI_DIFFICULTY_STANDARD) {
                dynamic_depth = 1u;
            } else {
                dynamic_depth = 0u;
            }
        } else if (move_volume >= 12u && dynamic_depth > 2u) {
            dynamic_depth = 2u;
        } else if (move_volume >= 9u && dynamic_depth > 3u) {
            dynamic_depth = 3u;
        }
    }

#ifdef AI_AGENT_DEBUG_NEGAMAX
    printf(
        "DEBUG: is_puzzle_hint=%d, final_dynamic_depth=%d (0 means 1-ply "
        "heuristic only)\n",
        is_puzzle_hint, dynamic_depth);
#endif

    uint32_t node_cap = ai_select_node_cap(config, pressure);
    if (move_volume >= 18u && node_cap > 3000u) {
        node_cap = 3000u;
    } else if (move_volume >= 12u && node_cap > 5000u) {
        node_cap = 5000u;
    }

    if (is_puzzle_hint) {
        tuned.enable_forcing_check = false;
    }

    move_t best_move = (move_t){0};
    if (s_hint_trace.enabled) {
        s_hint_trace.count = 0u;
    }

    bool move_found = false;
    uint32_t nodes_recorded = 0u;
    int16_t best_score = AI_SCORE_LOSS;

    if (dynamic_depth == 0u) {
        move_found = ai_select_move_heuristic(&root, &tuned, &best_move, &nodes_recorded);
    } else {
        if (pressure == 4u) {
            tuned.search.base_depth = dynamic_depth;
            tuned.search.max_depth = dynamic_depth;
            tuned.search.use_iterative_deepening = false;
            tuned.search.use_transposition = false;
        } else {
            tuned.search.max_depth = dynamic_depth;
            if (tuned.search.base_depth > tuned.search.max_depth) {
                tuned.search.base_depth = tuned.search.max_depth;
            }
            if (dynamic_depth <= 2u) {
                tuned.search.use_iterative_deepening = false;
                tuned.search.use_transposition = false;
            } else {
                tuned.search.use_iterative_deepening = config->search.use_iterative_deepening;
                tuned.search.use_transposition = config->search.use_transposition;
            }
        }
        tuned.search.node_limit = node_cap;

        ai_search_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ai_search_context_t *previous_active_ctx = s_active_search_ctx;
        s_active_search_ctx = &ctx;
        ctx.config = &tuned;
        ctx.node_limit = tuned.search.node_limit ? tuned.search.node_limit : AI_NODE_LIMIT_FALLBACK;
        ctx.use_hint_eval = tuned.use_hint_profile;
        if (s_hint_trace.enabled) {
            ctx.hint_trace = &s_hint_trace;
        }
#ifdef AI_AGENT_ENABLE_TIMER
        if (tuned.search.time_limit_ms) {
            ctx.deadline = clock() + (clock_t)((tuned.search.time_limit_ms * CLOCKS_PER_SEC) / 1000u);
        }
#endif

        uint8_t target_depth = tuned.search.use_iterative_deepening ? tuned.search.max_depth : tuned.search.base_depth;
        if (target_depth == 0u) {
            target_depth = 1u;
        }
        uint8_t min_depth = tuned.search.base_depth ? tuned.search.base_depth : 1u;

        uint8_t start_depth = tuned.search.use_iterative_deepening ? 1u : min_depth;
        for (uint8_t depth = start_depth; depth <= target_depth; ++depth) {
            if (!tuned.search.use_iterative_deepening && depth != min_depth) {
                continue;
            }
            if (depth < min_depth) {
                continue;
            }

            move_t iteration_best = (move_t){0};
            int16_t score = ai_negamax(&root, &ctx, depth, 0, 0, AI_SCORE_LOSS, AI_SCORE_WIN, &iteration_best);
            if (ctx.abort) {
                break;
            }

            best_score = score;
            best_move = iteration_best;
            move_found = true;

            // Invoke progress callback if registered
            if (config->progress_callback) {
                config->progress_callback(depth, ctx.nodes, config->progress_user_data);
            }

            if (!tuned.search.use_iterative_deepening) {
                break;
            }
        }

        nodes_recorded = ctx.nodes;

        if (!move_found) {
            uint32_t heuristic_nodes = 0u;
            if (ai_select_move_heuristic(&root, &tuned, &best_move, &heuristic_nodes)) {
                move_found = true;
                nodes_recorded += heuristic_nodes;
            }
        }

        s_active_search_ctx = previous_active_ctx;
    }

    uint32_t elapsed_ticks = ai_timer0_read();
    ai_print_diagnostics(nodes_recorded, elapsed_ticks, config->diagnostics_enabled);

    if (!move_found) {
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    *out_move = best_move;

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

    (void)best_score;
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
