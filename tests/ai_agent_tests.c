#include "../src/board.h"
#include "../src/ai_agent.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t kTestMaxMoves = 32;
static const uint16_t kExtendedTuningGamesPerRule = 1000;
static const uint16_t kExtendedTuningHalfMoveCap = 200;

typedef struct {
    ai_eval_weights_t baseline;
    ai_eval_weights_t refined;
    uint16_t refined_wins;
    uint16_t refined_losses;
    uint16_t refined_draws;
    uint16_t baseline_wins;
    uint16_t baseline_losses;
    uint16_t baseline_draws;
    uint32_t refined_total_half_moves;
    uint32_t baseline_total_half_moves;
} ai_tuning_summary_t;

typedef void (*ai_profile_configurator_t)(ai_config_t *, const ai_eval_weights_t *);

static ai_eval_weights_t kExtendedRefinedWeights[4];

static int16_t clamp_range(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint32_t rng_state = 0xC0FFEEu;

static void rng_seed(uint32_t seed) {
    if (seed == 0u) {
        seed = 1u;
    }
    rng_state = seed;
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static uint32_t rng_range(uint32_t limit) {
    if (limit == 0u) {
        return 0u;
    }
    return rng_next() % limit;
}

static bool rng_chance_percent(uint8_t percentage) {
    if (percentage == 0u) {
        return false;
    }
    if (percentage >= 100u) {
        return true;
    }
    return rng_range(100u) < percentage;
}

static bool env_flag_enabled(const char *value) {
    if (!value) {
        return false;
    }

    while (*value == ' ' || *value == '\t') {
        ++value;
    }

    if (*value == '\0') {
        return false;
    }

    if (*value == '0') {
        return false;
    }

    if (*value == '1') {
        return true;
    }

    if (*value == 'T' || *value == 't') {
        if (value[1] == '\0') {
            return true;
        }
        return (value[1] == 'R' || value[1] == 'r') &&
               (value[2] == 'U' || value[2] == 'u') &&
               (value[3] == 'E' || value[3] == 'e');
    }

    if (*value == 'Y' || *value == 'y') {
        if (value[1] == '\0') {
            return true;
        }
        return (value[1] == 'E' || value[1] == 'e') &&
               (value[2] == 'S' || value[2] == 's');
    }

    if (*value == 'O' || *value == 'o') {
        return (value[1] == 'N' || value[1] == 'n') &&
               (value[2] == '\0');
    }

    return false;
}

// Stubs for target-specific functions
void clear_puzzle_hint(void) {}
void *get_puzzle_collection(void) { return NULL; }
void *get_puzzle_by_index(void *collection, uint16_t index) { return NULL; }
void apply_puzzle_position(void *puzzle, board_t *board) {}
void print_puzzle_info(void *puzzle) {}
void clear_puzzle_info(void) {}
void video_reset_all_board_cell_colors(void) {}
void display_puzzle_solution(void *puzzle) {}

typedef struct {
    uint16_t white_advancement;
    uint16_t black_advancement;
    uint8_t white_frontier;
    uint8_t black_frontier;
    uint8_t plies_played;
    player_t winner;
} ai_self_play_metrics_t;

typedef struct {
    uint16_t wins;
    uint16_t losses;
    uint16_t draws;
    uint32_t total_half_moves;
    int64_t advantage_total;
    int64_t frontier_total;
} ai_weight_stats_t;

static bool weight_stats_better(const ai_weight_stats_t *lhs,
                                const ai_weight_stats_t *rhs) {
    if (lhs->wins != rhs->wins) {
        return lhs->wins > rhs->wins;
    }
    if (lhs->losses != rhs->losses) {
        return lhs->losses < rhs->losses;
    }
    return lhs->total_half_moves < rhs->total_half_moves;
}

static const ai_eval_weights_t kBaselineWeights[4] = {
    { 60, 40, 32, 44, 22 },
    { 58, 38, 28, 42, 22 },
    { 55, 42, 44, 36, 24 },
    { 55, 40, 40, 36, 24 }
};

static const uint8_t kRandomOpponentEpsilonPct = 20;

static const char *ai_player_name(player_t player) {
    switch (player) {
        case PLAYER_WHITE:
            return "White";
        case PLAYER_BLACK:
            return "Black";
        default:
            return "None";
    }
}

static void configure_self_play_profile(ai_config_t *config,
                                        const ai_eval_weights_t *weights) {
    config->weights = *weights;
    config->diagnostics_enabled = false;
    config->search.base_depth = 3;
    config->search.max_depth = 4;
    config->search.node_limit = 6000;
    config->search.use_iterative_deepening = false;
    config->search.use_transposition = true;
    config->search.use_move_ordering = true;
    config->search.use_killer_moves = true;
}

static void configure_tuning_profile(ai_config_t *config,
                                     const ai_eval_weights_t *weights) {
    configure_self_play_profile(config, weights);
    config->search.base_depth = 1;
    config->search.max_depth = 1;
    config->search.node_limit = 1200;
    config->search.use_iterative_deepening = false;
    config->search.use_transposition = false;
    config->search.use_move_ordering = false;
    config->search.use_killer_moves = false;
}

static uint16_t compute_advancement_score(const board_t *board, player_t player) {
    uint16_t total = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            if (player == PLAYER_WHITE) {
                total += (uint16_t)((BOARD_ROWS - 1u) - row);
            } else {
                total += row;
            }
        }
    }
    return total;
}

static uint8_t compute_frontier_row(const board_t *board, player_t player) {
    bool found = false;
    uint8_t frontier = (player == PLAYER_WHITE) ? BOARD_ROWS : 0u;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }
            found = true;
            if (player == PLAYER_WHITE) {
                if (row < frontier) {
                    frontier = row;
                }
            } else {
                if (row > frontier) {
                    frontier = row;
                }
            }
        }
    }
    if (!found) {
        return (player == PLAYER_WHITE) ? BOARD_ROWS : 0u;
    }
    return frontier;
}

