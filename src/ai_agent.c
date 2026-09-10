/**
 * @file ai_agent.c
 * @brief Specification-compliant heuristic AI agent for F256 Switcharoo.
 */
#include "ai_agent.h"
#include "f256lib.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "input.h"
#include "text_display.h"
#include "overlay_config.h"

#define AI_SCORE_WIN 30000
#define AI_SCORE_LOSS (-AI_SCORE_WIN)
#define AI_SCORE_MAX 32000

static void ai_random_seed_internal(uint16_t seed) {
    if (seed == 0u) {
        seed = 1u;
    }
    randomSeed(seed);
}

void ai_agent_set_random_seed(uint16_t seed) {
    ai_random_seed_internal(seed);
}

static uint16_t ai_random_next(void) {
    return randomRead();
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
    return (int32_t)lhs * (int32_t)rhs;
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
    uint8_t best_component_mask;  // Row mask of the most complete connected component
} ai_connection_metrics_t;

static ai_eval_breakdown_t s_last_breakdown;

// Global progress callback variables

static ai_progress_callback_t s_progress_callback = NULL;
static void *s_progress_user_data = NULL;

static const ai_eval_weights_t kRuleWeights[4] = {
    {58, 78, 40, 41, 35},  // Classic
    {73, 52, 69, 22, 11},  // Clears Own
    {69, 37, 62, 47, 8},  // Swapped Clears
    {63, 39, 67, 36, 20},  // Swapped Clears Own
};


void ai_board_copy(board_t *dest, const board_t *src) {
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

#pragma code(ovl9_code)

void ai_compute_connection_metrics(const board_t *board,
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
    out->best_component_mask = 0;

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
        
        // Track the best (most rows covered) single connected component
        if (ai_popcount(mask) > ai_popcount(out->best_component_mask)) {
            out->best_component_mask = mask;
        }
    }
}

