/**
 * @file ai_agent.h
 * @brief AI agent for F256 Switcharoo
 * 
 * Implements heuristic-based AI with multiple difficulty levels.
 */

#ifndef AI_AGENT_H
#define AI_AGENT_H

#include "f256lib.h"
#include "../src/board.h"
#include <stdint.h>

// Difficulty levels
typedef enum {
    AI_DIFFICULTY_LEARNING = 0,  // Random moves
    AI_DIFFICULTY_EASY = 1,      // Basic heuristics
    AI_DIFFICULTY_STANDARD = 2,  // Full heuristics
    AI_DIFFICULTY_EXPERT = 3     // Full heuristics + lookahead
} ai_difficulty_t;

// AI configuration
typedef struct {
    swap_rule_t swap_rule;
    ai_difficulty_t difficulty;
    player_t ai_player;
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

#endif // AI_AGENT_H