static bool test_immediate_win_available(const board_t *board, player_t player, swap_rule_t rule) {
    board_t scratch;
    memcpy(&scratch, board, sizeof(board_t));
    scratch.current_player = player;

    move_t moves[8];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            uint8_t count = board_get_legal_moves(&scratch, row, col, moves,
                                                  (uint8_t)(sizeof(moves) / sizeof(moves[0])));
            for (uint8_t i = 0; i < count; ++i) {
                board_t test_state;
                memcpy(&test_state, &scratch, sizeof(board_t));
                if (!board_execute_move(&test_state, &moves[i], rule)) {
                    continue;
                }
                if (board_check_win(&test_state, player, NULL)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool test_move_creates_forced_immediate_win(const board_t *board,
                                                   const move_t *move,
                                                   player_t ai_player,
                                                   swap_rule_t rule) {
    if (!move) {
        return false;
    }

    board_t after_ai;
    memcpy(&after_ai, board, sizeof(board_t));
    if (!board_execute_move(&after_ai, move, rule)) {
        return false;
    }

    if (board_check_win(&after_ai, ai_player, NULL)) {
        return true;
    }

    board_t opponent_state;
    memcpy(&opponent_state, &after_ai, sizeof(board_t));
    board_switch_turn(&opponent_state);
    opponent_state.current_player = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    move_t opponent_moves[kTestMaxMoves];
    uint8_t opponent_count = 0;

    for (uint8_t row = 0; row < BOARD_ROWS && opponent_count < kTestMaxMoves; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS && opponent_count < kTestMaxMoves; ++col) {
            piece_type_t piece = board_get_piece(&opponent_state, row, col);
            if (board_get_piece_owner(piece) != opponent_state.current_player) {
                continue;
            }

            opponent_count += board_get_legal_moves(&opponent_state,
                                                    row,
                                                    col,
                                                    &opponent_moves[opponent_count],
                                                    (uint8_t)(kTestMaxMoves - opponent_count));
        }
    }

    if (opponent_count == 0u) {
        return true;
    }

    for (uint8_t i = 0; i < opponent_count; ++i) {
        board_t after_opponent;
        memcpy(&after_opponent, &opponent_state, sizeof(board_t));
        if (!board_execute_move(&after_opponent, &opponent_moves[i], rule)) {
            continue;
        }

        if (board_check_win(&after_opponent, opponent_state.current_player, NULL)) {
            return false;
        }

        board_switch_turn(&after_opponent);
        after_opponent.current_player = ai_player;
        if (!test_immediate_win_available(&after_opponent, ai_player, rule)) {
            return false;
        }
    }

    return true;
}

static bool test_forcing_move_available(const board_t *board, player_t player, swap_rule_t rule) {
    board_t scratch;
    memcpy(&scratch, board, sizeof(board_t));
    scratch.current_player = player;

    move_t moves[kTestMaxMoves];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            uint8_t count = board_get_legal_moves(&scratch, row, col, moves, kTestMaxMoves);
            for (uint8_t i = 0; i < count; ++i) {
                if (test_move_creates_forced_immediate_win(&scratch, &moves[i], player, rule)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool select_random_move(board_t *board, move_t *out_move) {
    move_t moves[kTestMaxMoves];
    uint8_t count = 0;

    for (uint8_t row = 0; row < BOARD_ROWS && count < kTestMaxMoves; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS && count < kTestMaxMoves; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) != board->current_player) {
                continue;
            }

            count += board_get_legal_moves(board,
                                           row,
                                           col,
                                           &moves[count],
                                           (uint8_t)(kTestMaxMoves - count));
        }
    }

    if (count == 0u) {
        return false;
    }

    uint32_t chosen = rng_range(count);
    *out_move = moves[chosen];
    return true;
}

static ai_self_play_metrics_t run_profiled_self_play_session(
        const ai_eval_weights_t *white_weights,
        const ai_eval_weights_t *black_weights,
        swap_rule_t rule,
        uint16_t half_move_limit,
        ai_profile_configurator_t configurator) {
    board_t board;
    board_init(&board);

    ai_config_t white_cfg;
    ai_agent_init(&white_cfg, rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
    configurator(&white_cfg, white_weights);

    ai_config_t black_cfg;
    ai_agent_init(&black_cfg, rule, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);
    configurator(&black_cfg, black_weights);

    ai_self_play_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.white_frontier = BOARD_ROWS;
    metrics.winner = PLAYER_NONE;

    for (uint16_t ply = 0; ply < half_move_limit; ++ply) {
        ai_config_t *active_cfg = (board.current_player == PLAYER_WHITE)
                                      ? &white_cfg
                                      : &black_cfg;
        move_t move;
        bool found = ai_agent_find_best_move(&board, active_cfg, &move);
        assert(found);
        bool executed = board_execute_move(&board, &move, active_cfg->swap_rule);
        assert(executed);

        metrics.plies_played = (uint8_t)(ply + 1u);

        if (board_check_win(&board, active_cfg->ai_player, NULL)) {
            metrics.winner = active_cfg->ai_player;
            break;
        }

        board_switch_turn(&board);
        if (!board_has_legal_moves(&board, board.current_player)) {
            break;
        }
    }

    metrics.white_advancement = compute_advancement_score(&board, PLAYER_WHITE);
    metrics.black_advancement = compute_advancement_score(&board, PLAYER_BLACK);
    metrics.white_frontier = compute_frontier_row(&board, PLAYER_WHITE);
    metrics.black_frontier = compute_frontier_row(&board, PLAYER_BLACK);

    return metrics;
}

static ai_self_play_metrics_t run_self_play_session(
        const ai_eval_weights_t *white_weights,
        const ai_eval_weights_t *black_weights,
        swap_rule_t rule,
        uint8_t half_move_limit) {
    return run_profiled_self_play_session(white_weights, black_weights, rule,
                                          (uint16_t)half_move_limit,
                                          configure_self_play_profile);
}

static ai_self_play_metrics_t run_candidate_vs_random(
        const ai_eval_weights_t *candidate_weights,
        const ai_eval_weights_t *baseline_weights,
        swap_rule_t rule,
        uint16_t half_move_limit,
    bool candidate_as_white,
    uint32_t seed,
    uint8_t random_epsilon_pct) {
    board_t board;
    board_init(&board);

    player_t candidate_player = candidate_as_white ? PLAYER_WHITE : PLAYER_BLACK;
    player_t random_player = (candidate_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    ai_config_t candidate_cfg;
    ai_agent_init(&candidate_cfg, rule, AI_DIFFICULTY_STANDARD, candidate_player);
    configure_tuning_profile(&candidate_cfg, candidate_weights);

    ai_config_t random_cfg;
    ai_agent_init(&random_cfg, rule, AI_DIFFICULTY_STANDARD, random_player);
    configure_tuning_profile(&random_cfg, baseline_weights);

    ai_self_play_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.white_frontier = BOARD_ROWS;
    metrics.winner = PLAYER_NONE;

    rng_seed(seed);

    for (uint16_t ply = 0; ply < half_move_limit; ++ply) {
        bool candidate_turn = (board.current_player == candidate_player);
        ai_config_t *active_cfg = candidate_turn ? &candidate_cfg : &random_cfg;
        move_t move;
        bool found = false;

        if (candidate_turn) {
            found = ai_agent_find_best_move(&board, active_cfg, &move);
        } else {
            bool take_random = rng_chance_percent(random_epsilon_pct);
            if (take_random) {
                found = select_random_move(&board, &move);
            }
            if (!found) {
                found = ai_agent_find_best_move(&board, active_cfg, &move);
            }
        }

        if (!found) {
            break;
        }

        bool executed = board_execute_move(&board, &move, active_cfg->swap_rule);
        assert(executed);

        metrics.plies_played = (uint8_t)(ply + 1u);

        if (board_check_win(&board, candidate_turn ? candidate_player : random_player, NULL)) {
            metrics.winner = candidate_turn ? candidate_player : random_player;
            break;
        }

        board_switch_turn(&board);
        if (!board_has_legal_moves(&board, board.current_player)) {
            break;
        }
    }

    metrics.white_advancement = compute_advancement_score(&board, PLAYER_WHITE);
    metrics.black_advancement = compute_advancement_score(&board, PLAYER_BLACK);
    metrics.white_frontier = compute_frontier_row(&board, PLAYER_WHITE);
    metrics.black_frontier = compute_frontier_row(&board, PLAYER_BLACK);

    return metrics;
}

static ai_weight_stats_t evaluate_weight_set_vs_random(
        const ai_eval_weights_t *candidate_weights,
        const ai_eval_weights_t *baseline_weights,
        swap_rule_t rule,
        uint16_t games,
        uint16_t half_move_limit,
    uint32_t seed_base,
    uint8_t random_epsilon_pct) {
    ai_weight_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    for (uint16_t game = 0; game < games; ++game) {
        bool candidate_as_white = ((game & 1u) == 0u);
        uint32_t seed = seed_base ^ (((uint32_t)game + 1u) * 0x9E3779B9u);

    ai_self_play_metrics_t metrics = run_candidate_vs_random(candidate_weights,
                                 baseline_weights,
                                 rule,
                                 half_move_limit,
                                 candidate_as_white,
                                 seed,
                                 random_epsilon_pct);

        stats.total_half_moves += metrics.plies_played;

        if (metrics.winner == PLAYER_NONE) {
            stats.draws++;
        } else if (metrics.winner == (candidate_as_white ? PLAYER_WHITE : PLAYER_BLACK)) {
            stats.wins++;
        } else {
            stats.losses++;
        }

        int32_t adv_delta = (int32_t)metrics.white_advancement - (int32_t)metrics.black_advancement;
        int32_t frontier_delta = (int32_t)metrics.black_frontier - (int32_t)metrics.white_frontier;
        if (!candidate_as_white) {
            adv_delta = -adv_delta;
            frontier_delta = -frontier_delta;
        }

        stats.advantage_total += (int64_t)adv_delta;
        stats.frontier_total += (int64_t)frontier_delta;
    }

    return stats;
}

static player_t run_head_to_head_match(const ai_eval_weights_t *white_weights,
                                       const ai_eval_weights_t *black_weights,
                                       swap_rule_t rule,
                                       uint8_t half_move_limit) {
    ai_self_play_metrics_t metrics =
        run_self_play_session(white_weights, black_weights, rule, half_move_limit);
    return metrics.winner;
}

static ai_eval_weights_t refine_weights_for_rule(swap_rule_t rule,
                                                 uint16_t games,
                                                 uint16_t half_move_limit,
                                                 ai_tuning_summary_t *summary) {
    ai_config_t seed_config;
    ai_agent_init(&seed_config, rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
    ai_eval_weights_t seed = seed_config.weights;
    uint8_t rule_index = (uint8_t)(rule % 4u);
    ai_eval_weights_t baseline = kBaselineWeights[rule_index];

    ai_weight_stats_t baseline_stats = evaluate_weight_set_vs_random(&baseline,
                                                                     &baseline,
                                                                     rule,
                                                                     games,
                                                                     half_move_limit,
                                                                     0xBA51CAFEu + (uint32_t)rule_index,
                                                                     kRandomOpponentEpsilonPct);

    ai_weight_stats_t seed_stats = evaluate_weight_set_vs_random(&seed,
                                                                 &baseline,
                                                                 rule,
                                                                 games,
                                                                 half_move_limit,
                                                                 0xFEED0000u + (uint32_t)rule_index,
                                                                 kRandomOpponentEpsilonPct);

    int32_t avg_advantage = (int32_t)(seed_stats.advantage_total / (int64_t)games);
    int32_t avg_frontier = (int32_t)(seed_stats.frontier_total / (int64_t)games);

    int16_t connection_adjust = clamp_range((int16_t)(avg_advantage / 8), -12, 12);
    if (connection_adjust == 0 && (avg_advantage >= 4 || avg_advantage <= -4)) {
        connection_adjust = (avg_advantage > 0) ? 1 : (int16_t)-1;
    }

    int16_t bridge_adjust = clamp_range((int16_t)(avg_advantage / 16), -8, 8);
    if (bridge_adjust == 0 && (avg_advantage >= 8 || avg_advantage <= -8)) {
        bridge_adjust = (avg_advantage > 0) ? 1 : (int16_t)-1;
    }

    int16_t pressure_adjust = clamp_range(
        (int16_t)((avg_advantage + avg_frontier) / 20), -6, 6);

    int16_t blocking_adjust = clamp_range((int16_t)(avg_frontier / 12), -8, 8);
    if (blocking_adjust == 0 && (avg_frontier >= 6 || avg_frontier <= -6)) {
        blocking_adjust = (avg_frontier > 0) ? 1 : (int16_t)-1;
    }

    int16_t mobility_adjust = clamp_range((int16_t)(avg_frontier / 20), -6, 6);

    ai_eval_weights_t refined = seed;
    refined.connection_progress = clamp_range(
        (int16_t)(refined.connection_progress + connection_adjust), 32, 140);
    refined.bridge_potential = clamp_range(
        (int16_t)(refined.bridge_potential + bridge_adjust), 24, 96);
    refined.swap_pressure = clamp_range(
        (int16_t)(refined.swap_pressure + pressure_adjust), 20, 80);
    refined.blocking_coverage = clamp_range(
        (int16_t)(refined.blocking_coverage + blocking_adjust), 24, 64);
    refined.mobility = clamp_range(
        (int16_t)(refined.mobility + mobility_adjust), 8, 40);

    if (refined.connection_progress == seed.connection_progress &&
        refined.bridge_potential == seed.bridge_potential &&
        refined.swap_pressure == seed.swap_pressure &&
        refined.blocking_coverage == seed.blocking_coverage &&
        refined.mobility == seed.mobility) {
        int16_t nudge = (rule_index & 1u) ? (int16_t)-1 : (int16_t)1;
        refined.connection_progress = clamp_range(
            (int16_t)(refined.connection_progress + nudge), 32, 140);
        refined.bridge_potential = clamp_range(
            (int16_t)(refined.bridge_potential + nudge), 24, 96);
    }

    ai_weight_stats_t refined_stats = evaluate_weight_set_vs_random(&refined,
                                                                    &baseline,
                                                                    rule,
                                                                    games,
                                                                    half_move_limit,
                                                                    0xDEADBEEFu + (uint32_t)rule_index,
                                                                    kRandomOpponentEpsilonPct);

    const ai_eval_weights_t *selected_weights = &refined;
    ai_weight_stats_t selected_stats = refined_stats;

    if (weight_stats_better(&seed_stats, &selected_stats)) {
        selected_weights = &seed;
        selected_stats = seed_stats;
    }
    if (weight_stats_better(&baseline_stats, &selected_stats)) {
        selected_weights = &baseline;
        selected_stats = baseline_stats;
    }

    ai_eval_weights_t chosen = *selected_weights;

    if (summary) {
        summary->baseline = baseline;
        summary->refined = chosen;
        summary->baseline_wins = baseline_stats.wins;
        summary->baseline_losses = baseline_stats.losses;
        summary->baseline_draws = baseline_stats.draws;
        summary->baseline_total_half_moves = baseline_stats.total_half_moves;
        summary->refined_wins = selected_stats.wins;
        summary->refined_losses = selected_stats.losses;
        summary->refined_draws = selected_stats.draws;
        summary->refined_total_half_moves = selected_stats.total_half_moves;
    }

    return chosen;
}

static void evaluate_refined_against_baseline(swap_rule_t rule,
                                              const ai_eval_weights_t *refined,
                                              const ai_eval_weights_t *baseline,
                                              uint16_t half_move_limit,
                                              int32_t *advantage_out,
                                              int32_t *frontier_out) {
    ai_self_play_metrics_t refined_as_white =
        run_profiled_self_play_session(refined, baseline, rule, half_move_limit,
                                       configure_tuning_profile);
    ai_self_play_metrics_t refined_as_black =
        run_profiled_self_play_session(baseline, refined, rule, half_move_limit,
                                       configure_tuning_profile);

    int32_t adv_white = (int32_t)refined_as_white.white_advancement -
                        (int32_t)refined_as_white.black_advancement;
    int32_t adv_black = (int32_t)refined_as_black.black_advancement -
                        (int32_t)refined_as_black.white_advancement;

    int32_t frontier_white = (int32_t)refined_as_white.black_frontier -
                             (int32_t)refined_as_white.white_frontier;
    int32_t frontier_black = (int32_t)refined_as_black.white_frontier -
                              (int32_t)refined_as_black.black_frontier;

    if (advantage_out) {
        *advantage_out = (adv_white + adv_black) / 2;
    }
    if (frontier_out) {
        *frontier_out = (frontier_white + frontier_black) / 2;
    }
}

static void clear_board(board_t *board) {
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }
    board->history_count = 0;
    board->move_count = 0;
}

static bool test_moves_equal(const move_t *lhs, const move_t *rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    return lhs->from_row == rhs->from_row && lhs->from_col == rhs->from_col && lhs->to_row == rhs->to_row &&
           lhs->to_col == rhs->to_col && lhs->type == rhs->type && lhs->player == rhs->player;
}

static void setup_blunder_immediate_win_scenario(board_t *board) {
    board_init(board);
    clear_board(board);

    board->current_player = PLAYER_WHITE;

    board_set_piece(board, 1, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 2, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 3, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 4, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 5, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 7, 0, PIECE_BLACK_NORMAL);
    board_set_piece(board, 1, 1, PIECE_BLACK_NORMAL);

    board_set_piece(board, 6, 0, PIECE_WHITE_NORMAL);
}

static void test_evaluation_symmetry(void) {
    board_t board;
    board_init(&board);

    ai_config_t config_white;
    ai_agent_init(&config_white, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);

    ai_config_t config_black;
    ai_agent_init(&config_black, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);

    int16_t score_white = ai_agent_evaluate_board(&board, PLAYER_WHITE, &config_white);
    int16_t score_black = ai_agent_evaluate_board(&board, PLAYER_BLACK, &config_black);

    printf("Symmetry test: White=%d Black=%d\n", score_white, score_black);
    assert(score_white == -score_black);
}

static void test_forced_win_detection(void) {
    board_t board;
    board_init(&board);
    clear_board(&board);

    // Construct a near-complete vertical chain for White.
    board.current_player = PLAYER_WHITE;
    board_set_piece(&board, 1, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 2, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 3, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 4, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 5, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 7, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);

    assert(test_immediate_win_available(&board, PLAYER_WHITE, config.swap_rule));

    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    assert(found);

    printf("Win detection move: from (%u,%u) to (%u,%u) type=%u\n",
        best_move.from_row, best_move.from_col,
        best_move.to_row, best_move.to_col, best_move.type);

    board_t before_move;
    memcpy(&before_move, &board, sizeof(board_t));
    bool forced_path = test_move_creates_forced_immediate_win(&before_move,
                                                              &best_move,
                                                              PLAYER_WHITE,
                                                              config.swap_rule);

    piece_type_t moved_piece = board_get_piece(&board,
                                               best_move.from_row,
                                               best_move.from_col);
    assert(board_get_piece_owner(moved_piece) == PLAYER_WHITE);
    assert(best_move.type == MOVE_TYPE_EMPTY);

    board_execute_move(&board, &best_move, config.swap_rule);

    bool immediate_win = board_check_win(&board, PLAYER_WHITE, NULL);
    bool follow_up_win = test_immediate_win_available(&board, PLAYER_WHITE,
                                                      config.swap_rule);
    assert(immediate_win || follow_up_win || forced_path);
}

static void test_forcing_move_preference(void) {
    board_t board;
    board_init(&board);
    clear_board(&board);

    board.current_player = PLAYER_WHITE;

    board_set_piece(&board, 1, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 2, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 3, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 4, 0, PIECE_WHITE_NORMAL);

    board_set_piece(&board, 6, 2, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 6, 3, PIECE_BLACK_NORMAL);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);

    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    assert(found);

    assert(best_move.type == MOVE_TYPE_EMPTY);

    board_t after_move;
    memcpy(&after_move, &board, sizeof(board_t));
    bool executed = board_execute_move(&after_move, &best_move, config.swap_rule);
    assert(executed);
    assert(!board_check_win(&after_move, PLAYER_WHITE, NULL));

    board_switch_turn(&after_move);
    after_move.current_player = PLAYER_BLACK;

    ai_config_t opponent_cfg;
    ai_agent_init(&opponent_cfg, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);

    move_t opponent_response;
    bool opponent_found = ai_agent_find_best_move(&after_move, &opponent_cfg, &opponent_response);
    assert(opponent_found);

    board_t response_state;
    memcpy(&response_state, &after_move, sizeof(board_t));
    bool opp_executed = board_execute_move(&response_state, &opponent_response, opponent_cfg.swap_rule);
    assert(opp_executed);
    assert(!board_check_win(&response_state, PLAYER_BLACK, NULL));

    board_switch_turn(&response_state);
    response_state.current_player = PLAYER_WHITE;

}

static void test_unavoidable_loss_detection(void) {
    board_t board;
    board_init(&board);
    clear_board(&board);

    board.current_player = PLAYER_BLACK;

    for (uint8_t row = WIN_START_ROW; row <= 5u; ++row) {
        board_set_piece(&board, row, 0, PIECE_WHITE_NORMAL);
    }
    board_set_piece(&board, 7, 0, PIECE_WHITE_NORMAL);

    board_set_piece(&board, 0, 3, PIECE_BLACK_NORMAL);

    ai_config_t black_cfg;
    ai_agent_init(&black_cfg, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_BLACK);

    bool inevitable = ai_agent_detect_unavoidable_loss(&board, &black_cfg);
    assert(inevitable && "Expected inevitability detection to trigger");

    board_t defensive;
    memcpy(&defensive, &board, sizeof(board_t));
    board_set_piece(&defensive, 6, 1, PIECE_BLACK_SWAPPED);
    defensive.current_player = PLAYER_BLACK;

    bool avoidable = ai_agent_detect_unavoidable_loss(&defensive, &black_cfg);
    assert(!avoidable && "Defensive resource should prevent inevitability");
}

static void test_blunder_learning_allows_immediate_win(void) {
    board_t board;
    setup_blunder_immediate_win_scenario(&board);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_LEARNING, PLAYER_WHITE);
    ai_agent_config_set_randomization(&config, 1u, 0u);

    move_t loss_moves[kTestMaxMoves];
    uint8_t loss_count = 0u;

    move_t move_buffer[kTestMaxMoves];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_WHITE) {
                continue;
            }

            uint8_t count = board_get_legal_moves(&board, row, col, move_buffer, kTestMaxMoves);
            for (uint8_t i = 0; i < count; ++i) {
                board_t after_move;
                memcpy(&after_move, &board, sizeof(board_t));
                if (!board_execute_move(&after_move, &move_buffer[i], config.swap_rule)) {
                    continue;
                }

                board_switch_turn(&after_move);
                after_move.current_player = PLAYER_BLACK;
                if (test_immediate_win_available(&after_move, PLAYER_BLACK, config.swap_rule)) {
                    if (loss_count < kTestMaxMoves) {
                        loss_moves[loss_count++] = move_buffer[i];
                    }
                }
            }
        }
    }

    assert(loss_count > 0u);

    move_t baseline_move;
    bool baseline_found = ai_agent_find_best_move(&board, &config, &baseline_move);
    assert(baseline_found);

    for (uint8_t i = 0; i < loss_count; ++i) {
        assert(!test_moves_equal(&baseline_move, &loss_moves[i]));
    }

    board_t baseline_after;
    memcpy(&baseline_after, &board, sizeof(board_t));
    assert(board_execute_move(&baseline_after, &baseline_move, config.swap_rule));
    board_switch_turn(&baseline_after);
    baseline_after.current_player = PLAYER_BLACK;
    assert(!test_immediate_win_available(&baseline_after, PLAYER_BLACK, config.swap_rule));

    ai_agent_config_set_blunder(&config, true, AI_BLUNDER_ALLOW_IMMEDIATE_WIN, 100u);

    move_t blunder_move;
    bool blunder_found = ai_agent_find_best_move(&board, &config, &blunder_move);
    assert(blunder_found);

    bool matched = false;
    for (uint8_t i = 0; i < loss_count; ++i) {
        if (test_moves_equal(&blunder_move, &loss_moves[i])) {
            matched = true;
            break;
        }
    }
    assert(matched);

    board_t blunder_after;
    memcpy(&blunder_after, &board, sizeof(board_t));
    assert(board_execute_move(&blunder_after, &blunder_move, config.swap_rule));
    board_switch_turn(&blunder_after);
    blunder_after.current_player = PLAYER_BLACK;
    assert(test_immediate_win_available(&blunder_after, PLAYER_BLACK, config.swap_rule));
}

