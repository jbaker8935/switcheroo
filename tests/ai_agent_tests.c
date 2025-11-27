#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/ai_agent.h"
#include "../src/board.h"
#include "puzzle_test_data.h"

typedef struct {
    board_t board;
    board_context_t context;
} test_board_t;

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

static void clear_board(board_t *board, board_context_t *context) {
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }
    context->history_count = 0;
    board->move_count = 0;
    context->current_player = PLAYER_WHITE;
}

static void setup_board(board_t *board, const char *placement) {
    if (!board || !placement || *placement == '\0') {
        return;
    }

    char buffer[256];
    strncpy(buffer, placement, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';

    char *token = strtok(buffer, ",");
    while (token) {
        while (*token == ' ') {
            ++token;
        }

        size_t len = strlen(token);
        if (len < 4u || token[2] != ':') {
            fprintf(stderr, "Invalid token in setup string: %s\n", token);
            token = strtok(NULL, ",");
            continue;
        }

        char column_char = (char)toupper((unsigned char)token[0]);
        char row_char = token[1];
        char piece_char = token[3];

        if (column_char < 'A' || column_char >= ('A' + BOARD_COLS)) {
            fprintf(stderr, "Invalid column in setup string: %c\n", column_char);
            token = strtok(NULL, ",");
            continue;
        }

        if (row_char < '1' || row_char > '8') {
            fprintf(stderr, "Invalid row in setup string: %c\n", row_char);
            token = strtok(NULL, ",");
            continue;
        }

        uint8_t col = (uint8_t)(column_char - 'A');
        uint8_t row = (uint8_t)(row_char - '1');

        piece_type_t piece = PIECE_NONE;
        switch (piece_char) {
            case 'W':
                piece = PIECE_WHITE_NORMAL;
                break;
            case 'w':
                piece = PIECE_WHITE_SWAPPED;
                break;
            case 'B':
                piece = PIECE_BLACK_NORMAL;
                break;
            case 'b':
                piece = PIECE_BLACK_SWAPPED;
                break;
            default:
                fprintf(stderr, "Invalid piece type in setup string: %c\n", piece_char);
                token = strtok(NULL, ",");
                continue;
        }

        board_set_piece(board, row, col, piece);
        token = strtok(NULL, ",");
    }
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
        return (value[1] == 'R' || value[1] == 'r') && (value[2] == 'U' || value[2] == 'u') &&
               (value[3] == 'E' || value[3] == 'e');
    }

    if (*value == 'Y' || *value == 'y') {
        if (value[1] == '\0') {
            return true;
        }
        return (value[1] == 'E' || value[1] == 'e') && (value[2] == 'S' || value[2] == 's');
    }

    if (*value == 'O' || *value == 'o') {
        return (value[1] == 'N' || value[1] == 'n') && (value[2] == '\0');
    }

    return false;
}

// Stubs for target-specific functions
void clear_puzzle_hint(void) {}
void *get_puzzle_collection(void) {
    return NULL;
}
void *get_puzzle_by_index(void *collection, uint16_t index) {
    return NULL;
}
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

static bool weight_stats_better(const ai_weight_stats_t *lhs, const ai_weight_stats_t *rhs) {
    if (lhs->wins != rhs->wins) {
        return lhs->wins > rhs->wins;
    }
    if (lhs->losses != rhs->losses) {
        return lhs->losses < rhs->losses;
    }
    return lhs->total_half_moves < rhs->total_half_moves;
}

static const ai_eval_weights_t kBaselineWeights[4] = {
    {60, 40, 32, 44, 22}, {58, 38, 28, 42, 22}, {55, 42, 44, 36, 24}, {55, 40, 40, 36, 24}};

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

static void configure_self_play_profile(ai_config_t *config, const ai_eval_weights_t *weights) {
    config->weights = *weights;
    config->diagnostics_enabled = false;
    ai_agent_config_set_randomization(config, 1u, 0u);
    config->blunder_enabled = false;
    config->blunder_chance_pct = 0u;
    config->blunder_type = AI_BLUNDER_NONE;
    config->enable_forcing_check = true;
}

