#include "../src/ai_agent.h"
#include "../src/board.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *label;
    ai_difficulty_t difficulty;
    uint8_t random_top_k;
    uint8_t random_epsilon_pct;
} competitor_profile_t;

typedef struct {
    const char *label;
    competitor_profile_t competitors[2];
    uint32_t games;
    uint16_t half_move_cap;
    uint8_t layout_id;
    uint32_t seed;
    bool alternate_colors;
} experiment_config_t;

typedef struct {
    uint32_t wins;
    uint32_t losses;
    uint32_t draws;
    uint64_t total_half_moves;
    int64_t advantage_sum;
    int64_t frontier_sum;
    uint32_t as_white_games;
    uint32_t as_black_games;
} competitor_stats_t;

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

static const char *difficulty_to_string(ai_difficulty_t difficulty) {
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            return "Learning";
        case AI_DIFFICULTY_EASY:
            return "Easy";
        case AI_DIFFICULTY_STANDARD:
            return "Standard";
        case AI_DIFFICULTY_EXPERT:
            return "Expert";
        default:
            return "Unknown";
    }
}

static void print_summary(const experiment_config_t *config, const competitor_stats_t stats[2]) {
    printf("\nExperiment: %s\n", config->label);
    printf("  Games: %u | Half-move cap: %u | Layout: %u | Seed: 0x%08X\n",
           config->games,
           config->half_move_cap,
           config->layout_id,
           config->seed);
    for (int i = 0; i < 2; ++i) {
        const competitor_profile_t *profile = &config->competitors[i];
        const competitor_stats_t *stat = &stats[i];
        uint32_t games_played = stat->wins + stat->losses + stat->draws;
        double denom = (games_played > 0u) ? (double)games_played : 1.0;
        double win_rate = (denom > 0.0) ? (100.0 * (double)stat->wins / denom) : 0.0;
        double avg_half_moves = stat->total_half_moves / denom;
        double avg_advantage = stat->advantage_sum / denom;
        double avg_frontier = stat->frontier_sum / denom;

        printf("  %s [%s", profile->label, difficulty_to_string(profile->difficulty));
        if (profile->random_top_k > 1u) {
            printf(" topK=%u", profile->random_top_k);
        }
        if (profile->random_epsilon_pct > 0u) {
            printf(" ε=%u%%", profile->random_epsilon_pct);
        }
        printf("] W:%u L:%u D:%u Win%%:%.1f AvgHalfMoves:%.2f AdvΔ:%.2f FrontΔ:%.2f (White:%u | Black:%u)\n",
               stat->wins,
               stat->losses,
               stat->draws,
               win_rate,
               avg_half_moves,
               avg_advantage,
               avg_frontier,
               stat->as_white_games,
               stat->as_black_games);
    }
}

static void setup_competitor(competitor_profile_t *profile,
                             const char *label,
                             ai_difficulty_t difficulty,
                             uint8_t top_k,
                             uint8_t epsilon_pct) {
    profile->label = label;
    profile->difficulty = difficulty;
    profile->random_top_k = (top_k == 0u) ? 1u : top_k;
    profile->random_epsilon_pct = epsilon_pct;
}

