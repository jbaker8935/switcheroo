// Achievement tracking helpers for Switcharoo progression targets.
// Freeplay order:
// 0. Win first game
// 1. Win 10 games
// 2. Win all starting positions
// 3. Win a game in all swap rules
// 4. Win against Expert difficulty
// 5. Win 10 against Expert difficulty
// 6. Win in under 10 moves
// 7. Win 100 games
// Puzzle order:
// 8. Solve first puzzle
// 9. Solve 10 puzzles in under 30 seconds each
// 10. Solve Win in 3 without Hints
// 11. Solve Win in 4 without Hints
// 12. Solve 35 without Hints
// 13. Solve 50 puzzles in one session
// 14. Solve all puzzles for a swap rule
// 15. Solve all puzzles in the collection

#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <stdint.h>
#if defined(AI_AGENT_HOST_TEST)
#include <stdbool.h>
#endif

#include "../src/ai_agent.h"
#include "../src/board.h"

struct puzzle_t;

enum {
	ACHIEVEMENT_COUNT = 16u,
	ACHIEVEMENT_FREEPLAY_COUNT = 8u,
	ACHIEVEMENT_PUZZLE_COUNT = 8u,
	ACHIEVEMENT_PUZZLE_TIMER_TICKS = 900u
};

typedef enum {
	ACH_FREEPLAY_FIRST_WIN = 0,
	ACH_FREEPLAY_TEN_WINS = 1,
	ACH_FREEPLAY_ALL_LAYOUTS = 2,
	ACH_FREEPLAY_ALL_SWAP_RULES = 3,
	ACH_FREEPLAY_EXPERT_WIN = 4,
	ACH_FREEPLAY_EXPERT_TEN_WINS = 5,
	ACH_FREEPLAY_UNDER_TEN_MOVES = 6,
	ACH_FREEPLAY_HUNDRED_WINS = 7,
	ACH_PUZZLE_FIRST_SOLVE = 8,
	ACH_PUZZLE_FAST_TEN = 9,
	ACH_PUZZLE_WIN_IN_THREE = 10,
	ACH_PUZZLE_WIN_IN_FOUR = 11,
	ACH_PUZZLE_NO_HINT_TWENTY_FIVE = 12,
	ACH_PUZZLE_SESSION_FIFTY = 13,
	ACH_PUZZLE_RULE_COMPLETE = 14,
	ACH_PUZZLE_CATALOG_COMPLETE = 15
} achievement_id_t;

typedef struct {
	uint16_t unlocked_mask;
	uint16_t progress_count[ACHIEVEMENT_COUNT];
	uint8_t detail_bits[ACHIEVEMENT_COUNT];
	uint16_t freeplay_total_wins;
	uint16_t freeplay_expert_wins;
	uint16_t puzzle_total_solves;
	uint16_t puzzle_fast_solves;
	uint16_t puzzle_no_hint_solves;
	uint16_t solved_puzzles_catalog;
	uint16_t total_puzzles_catalog;
	uint16_t solved_puzzles_per_rule[NUMBER_OF_SWAP_RULES];
	uint16_t total_puzzles_per_rule[NUMBER_OF_SWAP_RULES];
	uint8_t puzzle_session_solves;
	uint8_t puzzle_timer_active;
	uint8_t puzzle_timer_expired;
	uint8_t puzzle_hint_used;
	uint8_t puzzle_attempt_active;
	uint8_t puzzle_solution_length;
	uint8_t puzzle_rule;
} achievements_state_t;

void achievements_init(achievements_state_t *state);
void achievements_refresh_catalog(achievements_state_t *state, swap_rule_t active_rule, uint16_t active_index);
void achievements_on_game_mode_changed(achievements_state_t *state, bool was_puzzle_mode, bool is_puzzle_mode);
void achievements_on_freeplay_win(achievements_state_t *state, uint8_t layout_id, swap_rule_t rule, ai_difficulty_t difficulty, uint8_t move_count);
void achievements_on_puzzle_loaded(achievements_state_t *state, const struct puzzle_t *puzzle);
void achievements_on_puzzle_hint(achievements_state_t *state);
void achievements_on_puzzle_attempt_completed(achievements_state_t *state, const struct puzzle_t *puzzle,  bool qualifies_for_mark);
void achievements_on_puzzle_failed(achievements_state_t *state);
void achievements_update_timer(achievements_state_t *state, bool alarm_elapsed);

uint16_t achievements_storage_size(void);
uint16_t achievements_serialize(const achievements_state_t *state, uint8_t *buffer, uint16_t max_bytes);
bool achievements_deserialize(achievements_state_t *state, const uint8_t *data, uint16_t length);

static inline uint16_t achievements_unlocked_mask(const achievements_state_t *state) {
	return state ? state->unlocked_mask : 0u;
}

static inline const uint16_t *achievements_progress(const achievements_state_t *state) {
	return state ? state->progress_count : NULL;
}

static inline const uint8_t *achievements_detail_bits(const achievements_state_t *state) {
	return state ? state->detail_bits : NULL;
}

void display_achievements_screen(achievements_state_t *state, uint8_t page);
void hide_achievements_screen(void);
void numbytestohex(uint8_t byte_count, const uint8_t *data, char *out_hex);

#endif // ACHIEVEMENTS_H