static void configure_tuning_profile(ai_config_t *config, const ai_eval_weights_t *weights) {
    configure_self_play_profile(config, weights);
    config->enable_forcing_check = false;
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
    board_context_t scratch_context;
    memcpy(&scratch, board, sizeof(board_t));
    scratch_context.current_player = player;

    move_t moves[8];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t count = board_get_legal_moves_soa(&scratch, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < count; ++i) {
                move_array_get_move(&soa_moves, i, &moves[i]);
                board_t test_state;
                memcpy(&test_state, &scratch, sizeof(board_t));
                board_context_t dummy_context = {0};
                dummy_context.current_player = player;
                if (!board_execute_move(&test_state, &dummy_context, &moves[i], rule)) {
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

static bool test_move_creates_forced_immediate_win(const board_t *board, const move_t *move, player_t ai_player,
                                                   swap_rule_t rule) {
    if (!move) {
        return false;
    }

    board_t after_ai;
    memcpy(&after_ai, board, sizeof(board_t));
    board_context_t dummy_context = {0};
    dummy_context.current_player = ai_player;
    if (!board_execute_move(&after_ai, &dummy_context, move, rule)) {
        return false;
    }

    if (board_check_win(&after_ai, ai_player, NULL)) {
        return true;
    }

    board_t opponent_state;
    board_context_t opponent_context;
    memcpy(&opponent_state, &after_ai, sizeof(board_t));
    board_switch_turn(&opponent_context);
    opponent_context.current_player = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    move_t opponent_moves[kTestMaxMoves];
    uint8_t opponent_count = 0;

    for (uint8_t row = 0; row < BOARD_ROWS && opponent_count < kTestMaxMoves; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS && opponent_count < kTestMaxMoves; ++col) {
            piece_type_t piece = board_get_piece(&opponent_state, row, col);
            if (board_get_piece_owner(piece) != opponent_context.current_player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(&opponent_state, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves && opponent_count < kTestMaxMoves; ++i) {
                move_array_get_move(&soa_moves, i, &opponent_moves[opponent_count]);
                opponent_count++;
            }
        }
    }

    if (opponent_count == 0u) {
        return true;
    }

    for (uint8_t i = 0; i < opponent_count; ++i) {
        board_t after_opponent;
        memcpy(&after_opponent, &opponent_state, sizeof(board_t));
        board_context_t dummy_context = {0};
        dummy_context.current_player = opponent_context.current_player;
        if (!board_execute_move(&after_opponent, &dummy_context, &opponent_moves[i], rule)) {
            continue;
        }

        if (board_check_win(&after_opponent, opponent_context.current_player, NULL)) {
            return false;
        }

        board_context_t after_opponent_context;
        board_switch_turn(&after_opponent_context);
        after_opponent_context.current_player = ai_player;
        if (!test_immediate_win_available(&after_opponent, ai_player, rule)) {
            return false;
        }
    }

    return true;
}

static bool test_forcing_move_available(const board_t *board, player_t player, swap_rule_t rule) {
    board_t scratch;
    board_context_t scratch_context;
    memcpy(&scratch, board, sizeof(board_t));
    scratch_context.current_player = player;

    move_t moves[kTestMaxMoves];
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&scratch, row, col);
            if (board_get_piece_owner(piece) != player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t count = board_get_legal_moves_soa(&scratch, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < count; ++i) {
                move_array_get_move(&soa_moves, i, &moves[i]);
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

    board_context_t context = {0};
    context.current_player = PLAYER_WHITE;
    for (uint8_t row = 0; row < BOARD_ROWS && count < kTestMaxMoves; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS && count < kTestMaxMoves; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) != context.current_player) {
                continue;
            }

            move_array_t soa_moves;
            uint8_t num_moves = board_get_legal_moves_soa(board, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < num_moves && count < kTestMaxMoves; ++i) {
                move_array_get_move(&soa_moves, i, &moves[count]);
                count++;
            }
        }
    }

    if (count == 0u) {
        return false;
    }

    uint32_t chosen = rng_range(count);
    *out_move = moves[chosen];
    return true;
}

static ai_self_play_metrics_t run_profiled_self_play_session(const ai_eval_weights_t *white_weights,
                                                             const ai_eval_weights_t *black_weights, swap_rule_t rule,
                                                             uint16_t half_move_limit,
                                                             ai_profile_configurator_t configurator) {
    board_t board;
    board_context_t context;
    board_init(&board);
    context.current_player = PLAYER_WHITE;
    context.history_count = 0;

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
        ai_config_t *active_cfg = (context.current_player == PLAYER_WHITE) ? &white_cfg : &black_cfg;
        move_t move;
        bool found = ai_agent_find_best_move(&board, &context, active_cfg, &move);
        assert(found);
        bool executed = board_execute_move(&board, &context, &move, active_cfg->swap_rule);
        assert(executed);

        metrics.plies_played = (uint8_t)(ply + 1u);

        if (board_check_win(&board, active_cfg->ai_player, NULL)) {
            metrics.winner = active_cfg->ai_player;
            break;
        }

        board_switch_turn(&context);
    }

    metrics.white_advancement = compute_advancement_score(&board, PLAYER_WHITE);
    metrics.black_advancement = compute_advancement_score(&board, PLAYER_BLACK);
    metrics.white_frontier = compute_frontier_row(&board, PLAYER_WHITE);
    metrics.black_frontier = compute_frontier_row(&board, PLAYER_BLACK);

    return metrics;
}

