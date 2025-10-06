/**
 * @file ai_agent.c
 * @brief Specification-compliant heuristic AI agent for F256 Switcharoo.
 */

#include "../src/ai_agent.h"
#include "../src/board.h"
#include <limits.h>
#include <string.h>

#ifdef AI_AGENT_ENABLE_TIMER
#include <time.h>
#endif

#if defined(__llvm_mos__) && !defined(AI_AGENT_DISABLE_OVERLAY)
#include "../include/f256lib.h"

typedef struct ai_overlay_info_s {
    uint32_t lma;
    uint16_t size;
} __attribute__((packed)) ai_overlay_info_t;

extern const ai_overlay_info_t __ai_overlay_info;

static bool s_ai_overlay_loaded = false;

static void __attribute__((used)) ai_overlay_ensure_loaded(void) {
    if (s_ai_overlay_loaded) {
        return;
    }

    uint32_t src = __ai_overlay_info.lma;
    uint16_t remaining = __ai_overlay_info.size;
    uint16_t dest = 0xA000u;

    for (uint16_t offset = 0; offset < remaining; ++offset) {
        uint8_t value = FAR_PEEK(src + offset);
        POKE(dest + offset, value);
    }

    s_ai_overlay_loaded = true;
}

#define AI_OVERLAY_SECTION __attribute__((noinline, used))
#else
#define AI_OVERLAY_SECTION
static void ai_overlay_ensure_loaded(void) {
    /* Host builds run without overlay indirection. */
}
#endif

#define AI_SCORE_WIN 30000
#define AI_SCORE_LOSS (-AI_SCORE_WIN)
#define AI_SCORE_MAX 32000
#define AI_NODE_LIMIT_FALLBACK 16000U
#define AI_MAX_ORDERED_MOVES 64
#define AI_MAX_DEPTH 8
#define AI_KILLER_PER_PLY 2
#define AI_TRANSPOSITION_SIZE 16
#define AI_TRANSPOSITION_MASK (AI_TRANSPOSITION_SIZE - 1)