uint8_t ai_count_bridge_potential(const board_t *board,
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

uint8_t ai_measure_swap_pressure(const board_t *board, player_t player, swap_rule_t rule) {
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

uint8_t ai_measure_blocking(const board_t *board, player_t player) {
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

uint16_t ai_measure_mobility(const board_t *board, player_t player) {
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

#pragma code(code)

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
                board_context_t test_context; memset(&test_context, 0, sizeof(test_context)); test_context.current_player = player;
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

    board_context_t opponent_context; memset(&opponent_context, 0, sizeof(opponent_context)); opponent_context.current_player = mover;
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
                board_context_t response_context; memset(&response_context, 0, sizeof(response_context)); response_context.current_player = opponent;
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


#pragma code(ovl9_code)

bool FAR_ai_forcing_move_available(const board_t *board,
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
                board_context_t move_ctx; memset(&move_ctx, 0, sizeof(move_ctx)); move_ctx.current_player = player;
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
                            board_context_t resp_ctx; memset(&resp_ctx, 0, sizeof(resp_ctx)); resp_ctx.current_player = ai_player;
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

#pragma code(code)

bool ai_forcing_move_available(const board_t *board, player_t current_player, player_t player, swap_rule_t rule) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_9);
    bool result = FAR_ai_forcing_move_available(board, current_player, player, rule);
    POKE(OVERLAY_MMU_REG, saved);
    return result;
}

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


#pragma code(ovl10_code)

uint8_t FAR_ai_generate_moves(const board_t *board,
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

                // NOTE: Central column bonus REMOVED from move ordering
                // It was causing A/D column pieces to be deprioritized, leaving them
                // stranded on back ranks. The evaluation function still considers
                // central control, but move ordering should not bias against edge columns.

                // Player-agnostic advancement logic
                // Board coordinates: Row 0 = top (Black's back, chess row 8)
                //                    Row 7 = bottom (White's back, chess row 1)
                // White advances UP (toward row 0), Black advances DOWN (toward row 7)
                bool is_advancing;
                bool is_leaving_absolute_back;
                bool is_leaving_second_rank;
                bool reached_far_edge;
                
                if (current_player == PLAYER_WHITE) {
                    // White advances by moving to lower row numbers (up toward row 0)
                    is_advancing = (move.to_row < move.from_row);
                    reached_far_edge = (move.to_row == WIN_START_ROW);  // Row 1
                    // White's absolute back is row 7, second rank is row 6
                    is_leaving_absolute_back = (move.from_row == 7 && move.to_row < 7);
                    is_leaving_second_rank = (move.from_row == 6 && move.to_row < 6);
                } else {
                    // Black advances by moving to higher row numbers (down toward row 7)
                    is_advancing = (move.to_row > move.from_row);
                    reached_far_edge = (move.to_row == WIN_END_ROW);  // Row 6
                    // Black's absolute back is row 0, second rank is row 1
                    is_leaving_absolute_back = (move.from_row == 0 && move.to_row > 0);
                    is_leaving_second_rank = (move.from_row == 1 && move.to_row > 1);
                }
                
                if (is_advancing) {
                    score += 400;  // Bonus for advancing toward opponent's side
                }
                
                if (reached_far_edge) {
                    score += 300;  // Bonus for reaching the far edge victory row
                }
                
                // VERY STRONG bonus for moving OFF the absolute back rank
                // This is critical for development - back rank pieces are liabilities
                if (is_leaving_absolute_back) {
                    score += 1200;  // Increased from 800 - prioritize evacuation
                }
                
                // Strong bonus for advancing from second rank to victory rows
                if (is_leaving_second_rank) {
                    score += 500;  // Increased from 300
                }

                if (move.type == MOVE_TYPE_EMPTY && move.to_row >= WIN_START_ROW && move.to_row <= WIN_END_ROW) {
                    score += 120;
                }
                
                // UNBLOCKING BONUS: Favor moves that free trapped back-rank pieces.
                // If we have pieces on the back ranks with 0 legal moves, prioritize
                // moves that will give them mobility (by vacating blocking squares).
                // Check if this move vacates a square adjacent to our back rank.
                if (move.type == MOVE_TYPE_EMPTY) {
                    uint8_t back_row = (current_player == PLAYER_WHITE) ? 7u : 0u;
                    uint8_t second_row = (current_player == PLAYER_WHITE) ? 6u : 1u;
                    
                    // If moving FROM the second rank, check if we unblock back rank pieces
                    if (move.from_row == second_row) {
                        // Count trapped pieces on back rank BEFORE this move
                        uint8_t trapped_before = 0;
                        move_array_t check_moves;
                        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                            piece_type_t p = board_get_piece_unchecked(board, back_row, c);
                            if (board_get_piece_owner(p) == current_player) {
                                if (board_get_legal_moves_soa(board, current_player, back_row, c, &check_moves) == 0) {
                                    trapped_before++;
                                }
                            }
                        }
                        
                        // If there are trapped back-rank pieces, give a big bonus
                        // for moves that might unblock them
                        if (trapped_before > 0) {
                            // Moving from second rank creates space for back rank
                            // Bonus scales with number of trapped pieces
                            score += (int16_t)(trapped_before * 400);
                        }
                    }
                }

                // Anti-reversal: penalize moves that reverse opponent's last move
                if (config->last_opp_from != 0xFF) {
                    uint8_t opp_from_row = config->last_opp_from >> 4;
                    uint8_t opp_from_col = config->last_opp_from & 0x0F;
                    uint8_t opp_to_row = config->last_opp_to >> 4;
                    uint8_t opp_to_col = config->last_opp_to & 0x0F;
                    // Check if this move reverses opponent's last move
                    if (move.to_row == opp_from_row && move.to_col == opp_from_col &&
                        move.from_row == opp_to_row && move.from_col == opp_to_col) {
                        score -= 600;  // Discourage direct reversal
                    }
                }

                // Self-reversal: penalize returning own piece to where it was 2 plies ago
                if (config->self_prev_from != 0xFF) {
                    uint8_t self_from_row = config->self_prev_from >> 4;
                    uint8_t self_from_col = config->self_prev_from & 0x0F;
                    uint8_t self_to_row = config->self_prev_to >> 4;
                    uint8_t self_to_col = config->self_prev_to & 0x0F;
                    // Check if this move returns a piece to its position from 2 plies ago
                    if (move.to_row == self_from_row && move.to_col == self_from_col &&
                        move.from_row == self_to_row && move.from_col == self_to_col) {
                        score -= 700;  // Stronger penalty for self-reversal (oscillation)
                    }
                }

                // Apply move once for immediate outcome analysis
                board_t after_move;
                ai_board_copy(&after_move, board);
                board_context_t move_context; memset(&move_context, 0, sizeof(move_context)); move_context.current_player = current_player;
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

                board_context_t opponent_context; memset(&opponent_context, 0, sizeof(opponent_context)); opponent_context.current_player = current_player;
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
                // Add small random tiebreaker (+0 to +15) to break deterministic 
                // left-to-right enumeration bias for moves with equal strategic value.
                // This ensures pieces on all columns have equal chance of being selected
                // when their moves have equivalent ordering scores.
                out_moves->order_scores[slot_index] = (int16_t)(score + (int16_t)(ai_random_next() & 0x0F));
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

#pragma code(code)


// Count pieces on back ranks and victory rows for a player
// Returns packed data:
//   High nibble (bits 4-7): pieces on back rank
//   Low nibble (bits 0-3): pieces on second rank
//
// Board coordinate system:
//   Row 0 = Black's back rank (top of screen, chess row 8)
//   Row 1 = Black's second rank (chess row 7)
//   Row 6 = White's second rank (chess row 2)
//   Row 7 = White's back rank (bottom of screen, chess row 1)
//   White advances toward row 0 (up), Black advances toward row 7 (down)
static uint8_t ai_count_back_rank_pieces(const board_t *board, player_t player) {
    uint8_t back_rank_count = 0;
    uint8_t second_rank_count = 0;
    
    // White's back rank is row 7 (bottom, chess A1-D1)
    // Black's back rank is row 0 (top, chess A8-D8)
    uint8_t back_row = (player == PLAYER_WHITE) ? 7u : 0u;
    uint8_t second_row = (player == PLAYER_WHITE) ? 6u : 1u;
    
    for (uint8_t col = 0; col < BOARD_COLS; ++col) {
        piece_type_t piece_back = board_get_piece_unchecked(board, back_row, col);
        if (board_get_piece_owner(piece_back) == player) {
            back_rank_count++;
        }
        
        piece_type_t piece_second = board_get_piece_unchecked(board, second_row, col);
        if (board_get_piece_owner(piece_second) == player) {
            second_rank_count++;
        }
    }
    
    return (uint8_t)((back_rank_count << 4) | second_rank_count);
}

// Count pieces on victory rows and how many victory rows are occupied
// Returns packed data:
//   High byte: total pieces on victory rows (2-7)
//   Low byte: number of distinct victory rows occupied (0-6)
static uint16_t ai_count_victory_row_pieces(const board_t *board, player_t player) {
    uint8_t total_pieces = 0;
    uint8_t rows_occupied = 0;
    
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; ++row) {
        uint8_t pieces_on_row = 0;
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece_unchecked(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                pieces_on_row++;
            }
        }
        total_pieces += pieces_on_row;
        if (pieces_on_row > 0) {
            rows_occupied++;
        }
    }
    
    return (uint16_t)((total_pieces << 8) | rows_occupied);
}

// Calculate development/occupancy score based on puzzle analysis
// Derived from 40 Win-in-2 puzzles:
//   - 95% have 7-8 pieces on victory rows (target: 7+)
//   - 95% have 0-1 pieces on back ranks combined
//   - 90% have 4-5 victory rows occupied
//   - Max 1 piece on own back rank, max 2 on far back (very rare)
//
// Returns: positive score for good development, negative for poor
// Range: approximately -600 to +400
static int16_t ai_calculate_development_score(const board_t *board, player_t player) {
    uint8_t packed_back = ai_count_back_rank_pieces(board, player);
    uint8_t back_rank = (packed_back >> 4) & 0x0F;
    uint8_t second_rank = packed_back & 0x0F;
    uint8_t total_back_rows = back_rank + second_rank;
    
    uint16_t packed_victory = ai_count_victory_row_pieces(board, player);
    uint8_t victory_pieces = (packed_victory >> 8) & 0xFF;
    uint8_t victory_rows_occupied = packed_victory & 0xFF;
    
    int16_t score = 0;
    
    // ========== BACK RANK PENALTIES ==========
    // CRITICAL: Pieces on starting ranks are a major strategic liability.
    // The AI must prioritize evacuating ALL pieces, not just some.
    //
    // Increased penalties to ensure back-rank evacuation is prioritized
    // over other strategic considerations.
    
    // VERY strong penalty for pieces on absolute back rank (row 8 for Black, row 1 for White)
    // Each piece on absolute back = -150 points (was -60)
    score -= (int16_t)(back_rank * 150);
    
    // Escalating penalty: more pieces on back rank is exponentially worse
    // 2 pieces: extra -100, 3 pieces: extra -200, 4 pieces: extra -300
    if (back_rank >= 2) {
        score -= (int16_t)((back_rank - 1) * 100);
    }
    
    // Moderate penalty for pieces on second rank
    // Each piece = -60 points (was -30 for pieces beyond 2)
    score -= (int16_t)(second_rank * 60);
    
    // Extra penalty if 3+ pieces on second rank
    if (second_rank >= 3) {
        score -= (int16_t)((second_rank - 2) * 80);
    }
    
    // Severe penalty for having many pieces clustered on back two ranks
    // 4+ pieces on rows 1-2 combined is very bad
    if (total_back_rows >= 4) {
        score -= (int16_t)((total_back_rows - 3) * 100);
    }
    
    // ========== ADVANCEMENT BONUSES ==========
    // Reward for pieces on victory rows (rows 2-7 for both players)
    
    // Each piece on victory row = +30 points
    score += (int16_t)(victory_pieces * 30);
    
    // Bonus for good coverage (6+ pieces advanced)
    if (victory_pieces >= 6) {
        score += 60;
    }
    
    // Bonus for row coverage
    score += (int16_t)(victory_rows_occupied * 20);
    
    // ========== COMBINED ASSESSMENT ==========
    
    // Bonus for well-developed positions:
    // 6+ pieces advanced AND <=1 on absolute back rank
    if (victory_pieces >= 6 && back_rank <= 1) {
        score += 50;
    }
    
    // Penalty for underdeveloped positions:
    // <5 pieces advanced means not enough development
    if (victory_pieces < 5) {
        score -= (int16_t)((5 - victory_pieces) * 25);
    }
    
    return score;
}

#pragma code(ovl9_code)

int16_t FAR_ai_agent_evaluate_internal(const board_t *board,
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
            ai_forcing_move_available(board, current_player, opponent, config->swap_rule)) {
            // Opponent has a forcing move, very bad for us
            return AI_SCORE_LOSS + (int16_t)(board->move_count & 0x7FFF) + 1000;  // Heavy penalty
        }
    }

    ai_connection_metrics_t conn_me;
    ai_connection_metrics_t conn_op;
    ai_compute_connection_metrics(board, perspective, &conn_me);
    ai_compute_connection_metrics(board, opponent, &conn_op);

    // Base connection score from row coverage and links
    int16_t connection_me =
        (int16_t)(ai_popcount(conn_me.row_mask) * 12 + conn_me.row_links * 18 + conn_me.branching_nodes * 5);
    int16_t connection_op =
        (int16_t)(ai_popcount(conn_op.row_mask) * 12 + conn_op.row_links * 18 + conn_op.branching_nodes * 5);

    // Add bonus for edge row coverage in best connected component
    // Bit 0 = row 1 (WIN_START_ROW), Bit 5 = row 6 (WIN_END_ROW)
    const uint8_t EDGE_ROW_START_BIT = 0u;
    const uint8_t EDGE_ROW_END_BIT = (uint8_t)(WIN_END_ROW - WIN_START_ROW);  // Should be 5
    const uint8_t START_MASK = (uint8_t)(1u << EDGE_ROW_START_BIT);
    const uint8_t END_MASK = (uint8_t)(1u << EDGE_ROW_END_BIT);
    
    // Bonus for having each edge row in the best component
    if (conn_me.best_component_mask & START_MASK) {
        connection_me += 25;  // Has row 1 in main component
    }
    if (conn_me.best_component_mask & END_MASK) {
        connection_me += 25;  // Has row 6 in main component
    }
    // Big bonus for having BOTH edge rows in the same component (near winning!)
    if ((conn_me.best_component_mask & START_MASK) && (conn_me.best_component_mask & END_MASK)) {
        connection_me += 80;  // Connected from row 1 to row 6!
    }
    
    // Same for opponent
    if (conn_op.best_component_mask & START_MASK) {
        connection_op += 25;
    }
    if (conn_op.best_component_mask & END_MASK) {
        connection_op += 25;
    }
    if ((conn_op.best_component_mask & START_MASK) && (conn_op.best_component_mask & END_MASK)) {
        connection_op += 80;
    }

    int16_t bridge_me = (int16_t)ai_count_bridge_potential(board, perspective);
    int16_t bridge_op = (int16_t)ai_count_bridge_potential(board, opponent);

    int16_t swap_me = (int16_t)ai_measure_swap_pressure(board, perspective, config->swap_rule);
    int16_t swap_op = (int16_t)ai_measure_swap_pressure(board, opponent, config->swap_rule);

    int16_t block_me = (int16_t)ai_measure_blocking(board, perspective);
    int16_t block_op = (int16_t)ai_measure_blocking(board, opponent);

    int16_t mobility_me = (int16_t)ai_measure_mobility(board, perspective);
    int16_t mobility_op = (int16_t)ai_measure_mobility(board, opponent);

    // Development score: positive = good development, negative = poor
    // Based on puzzle analysis: target 7+ pieces on victory rows, ≤1 on back ranks
    int16_t dev_score_me = ai_calculate_development_score(board, perspective);
    int16_t dev_score_op = ai_calculate_development_score(board, opponent);

    int32_t total = 0;
    int16_t diff_conn = (int16_t)(connection_me - connection_op);
    int16_t diff_bridge = (int16_t)(bridge_me - bridge_op);
    int16_t diff_swap = (int16_t)(swap_me - swap_op);
    int16_t diff_block = (int16_t)(block_me - block_op);
    int16_t diff_mobility = (int16_t)(mobility_me - mobility_op);
    // Development: positive score is better, so we want (my_score - opponent_score)
    int16_t diff_development = (int16_t)(dev_score_me - dev_score_op);

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
    
    // Add development contribution directly (already scaled in the penalty calculation)
    // This encourages advancing pieces off the back ranks
    total += diff_development;

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