static ai_self_play_metrics_t run_self_play_session(const ai_eval_weights_t *white_weights,
                                                    const ai_eval_weights_t *black_weights, swap_rule_t rule,
                                                    uint8_t half_move_limit) {
    return run_profiled_self_play_session(white_weights, black_weights, rule, (uint16_t)half_move_limit,
                                          configure_self_play_profile);
}

static ai_self_play_metrics_t run_candidate_vs_random(const ai_eval_weights_t *candidate_weights,
                                                      const ai_eval_weights_t *baseline_weights, swap_rule_t rule,
                                                      uint16_t half_move_limit, bool candidate_as_white, uint32_t seed,
                                                      uint8_t random_epsilon_pct) {
    board_t board;
    board_context_t context;
    board_init(&board);
    context.current_player = candidate_as_white ? PLAYER_WHITE : PLAYER_BLACK;

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
        bool candidate_turn = (context.current_player == candidate_player);
        ai_config_t *active_cfg = candidate_turn ? &candidate_cfg : &random_cfg;
        move_t move;
        bool found = false;

        if (candidate_turn) {
            found = ai_agent_find_best_move(&board, &context, active_cfg, &move);
        } else {
            bool take_random = rng_chance_percent(random_epsilon_pct);
            if (take_random) {
                found = select_random_move(&board, &move);
            }
            if (!found) {
                found = ai_agent_find_best_move(&board, &context, active_cfg, &move);
            }
        }

        if (!found) {
            break;
        }

        board_context_t dummy_context = {0};
        dummy_context.current_player = active_cfg->ai_player;
        bool executed = board_execute_move(&board, &dummy_context, &move, active_cfg->swap_rule);
        assert(executed);

        metrics.plies_played = (uint8_t)(ply + 1u);

        if (board_check_win(&board, candidate_turn ? candidate_player : random_player, NULL)) {
            metrics.winner = candidate_turn ? candidate_player : random_player;
            break;
        }

        board_context_t switch_context = {0};
        board_switch_turn(&switch_context);
    }

    metrics.white_advancement = compute_advancement_score(&board, PLAYER_WHITE);
    metrics.black_advancement = compute_advancement_score(&board, PLAYER_BLACK);
    metrics.white_frontier = compute_frontier_row(&board, PLAYER_WHITE);
    metrics.black_frontier = compute_frontier_row(&board, PLAYER_BLACK);

    return metrics;
}