static void run_experiment(const experiment_config_t *config) {
    competitor_stats_t stats[2];
    memset(stats, 0, sizeof(stats));

    for (uint32_t game = 0; game < config->games; ++game) {
        bool swap_colours = config->alternate_colors && ((game & 1u) == 1u);
        const competitor_profile_t *white_profile = &config->competitors[swap_colours ? 1 : 0];
        const competitor_profile_t *black_profile = &config->competitors[swap_colours ? 0 : 1];

        ai_agent_set_random_seed(config->seed ^ (((uint32_t)game + 1u) * 0x9E3779B9u));

        board_t board;
        board_init(&board);
        board_set_starting_layout(&board, config->layout_id);
        board.current_player = PLAYER_WHITE;

        ai_config_t white_cfg;
        ai_agent_init(&white_cfg, SWAP_RULE_CLASSIC, white_profile->difficulty, PLAYER_WHITE);
        ai_agent_config_set_randomization(&white_cfg,
                                          white_profile->random_top_k,
                                          white_profile->random_epsilon_pct);

        ai_config_t black_cfg;
        ai_agent_init(&black_cfg, SWAP_RULE_CLASSIC, black_profile->difficulty, PLAYER_BLACK);
        ai_agent_config_set_randomization(&black_cfg,
                                          black_profile->random_top_k,
                                          black_profile->random_epsilon_pct);

        uint16_t plies_played = 0;
        player_t winner = PLAYER_NONE;

        for (uint16_t ply = 0; ply < config->half_move_cap; ++ply) {
            ai_config_t *active_cfg = (board.current_player == PLAYER_WHITE) ? &white_cfg : &black_cfg;
            move_t move;
            bool found = ai_agent_find_best_move(&board, active_cfg, &move);

            if (!found) {
                break;
            }

            if (!board_execute_move(&board, &move, active_cfg->swap_rule)) {
                break;
            }

            plies_played = (uint16_t)(ply + 1u);

            if (board_check_win_fast(&board, active_cfg->ai_player)) {
                winner = active_cfg->ai_player;
                break;
            }

            board_switch_turn(&board);

            if (!board_has_legal_moves(&board, board.current_player)) {
                break;
            }
        }

        uint16_t white_advancement = compute_advancement_score(&board, PLAYER_WHITE);
        uint16_t black_advancement = compute_advancement_score(&board, PLAYER_BLACK);
        uint8_t white_frontier = compute_frontier_row(&board, PLAYER_WHITE);
        uint8_t black_frontier = compute_frontier_row(&board, PLAYER_BLACK);

        int white_index = swap_colours ? 1 : 0;
        int black_index = swap_colours ? 0 : 1;

        competitor_stats_t *white_stats = &stats[white_index];
        competitor_stats_t *black_stats = &stats[black_index];

        white_stats->as_white_games++;
        black_stats->as_black_games++;

        white_stats->total_half_moves += plies_played;
        black_stats->total_half_moves += plies_played;

        int32_t adv_delta = (int32_t)white_advancement - (int32_t)black_advancement;
        int32_t frontier_delta = (int32_t)black_frontier - (int32_t)white_frontier;

        white_stats->advantage_sum += adv_delta;
        white_stats->frontier_sum += frontier_delta;
        black_stats->advantage_sum -= adv_delta;
        black_stats->frontier_sum -= frontier_delta;

        if (winner == PLAYER_WHITE) {
            white_stats->wins++;
            black_stats->losses++;
        } else if (winner == PLAYER_BLACK) {
            black_stats->wins++;
            white_stats->losses++;
        } else {
            white_stats->draws++;
            black_stats->draws++;
        }
    }

    print_summary(config, stats);
}