#pragma code(code)

int16_t ai_agent_evaluate_internal(const board_t *board, player_t current_player, player_t perspective, const ai_config_t *config, ai_eval_breakdown_t *breakdown) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_9);
    int16_t result = FAR_ai_agent_evaluate_internal(board, current_player, perspective, config, breakdown);
    POKE(OVERLAY_MMU_REG, saved);
    return result;
}


#pragma code(ovl9_code)

void FAR_ai_agent_init(ai_config_t *config, swap_rule_t swap_rule,
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
    config->last_opp_from = 0xFF;   // No last opponent move initially
    config->self_prev_from = 0xFF;  // No previous own move initially

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

#pragma code(code)

void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule, ai_difficulty_t difficulty, player_t ai_player) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_9);
    FAR_ai_agent_init(config, swap_rule, difficulty, ai_player);
    POKE(OVERLAY_MMU_REG, saved);
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


#pragma code(ovl11_code)

static uint8_t FAR_ai_evaluate_moves(
    board_t *root, player_t current_player, const ai_config_t *config, const ai_ordered_moves_t *ordered,
    uint8_t generated, ai_evaluated_moves_t *evaluated) {
    if (!root || !config || !ordered || !evaluated) {
        return 0u;
    }

    uint8_t count = 0u;
    player_t opponent = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Track if AI has found a forced win - skip expensive opponent forcing checks after that
    bool ai_has_forced_win = false;

    for (uint8_t i = 0u; i < generated && count < AI_MAX_ORDERED_MOVES; ++i) {
        uint8_t move_slot = ordered->indices[i];
        if (move_slot >= AI_MAX_ORDERED_MOVES) {
            continue;
        }

        move_t move = move_make(ordered->moves.from_rows[move_slot], ordered->moves.from_cols[move_slot], ordered->moves.to_rows[move_slot], ordered->moves.to_cols[move_slot], ordered->moves.types[move_slot], ordered->moves.players[move_slot]);

        board_t child;
        ai_board_copy(&child, root);
        board_context_t move_context; memset(&move_context, 0, sizeof(move_context)); move_context.current_player = current_player;
        if (!board_execute_move_without_history(&child, &move_context, &move, config->swap_rule)) {
            continue;
        }

        uint8_t move_flags = ordered->flags[move_slot];
        bool opponent_immediate = (move_flags & AI_ORDER_FLAG_OPPONENT_IMMEDIATE) != 0u;

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
        evaluated->immediate_wins_opponent[target_index] = opponent_immediate;

        if (self_immediate) {
            evaluated->immediate_wins_self[target_index] = true;
            evaluated->opponent_wins_next_move[target_index] = false;
            evaluated->opponent_forced_wins[target_index] = false;
            evaluated->forced_wins_self[target_index] = false;
            evaluated->evaluations[target_index] = AI_SCORE_WIN - (int16_t)(root->move_count & 0x7FFF);
            count = 1u;
            break;
        }

        evaluated->immediate_wins_self[target_index] = false;

        board_context_t next_context; memset(&next_context, 0, sizeof(next_context)); next_context.current_player = current_player;
        board_switch_turn(&next_context);
        player_t defender = next_context.current_player;

        bool opponent_win_next = opponent_immediate ? true : ai_immediate_win_available(&child, opponent, config->swap_rule);
        evaluated->opponent_wins_next_move[target_index] = opponent_win_next;

        bool forced_self = false;
        if (config->enable_forcing_check && config->difficulty >= AI_DIFFICULTY_STANDARD) {
            forced_self =
                ai_board_creates_forced_immediate_win_postmove(&child, move.player, move.player, config->swap_rule);
            if (forced_self) {
                ai_has_forced_win = true;
            }
        }
        evaluated->forced_wins_self[target_index] = forced_self;

        bool opponent_forced = false;
        // Skip expensive opponent forcing check if AI already has a forced win move
        // (AI will play the forced win, so opponent's forcing potential is irrelevant)
        if (config->enable_forcing_check && config->difficulty == AI_DIFFICULTY_EXPERT && 
            !opponent_win_next && !ai_has_forced_win) {
            opponent_forced = ai_forcing_move_available(&child, defender, opponent, config->swap_rule);
        }
        evaluated->opponent_forced_wins[target_index] = opponent_forced;

        ai_config_t eval_config = *config;
        eval_config.enable_forcing_check = false;

        int16_t eval_score = ai_agent_evaluate_internal(&child, defender, config->ai_player, &eval_config, NULL);

        if (config->enable_forcing_check && opponent_forced) {
            eval_score = AI_SCORE_LOSS + (int16_t)(child.move_count & 0x7FFF) + 1000;
        }

        // Apply self-reversal penalty to final evaluation (prevents oscillation loops)
        // Only apply if not already a bad move (opponent wins next or forced loss)
        if (config->self_prev_from != 0xFF && !opponent_win_next && !opponent_forced) {
            uint8_t self_from_row = config->self_prev_from >> 4;
            uint8_t self_from_col = config->self_prev_from & 0x0F;
            uint8_t self_to_row = config->self_prev_to >> 4;
            uint8_t self_to_col = config->self_prev_to & 0x0F;
            // Check if this move reverses our own previous move
            if (move.to_row == self_from_row && move.to_col == self_from_col &&
                move.from_row == self_to_row && move.from_col == self_to_col) {
                // Strong penalty to prevent oscillation - proportional to avoid overflow
                eval_score -= 2000;  // Heavy penalty to break oscillation loops
            }
        }

        evaluated->evaluations[target_index] = eval_score;

        ++count;
    }

    return count;
}

