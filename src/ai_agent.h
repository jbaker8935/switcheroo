/**
 * @file ai_agent.h
 * @brief AI agent for F256 Switcharoo
 * 
 * Implements heuristic-based AI with multiple difficulty levels.
 */

#ifndef AI_AGENT_H
#define AI_AGENT_H

#include <stdint.h>

#ifdef AI_AGENT_HOST_TEST
#include <stdbool.h>
#include "../tests/include/f256lib_host.h"
#else
#include "platform_f256.h"
#endif

#include "../src/board.h"

// Callback type for AI search progress updates
typedef void (*ai_progress_callback_t)(uint8_t current_depth, uint32_t nodes_searched, void *user_data);

// Difficulty levels
typedef enum {
    AI_DIFFICULTY_LEARNING = 0,  // Random moves
    AI_DIFFICULTY_EASY = 1,      // Basic heuristics
    AI_DIFFICULTY_STANDARD = 2,  // Full heuristics
    AI_DIFFICULTY_EXPERT = 3     // Full heuristics + lookahead
} ai_difficulty_t;

typedef enum {
    AI_BLUNDER_NONE = 0,
    AI_BLUNDER_ALLOW_IMMEDIATE_WIN = 1,
    AI_BLUNDER_ALLOW_FORCING_MOVE = 2
} ai_blunder_type_t;

typedef struct {
    int16_t connection_progress;
    int16_t bridge_potential;
    int16_t swap_pressure;
    int16_t blocking_coverage;
    int16_t mobility;
} ai_eval_weights_t;

typedef struct {
    int16_t connection_progress;
    int16_t bridge_potential;
    int16_t swap_pressure;
    int16_t blocking_coverage;
    int16_t mobility;
    int16_t total;
} ai_eval_breakdown_t;

typedef struct {
    uint8_t base_depth;
    uint8_t max_depth;
    uint8_t max_extension;
    uint32_t node_limit;
    uint16_t time_limit_ms;
    bool use_iterative_deepening;
    bool use_transposition;
    bool use_move_ordering;
    bool use_killer_moves;
} ai_search_settings_t;

typedef struct {
    swap_rule_t swap_rule;
    ai_difficulty_t difficulty;
    player_t ai_player;
    ai_eval_weights_t weights;
    ai_search_settings_t search;
    bool diagnostics_enabled;
    bool enable_forcing_check;
    bool use_hint_profile;
    ai_progress_callback_t progress_callback;
    void *progress_user_data;
    uint8_t random_top_k;
    uint8_t random_epsilon_pct;
    bool blunder_enabled;
    uint8_t blunder_chance_pct;
    ai_blunder_type_t blunder_type;
} ai_config_t;

// Initialize AI agent
void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule, 
                   ai_difficulty_t difficulty, player_t ai_player);

// Find and return the best move for the current player
// Returns true if a move was found, false otherwise
bool ai_agent_find_best_move(const board_t *board, const ai_config_t *config,
                             move_t *out_move);

// Evaluate a board position from the perspective of a player
// Higher scores are better for that player
int16_t ai_agent_evaluate_board(const board_t *board, player_t player,
                                const ai_config_t *config);

// Retrieve the feature breakdown for the most recent move selection.
// If diagnostics are disabled, all fields are set to zero.
void ai_agent_get_last_breakdown(ai_eval_breakdown_t *out);

// Register a callback for search progress updates
void ai_agent_set_progress_callback(ai_config_t *config, ai_progress_callback_t callback,
                                    void *user_data);

void ai_agent_config_set_randomization(ai_config_t *config, uint8_t top_k, uint8_t epsilon_pct);
void ai_agent_config_set_blunder(ai_config_t *config, bool enabled, ai_blunder_type_t type, uint8_t chance_pct);
ai_blunder_type_t ai_allowed_blunder_type(ai_difficulty_t difficulty);
void ai_agent_set_random_seed(uint32_t seed);

typedef struct {
    uint32_t board_hash;
    player_t perspective;
    int16_t total;
    int16_t swap_contrib;
    int16_t block_contrib;
    int16_t goal_contrib;
    uint8_t depth;
    uint8_t ply;
} ai_hint_eval_record_t;

void ai_agent_hint_trace_enable(bool enabled);
void ai_agent_hint_trace_clear(void);
uint8_t ai_agent_hint_trace_get(const ai_hint_eval_record_t **out_records);

#ifdef AI_AGENT_HOST_TEST
bool ai_agent_detect_unavoidable_loss(const board_t *board, const ai_config_t *config);
bool ai_agent_move_creates_forced_immediate_win(const board_t *board,
                                                const move_t *move,
                                                const ai_config_t *config,
                                                player_t ai_player);
bool ai_agent_move_allows_opponent_immediate_win(const board_t *board,
                                                 const move_t *move,
                                                 const ai_config_t *config,
                                                 player_t ai_player);
#endif

#endif // AI_AGENT_H