int main(void) {
    const uint32_t games = 120u;
    const uint16_t half_move_cap = 160u;
    const uint8_t layout_id = 2u;
    const bool alternate_colors = true;

    experiment_config_t config;
    config.games = games;
    config.half_move_cap = half_move_cap;
    config.layout_id = layout_id;
    config.alternate_colors = alternate_colors;

    // Baseline: deterministic Easy vs deterministic Expert
    config.label = "Easy vs Expert (deterministic head-to-head)";
    setup_competitor(&config.competitors[0], "Easy", AI_DIFFICULTY_EASY, 1u, 0u);
    setup_competitor(&config.competitors[1], "Expert", AI_DIFFICULTY_EXPERT, 1u, 0u);
    config.seed = 0xE45E90A1u;
    run_experiment(&config);

    // Baseline: Easy randomness vs Expert randomness
    char expert_eps20_label[32];
    snprintf(expert_eps20_label, sizeof(expert_eps20_label), "Expert ε=%u%%", 20u);
    config.label = "Easy vs Expert (Expert ε-random baseline)";
    setup_competitor(&config.competitors[0], "Easy", AI_DIFFICULTY_EASY, 4u, 20u);
    setup_competitor(&config.competitors[1], expert_eps20_label, AI_DIFFICULTY_EXPERT, 3u, 20u);
    config.seed = 0x5A17C3D7u;
    run_experiment(&config);

    // Baseline: deterministic Expert mirror
    config.label = "Expert vs Expert (deterministic mirror)";
    setup_competitor(&config.competitors[0], "Expert", AI_DIFFICULTY_EXPERT, 1u, 0u);
    setup_competitor(&config.competitors[1], "Expert", AI_DIFFICULTY_EXPERT, 1u, 0u);
    config.seed = 0x1ABCDEF0u;
    run_experiment(&config);

    // Series: deterministic Expert vs Expert with varying epsilon on opponent
    const uint8_t expert_epsilons[] = { 5u, 10u, 15u, 20u, 25u };
    for (size_t i = 0; i < sizeof(expert_epsilons) / sizeof(expert_epsilons[0]); ++i) {
        uint8_t epsilon = expert_epsilons[i];
        char label_buf[96];
        char opponent_label[32];
        snprintf(label_buf, sizeof(label_buf), "Expert vs Expert (ε=%u%% opponent)", epsilon);
        snprintf(opponent_label, sizeof(opponent_label), "Expert ε=%u%%", epsilon);
        config.label = label_buf;
        setup_competitor(&config.competitors[0], "Expert", AI_DIFFICULTY_EXPERT, 1u, 0u);
        setup_competitor(&config.competitors[1], opponent_label, AI_DIFFICULTY_EXPERT, 3u, epsilon);
        config.seed = 0x31415926u ^ ((uint32_t)epsilon * 0x9E3779B9u);
        run_experiment(&config);
    }

    // Series: Standard vs Expert with varying epsilon on Expert
    const uint8_t standard_epsilons[] = { 5u, 10u, 15u, 20u };
    for (size_t i = 0; i < sizeof(standard_epsilons) / sizeof(standard_epsilons[0]); ++i) {
        uint8_t epsilon = standard_epsilons[i];
        char label_buf[96];
        char opponent_label[32];
        snprintf(label_buf, sizeof(label_buf), "Standard vs Expert (ε=%u%% opponent)", epsilon);
        snprintf(opponent_label, sizeof(opponent_label), "Expert ε=%u%%", epsilon);
        config.label = label_buf;
        setup_competitor(&config.competitors[0], "Standard", AI_DIFFICULTY_STANDARD, 2u, 5u);
        setup_competitor(&config.competitors[1], opponent_label, AI_DIFFICULTY_EXPERT, 3u, epsilon);
        config.seed = 0x2468ACE1u ^ ((uint32_t)epsilon * 0x7F4A7C15u);
        run_experiment(&config);
    }

    // Series: Easy vs Expert with varying epsilon on Expert
    const uint8_t easy_epsilons[] = { 5u, 10u, 15u, 20u };
    for (size_t i = 0; i < sizeof(easy_epsilons) / sizeof(easy_epsilons[0]); ++i) {
        uint8_t epsilon = easy_epsilons[i];
        char label_buf[96];
        char opponent_label[32];
        snprintf(label_buf, sizeof(label_buf), "Easy vs Expert (ε=%u%% opponent)", epsilon);
        snprintf(opponent_label, sizeof(opponent_label), "Expert ε=%u%%", epsilon);
        config.label = label_buf;
        setup_competitor(&config.competitors[0], "Easy", AI_DIFFICULTY_EASY, 4u, 20u);
        setup_competitor(&config.competitors[1], opponent_label, AI_DIFFICULTY_EXPERT, 3u, epsilon);
        config.seed = 0x5A17C3D7u ^ ((uint32_t)epsilon * 0xA511E9B3u);
        run_experiment(&config);
    }

    puts("\nAI difficulty experiments complete.");
    return 0;
}