static const int8_t kAdjRow[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const int8_t kAdjCol[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

typedef enum {
    TT_FLAG_NONE = 0,
    TT_FLAG_EXACT = 1,
    TT_FLAG_LOWER = 2,
    TT_FLAG_UPPER = 3
} ai_tt_flag_t;

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
    move_t pv[AI_MAX_DEPTH];
    uint8_t pv_length;
    move_t killer_moves[AI_MAX_DEPTH][AI_KILLER_PER_PLY];
    uint8_t killer_count[AI_MAX_DEPTH];
#ifdef AI_AGENT_ENABLE_TIMER
    clock_t deadline;
#endif
} ai_search_context_t;

static bool s_zobrist_ready = false;
static uint16_t s_zobrist_board[BOARD_CELLS][5];
static uint16_t s_zobrist_player[2];
static ai_tt_entry_t s_tt[AI_TRANSPOSITION_SIZE];
static ai_eval_breakdown_t s_last_breakdown;

static const ai_eval_weights_t kRuleWeights[4] = {
    { 60, 40, 32, 44, 22 }, // Classic
    { 58, 38, 28, 42, 22 }, // Clears Own
    { 55, 42, 44, 36, 24 }, // Swapped Clears
    { 55, 40, 40, 36, 24 }  // Swapped Clears Own
};

static void ai_board_copy(board_t *dest, const board_t *src) {
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
    return a->from_row == b->from_row &&
           a->from_col == b->from_col &&
           a->to_row == b->to_row &&
           a->to_col == b->to_col &&
           a->type == b->type;
}

static void ai_tt_store(uint32_t key, uint8_t depth, ai_tt_flag_t flag,
                        int16_t score, const move_t *move) {
    ai_tt_entry_t *entry = ai_tt_lookup(key);
    if (entry->key == key && entry->depth > depth) {
        return; // Keep deeper information
    }

    entry->key = key;
    entry->score = score;
    entry->depth = depth;
    entry->flag = (uint8_t)flag;
    entry->move = *move;
}

#if defined(__llvm_mos__) && !defined(AI_AGENT_DISABLE_OVERLAY)
#pragma clang section text=".block8.ai"
#endif

static void AI_OVERLAY_SECTION ai_store_killer(ai_search_context_t *ctx, uint8_t ply, const move_t *move) {
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

static void AI_OVERLAY_SECTION ai_compute_connection_metrics(const board_t *board, player_t player,
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
                if (new_row >= 0 && new_row < BOARD_ROWS &&
                    new_col >= 0 && new_col < BOARD_COLS) {
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
                if (new_row >= 0 && new_row < BOARD_ROWS &&
                    new_col >= 0 && new_col < BOARD_COLS) {
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

static uint8_t AI_OVERLAY_SECTION ai_count_bridge_potential(const board_t *board, player_t player) {
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
                if (new_row >= 0 && new_row < BOARD_ROWS &&
                    new_col >= 0 && new_col < BOARD_COLS) {
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

static uint8_t AI_OVERLAY_SECTION ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
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
                    if (new_row >= 0 && new_row < BOARD_ROWS &&
                        new_col >= 0 && new_col < BOARD_COLS) {
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
                    if (new_row >= 0 && new_row < BOARD_ROWS &&
                        new_col >= 0 && new_col < BOARD_COLS) {
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

static uint8_t AI_OVERLAY_SECTION ai_measure_blocking(const board_t *board, player_t player) {
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
                if (new_row >= 0 && new_row < BOARD_ROWS &&
                    new_col >= 0 && new_col < BOARD_COLS) {
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

static uint16_t ai_measure_mobility(const board_t *board, player_t player) {
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

static bool AI_OVERLAY_SECTION ai_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule) {
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
        if (board_check_win(&test, player, NULL)) {
            return true;
        }
    }

    return false;
}

static bool AI_OVERLAY_SECTION ai_has_tactical_threat(const board_t *board, swap_rule_t rule) {
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

static uint8_t AI_OVERLAY_SECTION ai_generate_moves(const board_t *board, const ai_search_context_t *ctx,
                                                    ai_ordered_move_t *out_moves, uint8_t ply) {
    board_t scratch;
    ai_board_copy(&scratch, board);
    scratch.current_player = board->current_player;

    uint8_t count = 0;
    move_t buffer[8];

    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != scratch.current_player) {
                continue;
            }

            uint8_t generated = board_get_legal_moves(&scratch, row, col, buffer,
                                                     (uint8_t)(AI_MAX_ORDERED_MOVES - count));
            for (uint8_t i = 0; i < generated; ++i) {
                move_t *move = &buffer[i];
                int16_t score = 0;

                if (ai_is_killer(ctx, ply, move)) {
                    score += 4000;
                }
                if (move->type == MOVE_TYPE_SWAP) {
                    score += 900;
                }
                if (move->to_col == 1 || move->to_col == 2) {
                    score += 350;
                }
                if (scratch.current_player == PLAYER_WHITE) {
                    if (move->to_row < move->from_row) {
                        score += 280;
                    }
                } else {
                    if (move->to_row > move->from_row) {
                        score += 280;
                    }
                }
                if (move->type == MOVE_TYPE_EMPTY && move->to_row >= WIN_START_ROW && move->to_row <= WIN_END_ROW) {
                    score += 120;
                }

                out_moves[count].move = *move;
                out_moves[count].order_score = score;
                count++;
            }
        }
    }

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

static int16_t AI_OVERLAY_SECTION ai_agent_evaluate_internal(const board_t *board, player_t perspective,
                                                             const ai_config_t *config,
                                                             ai_eval_breakdown_t *breakdown) {
    player_t opponent = (perspective == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    if (board_check_win(board, perspective, NULL)) {
        return AI_SCORE_WIN - (int16_t)(board->move_count & 0x7FFF);
    }
    if (board_check_win(board, opponent, NULL)) {
        return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF);
    }

    ai_connection_metrics_t conn_me;
    ai_connection_metrics_t conn_op;
    ai_compute_connection_metrics(board, perspective, &conn_me);
    ai_compute_connection_metrics(board, opponent, &conn_op);

    int16_t connection_me = (int16_t)(ai_popcount(conn_me.row_mask) * 12 +
                                      conn_me.row_links * 18 +
                                      conn_me.branching_nodes * 5);
    int16_t connection_op = (int16_t)(ai_popcount(conn_op.row_mask) * 12 +
                                      conn_op.row_links * 18 +
                                      conn_op.branching_nodes * 5);

    int16_t bridge_me = (int16_t)ai_count_bridge_potential(board, perspective);
    int16_t bridge_op = (int16_t)ai_count_bridge_potential(board, opponent);

    int16_t swap_me = (int16_t)ai_measure_swap_pressure(board, perspective, config->swap_rule);
    int16_t swap_op = (int16_t)ai_measure_swap_pressure(board, opponent, config->swap_rule);

    int16_t block_me = (int16_t)ai_measure_blocking(board, perspective);
    int16_t block_op = (int16_t)ai_measure_blocking(board, opponent);

    int16_t mobility_me = (int16_t)ai_measure_mobility(board, perspective);
    int16_t mobility_op = (int16_t)ai_measure_mobility(board, opponent);

    int32_t total = 0;
    int16_t contrib_conn = ai_clamp_score((int32_t)config->weights.connection_progress * (connection_me - connection_op));
    int16_t contrib_bridge = ai_clamp_score((int32_t)config->weights.bridge_potential * (bridge_me - bridge_op));
    int16_t contrib_swap = ai_clamp_score((int32_t)config->weights.swap_pressure * (swap_me - swap_op));
    int16_t contrib_block = ai_clamp_score((int32_t)config->weights.blocking_coverage * (block_me - block_op));
    int16_t contrib_mobility = ai_clamp_score((int32_t)config->weights.mobility * (mobility_me - mobility_op));

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

static int16_t AI_OVERLAY_SECTION ai_negamax(board_t *board, ai_search_context_t *ctx, uint8_t depth,
                                             uint8_t ply, uint8_t extensions_used, int16_t alpha,
                                             int16_t beta, move_t *out_move) {
    if (ctx->abort) {
        return 0;
    }

    if (ai_search_should_abort(ctx)) {
        return 0;
    }

    ctx->nodes++;
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
            return ai_agent_evaluate_internal(board, ctx->config->ai_player,
                                               ctx->config, NULL);
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
        return ai_agent_evaluate_internal(board, ctx->config->ai_player,
                                           ctx->config, NULL);
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
        int16_t score = (int16_t)(-ai_negamax(&child, ctx, (uint8_t)(depth - 1),
                                              (uint8_t)(ply + 1), extensions_used,
                                              (int16_t)(-beta), (int16_t)(-alpha),
                                              &response));
        if (ctx->abort) {
            return 0;
        }

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
        return ai_agent_evaluate_internal(board, ctx->config->ai_player,
                                           ctx->config, NULL);
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

void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule,
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
    s_last_breakdown = (ai_eval_breakdown_t){ 0 };
}

static bool AI_OVERLAY_SECTION ai_agent_find_best_move_impl(const board_t *board, const ai_config_t *config,
                                                            move_t *out_move) {

    board_t root;
    ai_board_copy(&root, board);
    root.current_player = config->ai_player;

    if (!board_has_legal_moves(&root, root.current_player)) {
        return false;
    }

    ai_search_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.config = config;
    ctx.node_limit = config->search.node_limit ? config->search.node_limit : AI_NODE_LIMIT_FALLBACK;
#ifdef AI_AGENT_ENABLE_TIMER
    if (config->search.time_limit_ms) {
        ctx.deadline = clock() + (clock_t)((config->search.time_limit_ms * CLOCKS_PER_SEC) / 1000u);
    }
#endif

    int16_t best_score = AI_SCORE_LOSS;
    move_t best_move = { 0 };
    bool has_completed_iteration = false;

    uint8_t target_depth = config->search.use_iterative_deepening ? config->search.max_depth : config->search.base_depth;
    uint8_t min_depth = config->search.base_depth ? config->search.base_depth : 1;

    for (uint8_t depth = 1; depth <= target_depth; ++depth) {
        if (!config->search.use_iterative_deepening && depth != min_depth) {
            continue;
        }
        if (depth < min_depth) {
            continue;
        }

        move_t iteration_best = { 0 };
        int16_t score = ai_negamax(&root, &ctx, depth, 0, 0, AI_SCORE_LOSS, AI_SCORE_WIN, &iteration_best);

        if (ctx.abort) {
            break;
        }

        best_score = score;
        best_move = iteration_best;
        has_completed_iteration = true;

        if (!config->search.use_iterative_deepening) {
            break;
        }
    }

    if (!has_completed_iteration) {
        ai_ordered_move_t moves[AI_MAX_ORDERED_MOVES];
        ai_search_context_t fallback_ctx;
        memset(&fallback_ctx, 0, sizeof(fallback_ctx));
        fallback_ctx.config = config;
        uint8_t count = ai_generate_moves(&root, &fallback_ctx, moves, 0);
        if (count == 0) {
            return false;
        }
        best_move = moves[0].move;
        has_completed_iteration = true;
    }

    if (!has_completed_iteration) {
        return false;
    }

    *out_move = best_move;

    if (config->diagnostics_enabled) {
        board_t analysed;
        ai_board_copy(&analysed, board);
        analysed.current_player = config->ai_player;
        if (board_execute_move(&analysed, out_move, config->swap_rule)) {
            board_switch_turn(&analysed);
            ai_eval_breakdown_t breakdown;
            ai_agent_evaluate_internal(&analysed, config->ai_player, config, &breakdown);
            s_last_breakdown = breakdown;
        } else {
            s_last_breakdown = (ai_eval_breakdown_t){ 0 };
        }
    } else {
        s_last_breakdown = (ai_eval_breakdown_t){ 0 };
    }

    (void)best_score; // Reserved for future diagnostics
    return true;
}

#if defined(__llvm_mos__) && !defined(AI_AGENT_DISABLE_OVERLAY)
#pragma clang section text=""
#endif

bool ai_agent_find_best_move(const board_t *board, const ai_config_t *config,
                             move_t *out_move) {
    if (!board || !config || !out_move) {
        return false;
    }

#if defined(__llvm_mos__) && !defined(AI_AGENT_DISABLE_OVERLAY)
    ai_overlay_ensure_loaded();
#endif

    return ai_agent_find_best_move_impl(board, config, out_move);
}

int16_t ai_agent_evaluate_board(const board_t *board, player_t player,
                                const ai_config_t *config) {
    if (!board || !config) {
        return 0;
    }
#if defined(__llvm_mos__) && !defined(AI_AGENT_DISABLE_OVERLAY)
    ai_overlay_ensure_loaded();
#endif
    return ai_agent_evaluate_internal(board, player, config, NULL);
}

void ai_agent_get_last_breakdown(ai_eval_breakdown_t *out) {
    if (!out) {
        return;
    }
    *out = s_last_breakdown;
}