#pragma code(code)

static uint8_t ai_evaluate_moves(board_t *root, player_t current_player, const ai_config_t *config, const ai_ordered_moves_t *ordered, uint8_t generated, ai_evaluated_moves_t *evaluated) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_11);
    uint8_t result = FAR_ai_evaluate_moves(root, current_player, config, ordered, generated, evaluated);
    POKE(OVERLAY_MMU_REG, saved);
    return result;
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
    board_context_t move_context; memset(&move_context, 0, sizeof(move_context)); move_context.current_player = current_player;
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
            ai_forcing_move_available(&child, move_context.current_player, opponent, config->swap_rule);
    }

    // Evaluate the position
    result->evaluation = ai_agent_evaluate_internal(&child, opponent, config->ai_player, config, NULL);
}

#pragma code(ovl10_code)

static bool FAR_ai_choose_move_from_evaluated(
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
            *out_move = move_make(evaluated->moves.from_rows[i], evaluated->moves.from_cols[i], evaluated->moves.to_rows[i], evaluated->moves.to_cols[i], evaluated->moves.types[i], evaluated->moves.players[i]);
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
            *out_move = move_make(evaluated->moves.from_rows[best_forced_index], evaluated->moves.from_cols[best_forced_index], evaluated->moves.to_rows[best_forced_index], evaluated->moves.to_cols[best_forced_index], evaluated->moves.types[best_forced_index], evaluated->moves.players[best_forced_index]);
            return true;
        }

        if (config->difficulty == AI_DIFFICULTY_STANDARD) {
            bool skip_forced = base_blunder_enabled && ai_random_chance(config->blunder_chance_pct);
            if (!skip_forced) {
                *out_move = move_make(evaluated->moves.from_rows[best_forced_index], evaluated->moves.from_cols[best_forced_index], evaluated->moves.to_rows[best_forced_index], evaluated->moves.to_cols[best_forced_index], evaluated->moves.types[best_forced_index], evaluated->moves.players[best_forced_index]);
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
            *out_move = move_make(evaluated->moves.from_rows[blunder_idx], evaluated->moves.from_cols[blunder_idx], evaluated->moves.to_rows[blunder_idx], evaluated->moves.to_cols[blunder_idx], evaluated->moves.types[blunder_idx], evaluated->moves.players[blunder_idx]);
            if (applied_blunder) {
                *applied_blunder = true;
            }
            return true;
        }

        // Two-stage move selection:
        // 1. Top-K scoring tiers: With epsilon chance, pick a lower-ranked score tier
        // 2. Random among equivalent: Randomly select among moves sharing the chosen score
        
        // First, sort safe moves by evaluation score (descending)
        uint8_t sorted_indices[AI_MAX_ORDERED_MOVES];
        for (uint8_t i = 0u; i < safe_count; ++i) {
            sorted_indices[i] = safe_indices[i];
        }
        ai_sort_indices_by_evaluation(evaluated, sorted_indices, safe_count);
        
        // Build distinct score tiers (groups of moves with the same score)
        // Each tier contains moves with equivalent evaluation scores
        int16_t tier_scores[AI_MAX_ORDERED_MOVES];
        uint8_t tier_start[AI_MAX_ORDERED_MOVES];  // Start index in sorted_indices for each tier
        uint8_t tier_count[AI_MAX_ORDERED_MOVES];  // Number of moves in each tier
        uint8_t num_tiers = 0u;
        
        for (uint8_t i = 0u; i < safe_count; ++i) {
            int16_t score = evaluated->evaluations[sorted_indices[i]];
            if (num_tiers == 0u || score != tier_scores[num_tiers - 1u]) {
                // New tier
                tier_scores[num_tiers] = score;
                tier_start[num_tiers] = i;
                tier_count[num_tiers] = 1u;
                num_tiers++;
            } else {
                // Same score as previous, extend current tier
                tier_count[num_tiers - 1u]++;
            }
        }
        
        // Stage 1: Select which score tier to use
        // Default to best tier (tier 0), but with epsilon chance pick from top-k tiers
        uint8_t selected_tier = 0u;
        if (!use_hint && config->random_top_k > 1u && config->random_epsilon_pct > 0u && 
            num_tiers > 1u && ai_random_chance(config->random_epsilon_pct)) {
            // Pick randomly among top-k distinct score tiers
            uint8_t tier_limit = config->random_top_k;
            if (tier_limit > num_tiers) {
                tier_limit = num_tiers;
            }
            selected_tier = (uint8_t)ai_random_range(tier_limit);
        }
        
        // Stage 2: Randomly select among moves in the chosen tier
        uint8_t tier_base = tier_start[selected_tier];
        uint8_t tier_size = tier_count[selected_tier];
        uint8_t choice_in_tier = 0u;
        if (tier_size > 1u && !use_hint) {
            choice_in_tier = (uint8_t)ai_random_range(tier_size);
        }
        
        uint8_t chosen_index = sorted_indices[tier_base + choice_in_tier];

        *out_move = move_make(evaluated->moves.from_rows[chosen_index], evaluated->moves.from_cols[chosen_index], evaluated->moves.to_rows[chosen_index], evaluated->moves.to_cols[chosen_index], evaluated->moves.types[chosen_index], evaluated->moves.players[chosen_index]);
        return true;
    }

    // If no safe moves and no blunder moves, return false (no valid moves)
    return false;
}