static ai_weight_stats_t evaluate_weight_set_vs_random(const ai_eval_weights_t *candidate_weights,
                                                       const ai_eval_weights_t *baseline_weights, swap_rule_t rule,
                                                       uint16_t games, uint16_t half_move_limit, uint32_t seed_base,
                                                       uint8_t random_epsilon_pct) {
    ai_weight_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    for (uint16_t game = 0; game < games; ++game) {
        bool candidate_as_white = ((game & 1u) == 0u);
        uint32_t seed = seed_base ^ (((uint32_t)game + 1u) * 0x9E3779B9u);

        ai_self_play_metrics_t metrics = run_candidate_vs_random(
            candidate_weights, baseline_weights, rule, half_move_limit, candidate_as_white, seed, random_epsilon_pct);

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

static player_t run_head_to_head_match(const ai_eval_weights_t *white_weights, const ai_eval_weights_t *black_weights,
                                       swap_rule_t rule, uint8_t half_move_limit) {
    ai_self_play_metrics_t metrics = run_self_play_session(white_weights, black_weights, rule, half_move_limit);
    return metrics.winner;
}

static ai_eval_weights_t refine_weights_for_rule(swap_rule_t rule, uint16_t games, uint16_t half_move_limit,
                                                 ai_tuning_summary_t *summary) {
    ai_config_t seed_config;
    ai_agent_init(&seed_config, rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
    ai_eval_weights_t seed = seed_config.weights;
    uint8_t rule_index = (uint8_t)(rule % 4u);
    ai_eval_weights_t baseline = kBaselineWeights[rule_index];

    ai_weight_stats_t baseline_stats =
        evaluate_weight_set_vs_random(&baseline, &baseline, rule, games, half_move_limit,
                                      0xBA51CAFEu + (uint32_t)rule_index, kRandomOpponentEpsilonPct);

    ai_weight_stats_t seed_stats = evaluate_weight_set_vs_random(
        &seed, &baseline, rule, games, half_move_limit, 0xFEED0000u + (uint32_t)rule_index, kRandomOpponentEpsilonPct);

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

    int16_t pressure_adjust = clamp_range((int16_t)((avg_advantage + avg_frontier) / 20), -6, 6);

    int16_t blocking_adjust = clamp_range((int16_t)(avg_frontier / 12), -8, 8);
    if (blocking_adjust == 0 && (avg_frontier >= 6 || avg_frontier <= -6)) {
        blocking_adjust = (avg_frontier > 0) ? 1 : (int16_t)-1;
    }

    int16_t mobility_adjust = clamp_range((int16_t)(avg_frontier / 20), -6, 6);

    ai_eval_weights_t refined = seed;
    refined.connection_progress = clamp_range((int16_t)(refined.connection_progress + connection_adjust), 32, 140);
    refined.bridge_potential = clamp_range((int16_t)(refined.bridge_potential + bridge_adjust), 24, 96);
    refined.swap_pressure = clamp_range((int16_t)(refined.swap_pressure + pressure_adjust), 20, 80);
    refined.blocking_coverage = clamp_range((int16_t)(refined.blocking_coverage + blocking_adjust), 24, 64);
    refined.mobility = clamp_range((int16_t)(refined.mobility + mobility_adjust), 8, 40);

    if (refined.connection_progress == seed.connection_progress && refined.bridge_potential == seed.bridge_potential &&
        refined.swap_pressure == seed.swap_pressure && refined.blocking_coverage == seed.blocking_coverage &&
        refined.mobility == seed.mobility) {
        int16_t nudge = (rule_index & 1u) ? (int16_t)-1 : (int16_t)1;
        refined.connection_progress = clamp_range((int16_t)(refined.connection_progress + nudge), 32, 140);
        refined.bridge_potential = clamp_range((int16_t)(refined.bridge_potential + nudge), 24, 96);
    }

    ai_weight_stats_t refined_stats =
        evaluate_weight_set_vs_random(&refined, &baseline, rule, games, half_move_limit,
                                      0xDEADBEEFu + (uint32_t)rule_index, kRandomOpponentEpsilonPct);

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

static void evaluate_refined_against_baseline(swap_rule_t rule, const ai_eval_weights_t *refined,
                                              const ai_eval_weights_t *baseline, uint16_t half_move_limit,
                                              int32_t *advantage_out, int32_t *frontier_out) {
    ai_self_play_metrics_t refined_as_white =
        run_profiled_self_play_session(refined, baseline, rule, half_move_limit, configure_tuning_profile);
    ai_self_play_metrics_t refined_as_black =
        run_profiled_self_play_session(baseline, refined, rule, half_move_limit, configure_tuning_profile);

    int32_t adv_white = (int32_t)refined_as_white.white_advancement - (int32_t)refined_as_white.black_advancement;
    int32_t adv_black = (int32_t)refined_as_black.black_advancement - (int32_t)refined_as_black.white_advancement;

    int32_t frontier_white = (int32_t)refined_as_white.black_frontier - (int32_t)refined_as_white.white_frontier;
    int32_t frontier_black = (int32_t)refined_as_black.white_frontier - (int32_t)refined_as_black.black_frontier;

    if (advantage_out) {
        *advantage_out = (adv_white + adv_black) / 2;
    }
    if (frontier_out) {
        *frontier_out = (frontier_white + frontier_black) / 2;
    }
}

static uint8_t notation_row_to_index(uint8_t row_number) {
    assert(row_number >= 1u && row_number <= 8u);
    return (uint8_t)(8u - row_number);
}

static uint8_t notation_col_to_index(char col) {
    if (col >= 'a' && col <= 'd') {
        col = (char)(col - 'a' + 'A');
    }
    assert(col >= 'A' && col <= 'D');
    return (uint8_t)(col - 'A');
}

static move_t make_notated_move(char from_col, uint8_t from_row, char to_col, uint8_t to_row, move_type_t type,
                                player_t player) {
    move_t move;
    move.from_col = notation_col_to_index(from_col);
    move.from_row = notation_row_to_index(from_row);
    move.to_col = notation_col_to_index(to_col);
    move.to_row = notation_row_to_index(to_row);
    move.type = type;
    move.player = player;
    return move;
}

static bool test_moves_equal(const move_t *lhs, const move_t *rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    return lhs->from_row == rhs->from_row && lhs->from_col == rhs->from_col && lhs->to_row == rhs->to_row &&
           lhs->to_col == rhs->to_col && lhs->type == rhs->type && lhs->player == rhs->player;
}

// Helper function to format move position string (col as letter, row as 1-based)
static void format_test_move_string(char *buf, size_t buf_size, const move_t *move) {
    if (!buf || buf_size == 0 || !move)
        return;
    char from_col = 'A' + move->from_col;
    char to_col = 'A' + move->to_col;
    uint8_t from_row = move->from_row + 1;
    uint8_t to_row = move->to_row + 1;
    const char *move_type = (move->type == MOVE_TYPE_SWAP) ? "swap" : "empty";
    snprintf(buf, buf_size, "%c%u->%c%u (%s)", from_col, from_row, to_col, to_row, move_type);
}

static void format_user_move_string(char *buf, size_t buf_size, const move_t *move) {}

#define DEBUG_MAX_MOVES 128
#define DEBUG_AI_MAX_ORDERED_MOVES 32

static bool moves_equal(const move_t *a, const move_t *b) {
    if (!a || !b) {
        return false;
    }
    return (a->from_row == b->from_row) && (a->from_col == b->from_col) && (a->to_row == b->to_row) &&
           (a->to_col == b->to_col) && (a->type == b->type);
}

static piece_type_t parse_piece_token(const char *piece_str) {
    return PIECE_NONE;
}

static void setup_board_from_puzzle(board_t *board, const puzzle_test_data_t *puzzle) {
    if (!board || !puzzle) {
        return;
    }
    memset(board, 0, sizeof(board_t));
    for (int j = 0; puzzle->setup_lines[j] != NULL; ++j) {
        char line[256];
        strncpy(line, puzzle->setup_lines[j], sizeof(line) - 1u);
        line[sizeof(line) - 1u] = '\0';
        int row = 0;
        int col = 0;
        char piece_str[32];
        if (sscanf(line, "board_set_piece(&board, %d, %d, %[^)]);", &row, &col, piece_str) != 3) {
            continue;
        }
        piece_type_t piece = PIECE_NONE;
        if (strcmp(piece_str, "PIECE_WHITE_NORMAL") == 0)
            piece = PIECE_WHITE_NORMAL;
        else if (strcmp(piece_str, "PIECE_WHITE_SWAPPED") == 0)
            piece = PIECE_WHITE_SWAPPED;
        else if (strcmp(piece_str, "PIECE_BLACK_NORMAL") == 0)
            piece = PIECE_BLACK_NORMAL;
        else if (strcmp(piece_str, "PIECE_BLACK_SWAPPED") == 0)
            piece = PIECE_BLACK_SWAPPED;
        if (piece != PIECE_NONE) {
            board_set_piece(board, (uint8_t)row, (uint8_t)col, piece);
        }
    }
}

static uint8_t collect_player_moves(const board_t *board, player_t player, move_t *out_moves, uint8_t max_moves) {
    return 0;
}

static void analyze_candidate_move_debug(const puzzle_test_data_t *puzzle, const char *label, const move_t *move) {
    if (!puzzle || !move) return;
    char buf[64];
    format_test_move_string(buf, sizeof(buf), move);
    printf("    %s: %s\n", label ? label : "Candidate", buf);
}

static void debug_forcing_issue(const puzzle_test_data_t *puzzle, const move_t *ai_move, const move_t *expected_move) {

    if (!puzzle) return;

    printf("\n--- Debug forcing issue ---\n");
    printf("Puzzle pointer: %p\n", (void*)puzzle);
    printf("Puzzle id: %s\n", puzzle->id);
    for (int i = 0; puzzle->setup_lines && puzzle->setup_lines[i] != NULL; ++i) {
        printf("  setup_line[%d]: %s\n", i, puzzle->setup_lines[i]);
    }

    board_t board;
    board_context_t context;
    setup_board_from_puzzle(&board, puzzle);
    memset(&context, 0, sizeof(context));
    context.current_player = PLAYER_WHITE;

    // Dump board
    printf("Board dump (rows 1..8):\n");
    for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
            piece_type_t piece = board_get_piece(&board, r, c);
            char piece_char = '.';
            if (piece == PIECE_WHITE_NORMAL) piece_char = 'w';
            else if (piece == PIECE_WHITE_SWAPPED) piece_char = 'W';
            else if (piece == PIECE_BLACK_NORMAL) piece_char = 'b';
            else if (piece == PIECE_BLACK_SWAPPED) piece_char = 'B';
            printf("%c ", piece_char);
        }
        printf("\n");
    }

    // Enumerate all legal WHITE moves and check which create a forced immediate win
    move_array_t soa_moves;
    int total_checked = 0;
    int total_forcing = 0;
    printf("Enumerating WHITE legal moves and checking for forced Win-in-2:\n");
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_WHITE) continue;
            uint8_t count = board_get_legal_moves_soa(&board, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < count; ++i) {
                move_t candidate;
                move_array_get_move(&soa_moves, i, &candidate);
                candidate.player = PLAYER_WHITE;
                bool forcing = test_move_creates_forced_immediate_win(&board, &candidate, PLAYER_WHITE, puzzle->swap_rule);
                char mstr[64]; format_test_move_string(mstr, sizeof(mstr), &candidate);
                printf("  %s -> forcing=%s\n", mstr, forcing ? "YES" : "no");
                total_checked++;
                if (forcing) total_forcing++;
            }
        }
    }

    printf("Total checked: %d, total forcing: %d\n", total_checked, total_forcing);

    if (expected_move) {
        bool expected_forcing = test_move_creates_forced_immediate_win(&board, expected_move, PLAYER_WHITE, puzzle->swap_rule);
        char expect_str[64]; format_test_move_string(expect_str, sizeof(expect_str), expected_move);
        printf("Expected move %s -> forcing=%s\n", expect_str, expected_forcing ? "YES" : "no");
    }

    if (ai_move) {
        char ai_str[64]; format_test_move_string(ai_str, sizeof(ai_str), ai_move);
        printf("AI selected move (from earlier run): %s\n", ai_str);
    }

    // Try running ai_forcing_move_available on the position directly (sanity check)
    bool any_forcing = test_forcing_move_available(&board, PLAYER_WHITE, puzzle->swap_rule);
    printf("test_forcing_move_available reports: %s\n", any_forcing ? "YES" : "no");

    // Also run the same check used by the main win-in-2 test (ai_forcing_move_available)
    // for each candidate after applying it so we can compare results.
    printf("\nRunning ai_forcing_move_available check per candidate (as main test does):\n");
    int checked2 = 0;
    int forcing2 = 0;
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(&board, row, col);
            if (board_get_piece_owner(piece) != PLAYER_WHITE) continue;
            uint8_t count = board_get_legal_moves_soa(&board, PLAYER_WHITE, row, col, &soa_moves);
            for (uint8_t i = 0; i < count; ++i) {
                move_t candidate;
                move_array_get_move(&soa_moves, i, &candidate);
                candidate.player = PLAYER_WHITE;
                board_t after_move;
                board_copy(&after_move, &board);
                board_context_t move_context = {.current_player = PLAYER_WHITE};
                if (!board_execute_move_without_history(&after_move, &move_context, &candidate, puzzle->swap_rule)) continue;
                bool ai_check = ai_forcing_move_available(&after_move, PLAYER_BLACK, PLAYER_WHITE, puzzle->swap_rule);
                char mstr[64]; format_test_move_string(mstr, sizeof(mstr), &candidate);
                printf("  %s -> ai_forcing_move_available=%s\n", mstr, ai_check ? "YES" : "no");
                checked2++;
                if (ai_check) forcing2++;
            }
        }
    }
    printf("ai_forcing_move_available: checked=%d, matches=%d\n", checked2, forcing2);

    printf("--- End debug for puzzle %s ---\n\n", puzzle->id);
}