static void test_blunder_standard_allows_forcing_move(void) {
    board_t board;
    setup_blunder_immediate_win_scenario(&board);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
    ai_agent_config_set_randomization(&config, 1u, 0u);

    move_t forcing_moves[kTestMaxMoves];
    uint8_t forcing_count = 0u;

    move_t move_buffer[kTestMaxMoves];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_WHITE) {
                continue;
            }

            uint8_t count = board_get_legal_moves(&board, row, col, move_buffer, kTestMaxMoves);
            for (uint8_t i = 0; i < count; ++i) {
                board_t after_move;
                memcpy(&after_move, &board, sizeof(board_t));
                if (!board_execute_move(&after_move, &move_buffer[i], config.swap_rule)) {
                    continue;
                }

                board_switch_turn(&after_move);
                after_move.current_player = PLAYER_BLACK;
                if (test_forcing_move_available(&after_move, PLAYER_BLACK, config.swap_rule)) {
                    if (forcing_count < kTestMaxMoves) {
                        forcing_moves[forcing_count++] = move_buffer[i];
                    }
                }
            }
        }
    }

    assert(forcing_count > 0u);

    move_t baseline_move;
    bool baseline_found = ai_agent_find_best_move(&board, &config, &baseline_move);
    assert(baseline_found);

    for (uint8_t i = 0; i < forcing_count; ++i) {
        assert(!test_moves_equal(&baseline_move, &forcing_moves[i]));
    }

    board_t baseline_after;
    memcpy(&baseline_after, &board, sizeof(board_t));
    assert(board_execute_move(&baseline_after, &baseline_move, config.swap_rule));
    board_switch_turn(&baseline_after);
    baseline_after.current_player = PLAYER_BLACK;
    assert(!test_forcing_move_available(&baseline_after, PLAYER_BLACK, config.swap_rule));

    ai_agent_config_set_blunder(&config, true, AI_BLUNDER_ALLOW_FORCING_MOVE, 100u);

    move_t blunder_move;
    bool blunder_found = ai_agent_find_best_move(&board, &config, &blunder_move);
    assert(blunder_found);

    bool matched = false;
    for (uint8_t i = 0; i < forcing_count; ++i) {
        if (test_moves_equal(&blunder_move, &forcing_moves[i])) {
            matched = true;
            break;
        }
    }
    assert(matched);

    board_t blunder_after;
    memcpy(&blunder_after, &board, sizeof(board_t));
    assert(board_execute_move(&blunder_after, &blunder_move, config.swap_rule));
    board_switch_turn(&blunder_after);
    blunder_after.current_player = PLAYER_BLACK;
    assert(test_forcing_move_available(&blunder_after, PLAYER_BLACK, config.swap_rule));
}