#pragma code(code)


#pragma code(ovl10_code)

bool FAR_ai_agent_find_best_move_impl(const board_t *board,
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
    uint8_t generated = FAR_ai_generate_moves(&root, current_player, &tuned, &ordered);
    if (generated == 0u) {
        s_last_breakdown = (ai_eval_breakdown_t){0};
        return false;
    }

    if (tuned.enable_forcing_check && ai_all_replies_allow_opponent_immediate_win_from_ordered(&ordered, generated)) {
        tuned.enable_forcing_check = false;
    }

    ai_evaluated_moves_t evaluated;
    uint8_t evaluated_count =
        ai_evaluate_moves(&root, current_player, &tuned, &ordered, generated, &evaluated);

    bool applied_blunder = false;
    move_t chosen_move = {0};
    bool move_found =
        FAR_ai_choose_move_from_evaluated(&tuned, &evaluated, evaluated_count, &chosen_move, &applied_blunder);

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
                chosen_move = move_make(ordered.moves.from_rows[slot], ordered.moves.from_cols[slot], ordered.moves.to_rows[slot], ordered.moves.to_cols[slot], ordered.moves.types[slot], ordered.moves.players[slot]);
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
            chosen_move = move_make(ordered.moves.from_rows[slot], ordered.moves.from_cols[slot], ordered.moves.to_rows[slot], ordered.moves.to_cols[slot], ordered.moves.types[slot], ordered.moves.players[slot]);
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
        board_context_t dummy_context; memset(&dummy_context, 0, sizeof(dummy_context)); dummy_context.current_player = tuned.ai_player;
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

#pragma code(code)

bool ai_agent_find_best_move_impl(const board_t *board, player_t current_player, const ai_config_t *config, move_t *out_move) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_10);
    bool result = FAR_ai_agent_find_best_move_impl(board, current_player, config, out_move);
    POKE(OVERLAY_MMU_REG, saved);
    return result;
}