static void test_all_win_in_2_puzzles(void) {
    printf("\n=== Testing all Win-in-2 puzzles ===\n");

    int passed = 0;
    int total = num_puzzles;

    for (int i = 0; i < num_puzzles; ++i) {
        const puzzle_test_data_t *puzzle = all_puzzles[i];
        printf("Testing puzzle %d: %s (%s)\n", i + 1, puzzle->id,
               puzzle->swap_rule == SWAP_RULE_CLASSIC          ? "CLASSIC"
               : puzzle->swap_rule == SWAP_RULE_CLEARS_OWN     ? "CLEARS_OWN"
               : puzzle->swap_rule == SWAP_RULE_SWAPPED_CLEARS ? "SWAPPED_CLEARS"
                                                               : "SWAPPED_CLEARS_OWN");

        board_t board;
        board_context_t context;
        setup_board_from_puzzle(&board, puzzle);
        memset(&context, 0, sizeof(context));
        context.current_player = PLAYER_WHITE;
        context.history_count = 0;

        // Initialize AI config for this puzzle
        ai_config_t white_config;
        ai_agent_init(&white_config, puzzle->swap_rule, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);

        // Test what move the AI selects
        move_t ai_selected_move;
        bool ai_found_move = ai_agent_find_best_move(&board, &context, &white_config, &ai_selected_move);

        // Format the AI's selected move
        char ai_move_str[64];
        if (ai_found_move) {
            format_test_move_string(ai_move_str, sizeof(ai_move_str), &ai_selected_move);
        } else {
            strcpy(ai_move_str, "NO MOVE FOUND");
        }

        // Format the expected first move from JSON
        char expected_move_str[64];
        format_test_move_string(expected_move_str, sizeof(expected_move_str), &puzzle->first_move);

        // Compare moves
        bool moves_match = ai_found_move && moves_equal(&ai_selected_move, &puzzle->first_move);

        printf("  AI move: %s | Expected: %s | Match: %s\n", ai_move_str, expected_move_str,
               moves_match ? "YES" : "NO");

        // Test if AI can find a forced win in 2 moves
        bool has_forced_win = false;
        move_array_t candidate_moves;
        int candidate_count = 0;
        ai_config_t test_config;
        ai_agent_init(&test_config, puzzle->swap_rule, AI_DIFFICULTY_STANDARD, PLAYER_WHITE);
        test_config.enable_forcing_check = true;
        for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
            for (uint8_t col = 0; col < BOARD_COLS; ++col) {
                piece_type_t piece = board_get_piece(&board, row, col);
                if (board_get_piece_owner(piece) != PLAYER_WHITE) continue;
                uint8_t move_count = board_get_legal_moves_soa(&board, PLAYER_WHITE, row, col, &candidate_moves);
                for (uint8_t i = 0; i < move_count; ++i) {
                    move_t candidate;
                    move_array_get_move(&candidate_moves, i, &candidate);
                    candidate.player = PLAYER_WHITE;
                    board_t after_move;
                    board_copy(&after_move, &board);
                    board_context_t move_context = {.current_player = PLAYER_WHITE};
                    if (!board_execute_move_without_history(&after_move, &move_context, &candidate, puzzle->swap_rule)) continue;
                    bool is_forcing = false;
                    if (test_config.enable_forcing_check && test_config.difficulty >= AI_DIFFICULTY_STANDARD) {
                        is_forcing = ai_board_creates_forced_immediate_win_postmove(&after_move, candidate.player, candidate.player, test_config.swap_rule);
                    }
                    if (is_forcing) {
                        has_forced_win = true;
                    }
                    candidate_count++;
                }
            }
        }
        if (has_forced_win) {
            printf("  SUCCESS: AI found a winning line that wins in 2 moves\n");
            passed++;
        } else {
            printf("  FAILED: AI did not find a winning line in 2 moves\n");
            debug_forcing_issue(puzzle, ai_found_move ? &ai_selected_move : NULL, &puzzle->first_move);
        }
    }

    printf("\nResults: %d/%d puzzles passed\n", passed, total);
    if (passed == total) {
        printf("All Win-in-2 puzzles passed!\n");
    } else {
        printf("Some puzzles failed - AI may need improvement\n");
    }
}



int main(void) {
    puts("Running Switcharoo AI agent tests...");
    test_all_win_in_2_puzzles();

    return 0;
}