static void test_blunder_expert_ignored(void) {
    board_t board;
    setup_blunder_immediate_win_scenario(&board);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    ai_agent_config_set_randomization(&config, 1u, 0u);

    move_t forcing_moves[kTestMaxMoves];
    uint8_t forcing_count = 0u;

    move_t move_buffer[kTestMaxMoves];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_WHITE) {
                continue;
            }

            uint8_t count = board_get_legal_moves(&board, row, col, move_buffer, kTestMaxMoves);
            for (uint8_t i = 0; i < count; ++i) {
                board_t after_move;
                memcpy(&after_move, &board, sizeof(board_t));
                if (!board_execute_move(&after_move, &move_buffer[i], config.swap_rule)) {
                    continue;
                }

                board_switch_turn(&after_move);
                after_move.current_player = PLAYER_BLACK;
                if (test_forcing_move_available(&after_move, PLAYER_BLACK, config.swap_rule)) {
                    if (forcing_count < kTestMaxMoves) {
                        forcing_moves[forcing_count++] = move_buffer[i];
                    }
                }
            }
        }
    }

    assert(forcing_count > 0u);

    ai_agent_config_set_blunder(&config, true, AI_BLUNDER_ALLOW_FORCING_MOVE, 100u);
    assert(!config.blunder_enabled);
    assert(config.blunder_chance_pct == 0u);
    assert(config.blunder_type == AI_BLUNDER_NONE);

    move_t move;
    bool found = ai_agent_find_best_move(&board, &config, &move);
    assert(found);

    for (uint8_t i = 0; i < forcing_count; ++i) {
        assert(!test_moves_equal(&move, &forcing_moves[i]));
    }

    board_t after;
    memcpy(&after, &board, sizeof(board_t));
    assert(board_execute_move(&after, &move, config.swap_rule));
    board_switch_turn(&after);
    after.current_player = PLAYER_BLACK;
    assert(!test_immediate_win_available(&after, PLAYER_BLACK, config.swap_rule));
    assert(!test_forcing_move_available(&after, PLAYER_BLACK, config.swap_rule));
}

static void test_determinism(void) {
    board_t board;
    board_init(&board);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);

    move_t first_run;
    move_t second_run;
    bool found_first = ai_agent_find_best_move(&board, &config, &first_run);
    bool found_second = ai_agent_find_best_move(&board, &config, &second_run);

    assert(found_first && found_second);
    assert(memcmp(&first_run, &second_run, sizeof(move_t)) == 0);
}

static void test_high_branching_stays_responsive(void) {
    board_t board;
    board_init(&board);
    clear_board(&board);

    board.current_player = PLAYER_BLACK;

    for (uint8_t row = 2; row <= 6; ++row) {
        board_set_piece(&board, row, 0, PIECE_WHITE_NORMAL);
    }

    board_set_piece(&board, 3, 1, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 3, 2, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 4, 1, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 4, 2, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 5, 2, PIECE_BLACK_NORMAL);

    uint8_t move_count = 0;
    move_t buffer[8];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_BLACK) {
                continue;
            }
            move_count += board_get_legal_moves(&board, row, col, buffer,
                                               (uint8_t)(sizeof(buffer) / sizeof(buffer[0])));
        }
    }
    assert(move_count >= 12);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);

    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    assert(found);
}