bool ai_agent_find_best_move(const board_t *board, const board_context_t *context, const ai_config_t *config,
                             move_t *out_move) {
    if (!board || !context || !config || !out_move) {
        return false;
    }

    // Inject move history for anti-reversal (lightweight copy)
    // NOTE: history is stored in reverse order - history[0] is the most recent move
    ai_config_t augmented = *config;
    
    // Opponent's last move (1 ply ago) - stored at history[0]
    if (context->history_count > 0) {
        const move_t *last = &context->history[0];
        augmented.last_opp_from = (last->from_row << 4) | last->from_col;
        augmented.last_opp_to = (last->to_row << 4) | last->to_col;
    } else {
        augmented.last_opp_from = 0xFF;  // Sentinel: no last move
    }
    
    // Own move from 2 plies ago (for self-reversal detection) - stored at history[1]
    if (context->history_count >= 2) {
        const move_t *own_prev = &context->history[1];
        augmented.self_prev_from = (own_prev->from_row << 4) | own_prev->from_col;
        augmented.self_prev_to = (own_prev->to_row << 4) | own_prev->to_col;
    } else {
        augmented.self_prev_from = 0xFF;  // Sentinel: no previous own move
    }

    return ai_agent_find_best_move_impl(board, context->current_player, &augmented, out_move);
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