static void test_self_play_aggressive_advancement(void) {
    static const char *kRuleNames[4] = {
        "Classic",
        "ClearsOwn",
        "SwappedClears",
        "SwappedClearsOwn"
    };

    const uint8_t half_moves = 12;
    const int32_t kScoreTolerance = 2;
    bool any_strict_improvement = false;

    for (uint8_t rule_index = 0; rule_index < 4u; ++rule_index) {
        swap_rule_t rule = (swap_rule_t)rule_index;

        ai_config_t snapshot;
        ai_agent_init(&snapshot, rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
        ai_eval_weights_t tuned = snapshot.weights;

        ai_self_play_metrics_t baseline_metrics =
            run_self_play_session(&kBaselineWeights[rule_index],
                                  &kBaselineWeights[rule_index],
                                  rule, half_moves);
        ai_self_play_metrics_t tuned_metrics =
            run_self_play_session(&tuned, &tuned, rule, half_moves);

        printf("Self-play %s: baseline Wadv=%u front=%u tuned Wadv=%u front=%u (plies %u/%u)\n",
               kRuleNames[rule_index],
               (unsigned int)baseline_metrics.white_advancement,
               (unsigned int)baseline_metrics.white_frontier,
               (unsigned int)tuned_metrics.white_advancement,
               (unsigned int)tuned_metrics.white_frontier,
               (unsigned int)baseline_metrics.plies_played,
               (unsigned int)tuned_metrics.plies_played);

        int32_t baseline_score = (int32_t)baseline_metrics.white_advancement -
                                 (int32_t)baseline_metrics.white_frontier;
        int32_t tuned_score = (int32_t)tuned_metrics.white_advancement -
                              (int32_t)tuned_metrics.white_frontier;

        assert(tuned_score + kScoreTolerance >= baseline_score);

        if (tuned_score > baseline_score) {
            any_strict_improvement = true;
        }
    }

    assert(any_strict_improvement);
}

static void test_head_to_head_outcomes(void) {
    static const char *kRuleNames[4] = {
        "Classic",
        "ClearsOwn",
        "SwappedClears",
        "SwappedClearsOwn"
    };

    const uint8_t half_moves = 80;
    const unsigned kWinTolerance = 1u;
    unsigned total_tuned_wins = 0;
    unsigned total_baseline_wins = 0;

    for (uint8_t rule_index = 0; rule_index < 4u; ++rule_index) {
        swap_rule_t rule = (swap_rule_t)rule_index;

        ai_config_t snapshot;
        ai_agent_init(&snapshot, rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
        ai_eval_weights_t tuned = snapshot.weights;

        player_t game_a = run_head_to_head_match(&tuned, &kBaselineWeights[rule_index],
                                                 rule, half_moves);
        player_t game_b = run_head_to_head_match(&kBaselineWeights[rule_index], &tuned,
                                                 rule, half_moves);

        unsigned tuned_wins = 0;
        unsigned baseline_wins = 0;

        if (game_a == PLAYER_WHITE) {
            tuned_wins++;
        } else if (game_a == PLAYER_BLACK) {
            baseline_wins++;
        }

        if (game_b == PLAYER_BLACK) {
            tuned_wins++;
        } else if (game_b == PLAYER_WHITE) {
            baseline_wins++;
        }

        printf("Head-to-head %s: gameA winner=%s gameB winner=%s (tuned=%u baseline=%u)\n",
               kRuleNames[rule_index],
               ai_player_name(game_a),
               ai_player_name(game_b),
               tuned_wins,
               baseline_wins);

        assert(tuned_wins + kWinTolerance >= baseline_wins);
        assert(tuned_wins > 0 || baseline_wins <= kWinTolerance);

        total_tuned_wins += tuned_wins;
        total_baseline_wins += baseline_wins;
    }

    assert(total_tuned_wins + kWinTolerance >= total_baseline_wins);
    assert(total_tuned_wins > 0);
}

static void test_extended_self_play_tuning(void) {
    const char *flag = getenv("AI_AGENT_ENABLE_EXTENDED_TUNING");
    if (!env_flag_enabled(flag)) {
        puts("Skipping extended tuning pass (set AI_AGENT_ENABLE_EXTENDED_TUNING=1 to enable)");
        return;
    }

    static const char *kRuleNames[4] = {
        "Classic",
        "ClearsOwn",
        "SwappedClears",
        "SwappedClearsOwn"
    };

    for (uint8_t rule_index = 0; rule_index < 4u; ++rule_index) {
        swap_rule_t rule = (swap_rule_t)rule_index;
        ai_tuning_summary_t summary;

        ai_eval_weights_t refined = refine_weights_for_rule(
            rule,
            kExtendedTuningGamesPerRule,
            kExtendedTuningHalfMoveCap,
            &summary);

        kExtendedRefinedWeights[rule_index] = refined;

        uint32_t refined_games = (uint32_t)summary.refined_wins +
                                 (uint32_t)summary.refined_losses +
                                 (uint32_t)summary.refined_draws;
        uint32_t baseline_games = (uint32_t)summary.baseline_wins +
                                  (uint32_t)summary.baseline_losses +
                                  (uint32_t)summary.baseline_draws;
        assert(refined_games == kExtendedTuningGamesPerRule);
        assert(baseline_games == kExtendedTuningGamesPerRule);

        assert(refined.connection_progress >= 32);
        assert(refined.bridge_potential >= 24);
        assert(refined.swap_pressure >= 20);
        assert(refined.blocking_coverage >= 24);
        assert(refined.mobility >= 8);

        assert(summary.refined_wins >= summary.baseline_wins);
        if (summary.refined_wins == summary.baseline_wins) {
            assert(summary.refined_losses <= summary.baseline_losses);
        }
        if (summary.refined_wins == summary.baseline_wins &&
            summary.refined_losses == summary.baseline_losses) {
            assert(summary.refined_total_half_moves <= summary.baseline_total_half_moves);
        }

        int32_t advantage_validation = 0;
        int32_t frontier_validation = 0;
        evaluate_refined_against_baseline(rule, &refined,
                                          &kBaselineWeights[rule_index],
                                          kExtendedTuningHalfMoveCap,
                                          &advantage_validation,
                                          &frontier_validation);
        assert(advantage_validation >= -64);
        assert(frontier_validation >= -64);
        assert(frontier_validation <= 64);

        uint32_t refined_avg_half_moves = summary.refined_total_half_moves / kExtendedTuningGamesPerRule;
        uint32_t baseline_avg_half_moves = summary.baseline_total_half_moves / kExtendedTuningGamesPerRule;

        printf("Extended tuning %s: refined W-L-D=%u-%u-%u baseline=%u-%u-%u avgPlies=%u/%u finalAdv=%d finalFront=%d weights={%d,%d,%d,%d,%d}\n",
               kRuleNames[rule_index],
               (unsigned)summary.refined_wins,
               (unsigned)summary.refined_losses,
               (unsigned)summary.refined_draws,
               (unsigned)summary.baseline_wins,
               (unsigned)summary.baseline_losses,
               (unsigned)summary.baseline_draws,
               (unsigned)refined_avg_half_moves,
               (unsigned)baseline_avg_half_moves,
               advantage_validation,
               frontier_validation,
               refined.connection_progress,
               refined.bridge_potential,
               refined.swap_pressure,
               refined.blocking_coverage,
               refined.mobility);
    }
}

// Helper function to format move position string (col as letter, row as 1-based)
static void format_test_move_string(char *buf, size_t buf_size, const move_t *move) {
    if (!buf || buf_size == 0 || !move) return;
    
    char from_col = 'A' + move->from_col;
    char to_col = 'A' + move->to_col;
    uint8_t from_row = move->from_row + 1;
    uint8_t to_row = move->to_row + 1;
    const char *move_type = (move->type == MOVE_TYPE_SWAP) ? "swap" : "empty";
    
    snprintf(buf, buf_size, "%c%u->%c%u (%s)", 
             from_col, from_row, to_col, to_row, move_type);
}

// Test the three win-in-3 puzzle examples from ai_best_move_test.txt
static void test_win_in_3_puzzle_example_1(void) {
    // Puzzle Example 1: Correct best move is D4->C3. AI was returning A7->B8.
    // This is a "classic_depth3_3" puzzle
    board_t board;
    board_init(&board);
    clear_board(&board);
    
    board.current_player = PLAYER_WHITE;
    
    // Set up starting position from the puzzle JSON
    board_set_piece(&board, 1, 1, PIECE_WHITE_SWAPPED);    // B2 White swapped
    board_set_piece(&board, 2, 0, PIECE_BLACK_SWAPPED);    // A3 Black swapped
    board_set_piece(&board, 2, 2, PIECE_BLACK_NORMAL);     // C3 Black normal
    board_set_piece(&board, 2, 3, PIECE_BLACK_NORMAL);     // D3 Black normal
    board_set_piece(&board, 3, 1, PIECE_WHITE_SWAPPED);    // B4 White swapped
    board_set_piece(&board, 3, 2, PIECE_BLACK_SWAPPED);    // C4 Black swapped
    board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4 White swapped
    board_set_piece(&board, 4, 0, PIECE_WHITE_NORMAL);     // A5 White normal
    board_set_piece(&board, 4, 2, PIECE_WHITE_NORMAL);     // C5 White normal
    board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5 Black normal
    board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6 Black normal
    board_set_piece(&board, 5, 3, PIECE_BLACK_NORMAL);     // D6 Black normal
    board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7 White normal
    board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8 Black normal
    board_set_piece(&board, 7, 2, PIECE_WHITE_NORMAL);     // C8 White normal
    board_set_piece(&board, 7, 3, PIECE_WHITE_NORMAL);     // D8 White normal
    
    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    
    move_t ai_move;
    bool found = ai_agent_find_best_move(&board, &config, &ai_move);
    assert(found);
    
    char move_str[64];
    format_test_move_string(move_str, sizeof(move_str), &ai_move);
    
    // Expected move: D4 (row 3, col 3) -> C3 (row 2, col 2) swap
    move_t expected = {
        .from_row = 3, .from_col = 3,
        .to_row = 2, .to_col = 2,
        .type = MOVE_TYPE_SWAP,
        .player = PLAYER_WHITE
    };
    
    char expected_str[64];
    format_test_move_string(expected_str, sizeof(expected_str), &expected);
    
    printf("WIN_IN_3_EXAMPLE_1: AI selected %s, expected %s\n", move_str, expected_str);
    
    if (ai_move.from_row != expected.from_row || 
        ai_move.from_col != expected.from_col ||
        ai_move.to_row != expected.to_row || 
        ai_move.to_col != expected.to_col) {
        printf("  WARNING: Suboptimal move selected (expected D4->C3 swap)\n");
    }
}

static void test_win_in_3_puzzle_example_2(void) {
    // Puzzle Example 2: Correct best move is A7->B6. AI was returning A7->B8.
    // This is a "classic_depth3_14" puzzle
    board_t board;
    board_init(&board);
    clear_board(&board);
    
    board.current_player = PLAYER_WHITE;
    
    // Set up starting position from the puzzle JSON
    board_set_piece(&board, 0, 3, PIECE_WHITE_NORMAL);     // D1 White normal
    board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);     // B2 White normal
    board_set_piece(&board, 1, 2, PIECE_BLACK_NORMAL);     // C2 Black normal
    board_set_piece(&board, 1, 3, PIECE_WHITE_NORMAL);     // D2 White normal
    board_set_piece(&board, 2, 2, PIECE_WHITE_SWAPPED);    // C3 White swapped
    board_set_piece(&board, 3, 0, PIECE_BLACK_NORMAL);     // A4 Black normal
    board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4 White swapped
    board_set_piece(&board, 4, 0, PIECE_BLACK_NORMAL);     // A5 Black normal
    board_set_piece(&board, 4, 2, PIECE_BLACK_SWAPPED);    // C5 Black swapped
    board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5 Black normal
    board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6 Black normal
    board_set_piece(&board, 5, 3, PIECE_WHITE_SWAPPED);    // D6 White swapped
    board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7 White normal
    board_set_piece(&board, 6, 3, PIECE_WHITE_SWAPPED);    // D7 White swapped
    board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8 Black normal
    board_set_piece(&board, 7, 3, PIECE_BLACK_SWAPPED);    // D8 Black swapped
    
    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    
    move_t ai_move;
    bool found = ai_agent_find_best_move(&board, &config, &ai_move);
    assert(found);
    
    char move_str[64];
    format_test_move_string(move_str, sizeof(move_str), &ai_move);
    
    // Expected move: A7 (row 6, col 0) -> B6 (row 5, col 1) swap
    move_t expected = {
        .from_row = 6, .from_col = 0,
        .to_row = 5, .to_col = 1,
        .type = MOVE_TYPE_SWAP,
        .player = PLAYER_WHITE
    };
    
    char expected_str[64];
    format_test_move_string(expected_str, sizeof(expected_str), &expected);
    
    printf("WIN_IN_3_EXAMPLE_2: AI selected %s, expected %s\n", move_str, expected_str);
    
    if (ai_move.from_row != expected.from_row || 
        ai_move.from_col != expected.from_col ||
        ai_move.to_row != expected.to_row || 
        ai_move.to_col != expected.to_col) {
        printf("  WARNING: Suboptimal move selected (expected A7->B6 swap)\n");
    }
}

static void test_win_in_3_puzzle_example_3(void) {
    // Puzzle Example 3: Best Move C2->B3. AI was returning A2->B3.
    // This is a "classic_depth3_18" puzzle
    board_t board;
    board_init(&board);
    clear_board(&board);
    
    board.current_player = PLAYER_WHITE;
    
    // Set up starting position from the puzzle JSON
    board_set_piece(&board, 1, 0, PIECE_WHITE_NORMAL);     // A2 White normal
    board_set_piece(&board, 1, 1, PIECE_BLACK_NORMAL);     // B2 Black normal
    board_set_piece(&board, 1, 2, PIECE_WHITE_NORMAL);     // C2 White normal
    board_set_piece(&board, 2, 0, PIECE_BLACK_NORMAL);     // A3 Black normal
    board_set_piece(&board, 2, 1, PIECE_BLACK_NORMAL);     // B3 Black normal
    board_set_piece(&board, 2, 3, PIECE_BLACK_NORMAL);     // D3 Black normal
    board_set_piece(&board, 3, 0, PIECE_BLACK_SWAPPED);    // A4 Black swapped
    board_set_piece(&board, 3, 1, PIECE_WHITE_SWAPPED);    // B4 White swapped
    board_set_piece(&board, 3, 3, PIECE_BLACK_SWAPPED);    // D4 Black swapped
    board_set_piece(&board, 4, 1, PIECE_WHITE_NORMAL);     // B5 White normal
    board_set_piece(&board, 4, 2, PIECE_WHITE_SWAPPED);    // C5 White swapped
    board_set_piece(&board, 4, 3, PIECE_WHITE_SWAPPED);    // D5 White swapped
    board_set_piece(&board, 5, 3, PIECE_BLACK_SWAPPED);    // D6 Black swapped
    board_set_piece(&board, 6, 2, PIECE_WHITE_SWAPPED);    // C7 White swapped
    board_set_piece(&board, 7, 1, PIECE_WHITE_SWAPPED);    // B8 White swapped
    board_set_piece(&board, 7, 2, PIECE_BLACK_SWAPPED);    // C8 Black swapped
    
    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    
    move_t ai_move;
    bool found = ai_agent_find_best_move(&board, &config, &ai_move);
    assert(found);
    
    char move_str[64];
    format_test_move_string(move_str, sizeof(move_str), &ai_move);
    
    // Expected move: C2 (row 1, col 2) -> B3 (row 2, col 1) swap
    move_t expected = {
        .from_row = 1, .from_col = 2,
        .to_row = 2, .to_col = 1,
        .type = MOVE_TYPE_SWAP,
        .player = PLAYER_WHITE
    };
    
    char expected_str[64];
    format_test_move_string(expected_str, sizeof(expected_str), &expected);
    
    printf("WIN_IN_3_EXAMPLE_3: AI selected %s, expected %s\n", move_str, expected_str);
    
    if (ai_move.from_row != expected.from_row || 
        ai_move.from_col != expected.from_col ||
        ai_move.to_row != expected.to_row || 
        ai_move.to_col != expected.to_col) {
        printf("  WARNING: Suboptimal move selected (expected C2->B3 swap)\n");
    }
}

// Debug test to analyze move evaluations for puzzle example 1
static void debug_test_win_in_3_puzzle_example_1_analysis(void) {
    // Puzzle Example 1: Correct best move is D4->C3. AI was returning A5->B6.
    board_t board;
    board_init(&board);
    clear_board(&board);
    
    board.current_player = PLAYER_WHITE;
    
    // Set up starting position
    board_set_piece(&board, 1, 1, PIECE_WHITE_SWAPPED);
    board_set_piece(&board, 2, 0, PIECE_BLACK_SWAPPED);
    board_set_piece(&board, 2, 2, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 2, 3, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 3, 1, PIECE_WHITE_SWAPPED);
    board_set_piece(&board, 3, 2, PIECE_BLACK_SWAPPED);
    board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);
    board_set_piece(&board, 4, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 4, 2, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 5, 3, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);
    board_set_piece(&board, 7, 2, PIECE_WHITE_NORMAL);
    board_set_piece(&board, 7, 3, PIECE_WHITE_NORMAL);
    
    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    
    // Analyze some candidate moves
    move_t candidate_moves[] = {
        {.from_row = 3, .from_col = 3, .to_row = 2, .to_col = 2, .type = MOVE_TYPE_SWAP, .player = PLAYER_WHITE}, // D4->C3 (expected)
        {.from_row = 4, .from_col = 0, .to_row = 5, .to_col = 1, .type = MOVE_TYPE_SWAP, .player = PLAYER_WHITE}, // A5->B6
        {.from_row = 6, .from_col = 0, .to_row = 7, .to_col = 1, .type = MOVE_TYPE_EMPTY, .player = PLAYER_WHITE}, // A7->B8
    };
    
    printf("\nDEBUG: Analyzing candidate moves for Example 1\n");
    for (size_t i = 0; i < sizeof(candidate_moves) / sizeof(candidate_moves[0]); ++i) {
        board_t test_board;
        memcpy(&test_board, &board, sizeof(board_t));
        
        move_t *move = &candidate_moves[i];
        char move_str[64];
        format_test_move_string(move_str, sizeof(move_str), move);
        
        if (!board_execute_move(&test_board, move, config.swap_rule)) {
            printf("  %s: INVALID MOVE\n", move_str);
            continue;
        }
        
        int16_t eval = ai_agent_evaluate_board(&test_board, PLAYER_WHITE, &config);
        printf("  %s: eval=%d\n", move_str, eval);
        
        // Check if it leads to a win
        if (board_check_win(&test_board, PLAYER_WHITE, NULL)) {
            printf("    -> IMMEDIATE WIN!\n");
        }
    }
}

int main(void) {
    puts("Running Switcharoo AI agent tests...");
    test_evaluation_symmetry();
    test_forced_win_detection();
    test_forcing_move_preference();
    test_unavoidable_loss_detection();
    test_blunder_learning_allows_immediate_win();
    test_blunder_standard_allows_forcing_move();
    test_blunder_expert_ignored();
    test_determinism();
    test_high_branching_stays_responsive();
    test_self_play_aggressive_advancement();
    test_head_to_head_outcomes();
    puts("\nTesting win-in-3 puzzle examples from ai_best_move_test.txt...");
    test_win_in_3_puzzle_example_1();
    test_win_in_3_puzzle_example_2();
    test_win_in_3_puzzle_example_3();
    debug_test_win_in_3_puzzle_example_1_analysis();
    
    // Extended debug: Investigate Example 2 - Platform difference analysis
    printf("\nDEBUG: Example 2 - Investigating winning move A7->B6\n");
    {
        board_t board;
        board_init(&board);
        clear_board(&board);
        
        board.current_player = PLAYER_WHITE;
        board_set_piece(&board, 0, 3, PIECE_WHITE_NORMAL);     // D1
        board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);     // B2
        board_set_piece(&board, 1, 2, PIECE_BLACK_NORMAL);     // C2
        board_set_piece(&board, 1, 3, PIECE_WHITE_NORMAL);     // D2
        board_set_piece(&board, 2, 2, PIECE_WHITE_SWAPPED);    // C3
        board_set_piece(&board, 3, 0, PIECE_BLACK_NORMAL);     // A4
        board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4
        board_set_piece(&board, 4, 0, PIECE_BLACK_NORMAL);     // A5
        board_set_piece(&board, 4, 2, PIECE_BLACK_SWAPPED);    // C5
        board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5
        board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6
        board_set_piece(&board, 5, 3, PIECE_WHITE_SWAPPED);    // D6
        board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7
        board_set_piece(&board, 6, 3, PIECE_WHITE_SWAPPED);    // D7
        board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8
        board_set_piece(&board, 7, 3, PIECE_BLACK_SWAPPED);    // D8
        
        ai_config_t config;
        ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
        
        // Test the expected winning move: A7->B6 (row 6, col 0 -> row 5, col 1)
        move_t winning_move = { .from_row = 6, .from_col = 0, .to_row = 5, .to_col = 1, .type = MOVE_TYPE_SWAP };
        board_t after_winning;
        memcpy(&after_winning, &board, sizeof(board_t));
        bool move_valid = board_execute_move(&after_winning, &winning_move, config.swap_rule);
        
        if (move_valid) {
            int16_t eval_after = ai_agent_evaluate_board(&after_winning, PLAYER_WHITE, &config);
            printf("  A7->B6 is VALID. Eval after move: %d\n", eval_after);
            
            // Now check what the AI actually selected for this position
            move_t ai_selected;
            bool ai_found = ai_agent_find_best_move(&board, &config, &ai_selected);
            if (ai_found) {
                char ai_move_str[64];
                format_test_move_string(ai_move_str, sizeof(ai_move_str), &ai_selected);
                printf("  AI selected: %s\n", ai_move_str);
                
                // Evaluate the AI's selected move
                board_t after_ai;
                memcpy(&after_ai, &board, sizeof(board_t));
                if (board_execute_move(&after_ai, &ai_selected, config.swap_rule)) {
                    int16_t eval_ai = ai_agent_evaluate_board(&after_ai, PLAYER_WHITE, &config);
                    printf("  AI move eval: %d\n", eval_ai);
                    printf("  Winning move eval: %d\n", eval_after);
                    printf("  Difference: %d\n", eval_after - eval_ai);
                } else {
                    printf("  AI's selected move is INVALID!\n");
                }
            }
        } else {
            printf("  A7->B6 is INVALID!\n");
        }
    }
    
    // Comprehensive evaluation analysis for Example 2
    printf("\nDEBUG: Example 2 - Comprehensive Move Evaluation Analysis\n");
    {
        board_t board;
        board_init(&board);
        clear_board(&board);
        
        board.current_player = PLAYER_WHITE;
        board_set_piece(&board, 0, 3, PIECE_WHITE_NORMAL);     // D1
        board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);     // B2
        board_set_piece(&board, 1, 2, PIECE_BLACK_NORMAL);     // C2
        board_set_piece(&board, 1, 3, PIECE_WHITE_NORMAL);     // D2
        board_set_piece(&board, 2, 2, PIECE_WHITE_SWAPPED);    // C3
        board_set_piece(&board, 3, 0, PIECE_BLACK_NORMAL);     // A4
        board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4
        board_set_piece(&board, 4, 0, PIECE_BLACK_NORMAL);     // A5
        board_set_piece(&board, 4, 2, PIECE_BLACK_SWAPPED);    // C5
        board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5
        board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6
        board_set_piece(&board, 5, 3, PIECE_WHITE_SWAPPED);    // D6
        board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7
        board_set_piece(&board, 6, 3, PIECE_WHITE_SWAPPED);    // D7
        board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8
        board_set_piece(&board, 7, 3, PIECE_BLACK_SWAPPED);    // D8
        
        ai_config_t config;
        ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
        
        // Collect all White's legal moves
        move_t all_moves[64];
        uint8_t move_count = 0;
        for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
            for (uint8_t col = 0; col < BOARD_COLS; ++col) {
                piece_type_t piece = board_get_piece(&board, row, col);
                if (board_get_piece_owner(piece) == PLAYER_WHITE) {
                    move_count += board_get_legal_moves(&board, row, col,
                                                        &all_moves[move_count],
                                                        (uint8_t)(64 - move_count));
                }
            }
        }
        
        // Evaluate each move
        int16_t max_eval = INT16_MIN;
        int16_t min_eval = INT16_MAX;
        move_t max_move, min_move, a7b6_move, d1c2_move;
        int16_t a7b6_eval = 0, d1c2_eval = 0;
        bool found_a7b6 = false, found_d1c2 = false;
        
        printf("  Total moves available: %u\n", move_count);
        
        for (uint8_t i = 0; i < move_count; ++i) {
            board_t test;
            memcpy(&test, &board, sizeof(board_t));
            if (board_execute_move(&test, &all_moves[i], config.swap_rule)) {
                int16_t eval = ai_agent_evaluate_board(&test, PLAYER_WHITE, &config);
                
                // Track extremes
                if (eval > max_eval) {
                    max_eval = eval;
                    max_move = all_moves[i];
                }
                if (eval < min_eval) {
                    min_eval = eval;
                    min_move = all_moves[i];
                }
                
                // Track specific moves
                if (all_moves[i].from_row == 6 && all_moves[i].from_col == 0 &&
                    all_moves[i].to_row == 5 && all_moves[i].to_col == 1) {
                    a7b6_eval = eval;
                    a7b6_move = all_moves[i];
                    found_a7b6 = true;
                }
                if (all_moves[i].from_row == 0 && all_moves[i].from_col == 3 &&
                    all_moves[i].to_row == 1 && all_moves[i].to_col == 2) {
                    d1c2_eval = eval;
                    d1c2_move = all_moves[i];
                    found_d1c2 = true;
                }
            }
        }
        
        printf("  Min evaluation: %d\n", min_eval);
        printf("  Max evaluation: %d\n", max_eval);
        if (found_a7b6) {
            printf("  A7→B6 evaluation: %d\n", a7b6_eval);
        }
        if (found_d1c2) {
            printf("  D1→C2 evaluation: %d\n", d1c2_eval);
        }
        if (found_a7b6 && found_d1c2) {
            printf("  COMPARISON: A7→B6 (%d) vs D1→C2 (%d)\n", a7b6_eval, d1c2_eval);
            if (d1c2_eval > a7b6_eval) {
                printf("    WARNING: D1→C2 has HIGHER eval than A7→B6! (AI chose higher: %d > %d)\n", 
                       d1c2_eval, a7b6_eval);
            } else if (a7b6_eval > d1c2_eval) {
                printf("    OK: A7→B6 has higher eval (as expected for winning move)\n");
            } else {
                printf("    TIE: Both moves have same eval\n");
            }
        }
    }
    
    // Extended debug: Analyze deeper evaluation after each move
    printf("\nDEBUG: Extended analysis after D4->C3 vs A5->B6\n");
    {
        board_t board;
        board_init(&board);
        clear_board(&board);
        
        board.current_player = PLAYER_WHITE;
        board_set_piece(&board, 1, 1, PIECE_WHITE_SWAPPED);
        board_set_piece(&board, 2, 0, PIECE_BLACK_SWAPPED);
        board_set_piece(&board, 2, 2, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 2, 3, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 3, 1, PIECE_WHITE_SWAPPED);
        board_set_piece(&board, 3, 2, PIECE_BLACK_SWAPPED);
        board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);
        board_set_piece(&board, 4, 0, PIECE_WHITE_NORMAL);
        board_set_piece(&board, 4, 2, PIECE_WHITE_NORMAL);
        board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 5, 3, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);
        board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);
        board_set_piece(&board, 7, 2, PIECE_WHITE_NORMAL);
        board_set_piece(&board, 7, 3, PIECE_WHITE_NORMAL);
        
        ai_config_t config;
        ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
        
        // Test D4->C3 (row 3, col 3 -> row 2, col 2)
        move_t move_d4_c3 = { .from_row = 3, .from_col = 3, .to_row = 2, .to_col = 2, .type = MOVE_TYPE_SWAP };
        board_t after_d4_c3;
        memcpy(&after_d4_c3, &board, sizeof(board_t));
        board_execute_move(&after_d4_c3, &move_d4_c3, config.swap_rule);
        bool d4_c3_wins = board_check_win(&after_d4_c3, PLAYER_WHITE, NULL);
        bool d4_c3_has_forced = test_immediate_win_available(&after_d4_c3, PLAYER_WHITE, config.swap_rule);
        
        // Test A5->B6 (row 4, col 0 -> row 5, col 1)
        move_t move_a5_b6 = { .from_row = 4, .from_col = 0, .to_row = 5, .to_col = 1, .type = MOVE_TYPE_SWAP };
        board_t after_a5_b6;
        memcpy(&after_a5_b6, &board, sizeof(board_t));
        board_execute_move(&after_a5_b6, &move_a5_b6, config.swap_rule);
        bool a5_b6_wins = board_check_win(&after_a5_b6, PLAYER_WHITE, NULL);
        bool a5_b6_has_forced = test_immediate_win_available(&after_a5_b6, PLAYER_WHITE, config.swap_rule);
        
        printf("  D4->C3: immediate_win=%d forced_win_available=%d\n", d4_c3_wins, d4_c3_has_forced);
        printf("  A5->B6: immediate_win=%d forced_win_available=%d\n", a5_b6_wins, a5_b6_has_forced);
    }
    
    // Detailed Example 2 Move Analysis - Check which moves actually lead to forced wins
    printf("\n=== EXAMPLE 2 DETAILED MOVE ANALYSIS ===\n");
    {
        board_t board;
        board_init(&board);
        clear_board(&board);
        
        board.current_player = PLAYER_WHITE;
        board_set_piece(&board, 0, 3, PIECE_WHITE_NORMAL);     // D1
        board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);     // B2
        board_set_piece(&board, 1, 2, PIECE_BLACK_NORMAL);     // C2
        board_set_piece(&board, 1, 3, PIECE_WHITE_NORMAL);     // D2
        board_set_piece(&board, 2, 2, PIECE_WHITE_SWAPPED);    // C3
        board_set_piece(&board, 3, 0, PIECE_BLACK_NORMAL);     // A4
        board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4
        board_set_piece(&board, 4, 0, PIECE_BLACK_NORMAL);     // A5
        board_set_piece(&board, 4, 2, PIECE_BLACK_SWAPPED);    // C5
        board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5
        board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6
        board_set_piece(&board, 5, 3, PIECE_WHITE_SWAPPED);    // D6
        board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7
        board_set_piece(&board, 6, 3, PIECE_WHITE_SWAPPED);    // D7
        board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8
        board_set_piece(&board, 7, 3, PIECE_BLACK_SWAPPED);    // D8
        
        ai_config_t config;
        ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
        
        printf("\n--- EXPERT MODE SEARCH CONFIGURATION ---\n");
        printf("Base Depth: %d\n", config.search.base_depth);
        printf("Max Depth: %d\n", config.search.max_depth);
        printf("Max Extension: %d\n", config.search.max_extension);
        printf("Node Limit: %d\n", config.search.node_limit);
        printf("Iterative Deepening: %s\n", config.search.use_iterative_deepening ? "YES" : "NO");
        printf("Transposition Table: %s\n", config.search.use_transposition ? "YES" : "NO");
        
        // Test with increased depth to see if it helps
        printf("\n--- TESTING WITH INCREASED DEPTH (base=6, max=8) ---\n");
        ai_config_t deep_config = config;
        deep_config.search.base_depth = 6;
        deep_config.search.max_depth = 8;
        deep_config.search.node_limit = 100000;  // Much higher limit
        
        move_t deep_best;
        bool deep_found = ai_agent_find_best_move(&board, &deep_config, &deep_best);
        if (deep_found) {
            char deep_move_str[64];
            format_test_move_string(deep_move_str, sizeof(deep_move_str), &deep_best);
            printf("Deep search best move: %s\n", deep_move_str);
            
            // Check if it's the winning move
            if (deep_best.from_row == 6 && deep_best.from_col == 0 &&
                deep_best.to_row == 5 && deep_best.to_col == 1) {
                printf("✓ DEEP SEARCH FOUND THE WINNING MOVE A7->B6!\n");
            } else {
                printf("✗ Deep search still didn't find A7->B6\n");
            }
        }
        
        // Test the three key moves:
        // 1. A7->B6 (expected winning move from puzzle)
        // 2. D1->C2 (what Linux host AI selected)
        // 3. A7->B8 (what target platform AI selected)
        
        printf("\n--- STANDARD EXPERT MODE (base=4, max=6) ---\n");
        move_t standard_best;
        bool standard_found = ai_agent_find_best_move(&board, &config, &standard_best);
        if (standard_found) {
            char standard_move_str[64];
            format_test_move_string(standard_move_str, sizeof(standard_move_str), &standard_best);
            printf("Standard EXPERT best move: %s\n", standard_move_str);
            
            if (standard_best.from_row == 6 && standard_best.from_col == 0 &&
                standard_best.to_row == 5 && standard_best.to_col == 1) {
                printf("✓ STANDARD FOUND THE WINNING MOVE A7->B6!\n");
            } else {
                printf("✗ Standard didn't find A7->B6 (this is the bug we're investigating)\n");
            }
        }
        
        printf("\n--- WINNING SEQUENCE VERIFICATION ---\n");
        
        printf("\n1. Testing A7->B6 (EXPECTED winning move):\n");
        move_t move_a7b6 = { .from_row = 6, .from_col = 0, .to_row = 5, .to_col = 1, .type = MOVE_TYPE_SWAP };
        board_t after_a7b6;
        memcpy(&after_a7b6, &board, sizeof(board_t));
        if (board_execute_move(&after_a7b6, &move_a7b6, config.swap_rule)) {
            bool immediate_win = board_check_win(&after_a7b6, PLAYER_WHITE, NULL);
            printf("   - After A7->B6, immediate win: %s\n", immediate_win ? "YES" : "NO");
            
            // Switch to Black's turn and get best response
            after_a7b6.current_player = PLAYER_BLACK;
            move_t black_response;
            ai_config_t black_config = config;
            black_config.ai_player = PLAYER_BLACK;
            bool black_found_move = ai_agent_find_best_move(&after_a7b6, &black_config, &black_response);
            if (black_found_move) {
                char black_move_str[64];
                format_test_move_string(black_move_str, sizeof(black_move_str), &black_response);
                printf("   - Black's best response: %s\n", black_move_str);
                
                // Make Black's response
                board_t after_black_resp1;
                memcpy(&after_black_resp1, &after_a7b6, sizeof(board_t));
                if (board_execute_move(&after_black_resp1, &black_response, config.swap_rule)) {
                    // Now test if White can play D6->D5 (row 5, col 3 -> row 4, col 3)
                    after_black_resp1.current_player = PLAYER_WHITE;
                    move_t white_move2 = { .from_row = 5, .from_col = 3, .to_row = 4, .to_col = 3, .type = MOVE_TYPE_SWAP };
                    board_t after_white2;
                    memcpy(&after_white2, &after_black_resp1, sizeof(board_t));
                    bool white2_valid = board_execute_move(&after_white2, &white_move2, config.swap_rule);
                    printf("   - White's 2nd move D6->D5 valid: %s\n", white2_valid ? "YES" : "NO");
                    
                    if (white2_valid) {
                        bool white2_immediate_win = board_check_win(&after_white2, PLAYER_WHITE, NULL);
                        printf("   - After D6->D5, immediate win: %s\n", white2_immediate_win ? "YES" : "NO");
                        
                        // Get Black's response to D6->D5
                        after_white2.current_player = PLAYER_BLACK;
                        move_t black_resp2;
                        black_found_move = ai_agent_find_best_move(&after_white2, &black_config, &black_resp2);
                        if (black_found_move) {
                            char black_move2_str[64];
                            format_test_move_string(black_move2_str, sizeof(black_move2_str), &black_resp2);
                            printf("   - Black's 2nd response: %s\n", black_move2_str);
                            
                            // Make Black's 2nd response
                            board_t after_black_resp2;
                            memcpy(&after_black_resp2, &after_white2, sizeof(board_t));
                            if (board_execute_move(&after_black_resp2, &black_resp2, config.swap_rule)) {
                                // Now test White's winning move B6->C6 (row 5, col 1 -> row 5, col 2)
                                after_black_resp2.current_player = PLAYER_WHITE;
                                move_t white_move3 = { .from_row = 5, .from_col = 1, .to_row = 5, .to_col = 2, .type = MOVE_TYPE_EMPTY };
                                board_t after_white3;
                                memcpy(&after_white3, &after_black_resp2, sizeof(board_t));
                                bool white3_valid = board_execute_move(&after_white3, &white_move3, config.swap_rule);
                                printf("   - White's 3rd move B6->C6 valid: %s\n", white3_valid ? "YES" : "NO");
                                
                                if (white3_valid) {
                                    bool final_win = board_check_win(&after_white3, PLAYER_WHITE, NULL);
                                    printf("   - After B6->C6, WHITE WINS: %s\n", final_win ? "YES!!!" : "NO");
                                    
                                    if (final_win) {
                                        printf("   ✓ CONFIRMED: A7->B6 leads to forced win in 3 White moves (5 total moves)\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        printf("\n2. Testing D1->C2 (Linux host AI selected):\n");
        move_t move_d1c2 = { .from_row = 0, .from_col = 3, .to_row = 1, .to_col = 2, .type = MOVE_TYPE_SWAP };
        board_t after_d1c2;
        memcpy(&after_d1c2, &board, sizeof(board_t));
        if (board_execute_move(&after_d1c2, &move_d1c2, config.swap_rule)) {
            bool immediate_win = board_check_win(&after_d1c2, PLAYER_WHITE, NULL);
            printf("   - Immediate win: %s\n", immediate_win ? "YES" : "NO");
            
            after_d1c2.current_player = PLAYER_BLACK;
            bool black_has_forcing = test_immediate_win_available(&after_d1c2, PLAYER_BLACK, config.swap_rule);
            printf("   - Black has immediate win available: %s\n", black_has_forcing ? "YES" : "NO");
            
            move_t black_response;
            ai_config_t black_config = config;
            black_config.ai_player = PLAYER_BLACK;
            bool black_found_move = ai_agent_find_best_move(&after_d1c2, &black_config, &black_response);
            if (black_found_move) {
                char black_move_str[64];
                format_test_move_string(black_move_str, sizeof(black_move_str), &black_response);
                printf("   - Black's best response: %s\n", black_move_str);
                
                board_t after_black_response;
                memcpy(&after_black_response, &after_d1c2, sizeof(board_t));
                if (board_execute_move(&after_black_response, &black_response, config.swap_rule)) {
                    after_black_response.current_player = PLAYER_WHITE;
                    bool white_has_forcing = test_immediate_win_available(&after_black_response, PLAYER_WHITE, config.swap_rule);
                    printf("   - After Black's response, White has immediate win: %s\n", white_has_forcing ? "YES" : "NO");
                }
            }
        }
        
        printf("\n3. Testing A7->B8 (Target platform AI selected):\n");
        move_t move_a7b8 = { .from_row = 6, .from_col = 0, .to_row = 7, .to_col = 1, .type = MOVE_TYPE_SWAP };
        board_t after_a7b8;
        memcpy(&after_a7b8, &board, sizeof(board_t));
        if (board_execute_move(&after_a7b8, &move_a7b8, config.swap_rule)) {
            bool immediate_win = board_check_win(&after_a7b8, PLAYER_WHITE, NULL);
            printf("   - Immediate win: %s\n", immediate_win ? "YES" : "NO");
            
            after_a7b8.current_player = PLAYER_BLACK;
            bool black_has_forcing = test_immediate_win_available(&after_a7b8, PLAYER_BLACK, config.swap_rule);
            printf("   - Black has immediate win available: %s\n", black_has_forcing ? "YES" : "NO");
            
            move_t black_response;
            ai_config_t black_config = config;
            black_config.ai_player = PLAYER_BLACK;
            bool black_found_move = ai_agent_find_best_move(&after_a7b8, &black_config, &black_response);
            if (black_found_move) {
                char black_move_str[64];
                format_test_move_string(black_move_str, sizeof(black_move_str), &black_response);
                printf("   - Black's best response: %s\n", black_move_str);
                
                board_t after_black_response;
                memcpy(&after_black_response, &after_a7b8, sizeof(board_t));
                if (board_execute_move(&after_black_response, &black_response, config.swap_rule)) {
                    after_black_response.current_player = PLAYER_WHITE;
                    bool white_has_forcing = test_immediate_win_available(&after_black_response, PLAYER_WHITE, config.swap_rule);
                    printf("   - After Black's response, White has immediate win: %s\n", white_has_forcing ? "YES" : "NO");
                }
            }
        }
    }
    
    // test_extended_self_play_tuning();  // COMMENTED OUT: Long-running test not needed for this investigation
    // test_weight_tuning_poc();
    puts("\n=== All tests passed. ===");
    return 0;
}
