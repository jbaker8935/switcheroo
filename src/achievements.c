#include "../src/achievements.h"
#include "../src/puzzle_data.h"
#include "../src/timer.h"

#include <string.h>

enum {
	ACHIEVEMENTS_STORAGE_VERSION = 1u
};

static uint16_t clamp_u16(uint16_t value, uint16_t limit) {
	return (value > limit) ? limit : value;
}

static uint8_t clamp_u8(uint16_t value) {
	return (value > 0xFFu) ? 0xFFu : (uint8_t)value;
}

static uint8_t count_bits8(uint8_t value) {
	uint8_t count = 0u;
	while (value != 0u) {
		value &= (uint8_t)(value - 1u);
		++count;
	}
	return count;
}

static void achievements_unlock(achievements_state_t *state, achievement_id_t achievement) {
	if (!state) {
		return;
	}
	state->unlocked_mask |= (uint16_t)(1u << achievement);
}

static bool achievements_is_unlocked(const achievements_state_t *state, achievement_id_t achievement) {
	if (!state) {
		return false;
	}
	return (state->unlocked_mask & (uint16_t)(1u << achievement)) != 0u;
}

static void achievements_update_rule_progress(achievements_state_t *state, swap_rule_t rule) {
	if (!state || rule >= NUMBER_OF_SWAP_RULES) {
		return;
	}

	const uint16_t total = state->total_puzzles_per_rule[rule];
	const uint16_t solved = state->solved_puzzles_per_rule[rule];

	if (total > 0u && solved >= total) {
		state->detail_bits[ACH_PUZZLE_RULE_COMPLETE] |= (uint8_t)(1u << rule);
		achievements_unlock(state, ACH_PUZZLE_RULE_COMPLETE);
	} else {
		state->detail_bits[ACH_PUZZLE_RULE_COMPLETE] &=
			(uint8_t)~(1u << rule);
	}

	state->progress_count[ACH_PUZZLE_RULE_COMPLETE] =
		count_bits8(state->detail_bits[ACH_PUZZLE_RULE_COMPLETE]);

	if (state->progress_count[ACH_PUZZLE_RULE_COMPLETE] >= NUMBER_OF_SWAP_RULES &&
		NUMBER_OF_SWAP_RULES > 0u) {
		achievements_unlock(state, ACH_PUZZLE_RULE_COMPLETE);
	}
}

static void achievements_update_catalog_progress(achievements_state_t *state) {
	if (!state) {
		return;
	}

	if (state->total_puzzles_catalog == 0u) {
		state->progress_count[ACH_PUZZLE_CATALOG_COMPLETE] = 0u;
		return;
	}

	state->progress_count[ACH_PUZZLE_CATALOG_COMPLETE] = clamp_u16(
		state->solved_puzzles_catalog,
		state->total_puzzles_catalog
	);

	if (state->solved_puzzles_catalog >= state->total_puzzles_catalog) {
		achievements_unlock(state, ACH_PUZZLE_CATALOG_COMPLETE);
	}
}

static void achievements_reset_puzzle_attempt(achievements_state_t *state) {
	if (!state) {
		return;
	}

	state->puzzle_timer_active = 0u;
	state->puzzle_timer_expired = 0u;
	state->puzzle_hint_used = 0u;
	state->puzzle_attempt_active = 0u;
	state->puzzle_solution_length = 0u;
}

void achievements_init(achievements_state_t *state) {
	if (!state) {
		return;
	}

	memset(state, 0, sizeof(achievements_state_t));
}

void achievements_refresh_catalog(achievements_state_t *state, swap_rule_t active_rule, uint16_t active_index) {
	if (!state) {
		return;
	}

	memset(state->total_puzzles_per_rule, 0, sizeof(state->total_puzzles_per_rule));
	memset(state->solved_puzzles_per_rule, 0, sizeof(state->solved_puzzles_per_rule));

	state->total_puzzles_catalog = 0u;
	state->solved_puzzles_catalog = 0u;
	state->detail_bits[ACH_PUZZLE_RULE_COMPLETE] = 0u;

	const swap_rule_t original_rule = get_current_puzzle_swap_rule();
	swap_rule_t restore_rule = active_rule;
	if (restore_rule >= NUMBER_OF_SWAP_RULES) {
		restore_rule = original_rule;
	}

	for (uint8_t rule = 0u; rule < NUMBER_OF_SWAP_RULES; ++rule) {
		const swap_rule_t loop_rule = (swap_rule_t)rule;
		set_current_puzzle_swap_rule(loop_rule);

		const puzzle_collection_t *collection = get_puzzle_collection();
		uint16_t count = 0u;
		if (collection && collection->count <= UINT16_MAX) {
			count = (uint16_t)collection->count;
		}

		state->total_puzzles_per_rule[rule] = count;
		state->total_puzzles_catalog = (uint16_t)(state->total_puzzles_catalog + count);

		uint16_t solved_count = 0u;
		for (uint16_t i = 0u; i < count; ++i) {
			const puzzle_t *puzzle = get_puzzle_by_index(i);
			if (puzzle && puzzle->is_solved) {
				++solved_count;
			}
		}

		state->solved_puzzles_per_rule[rule] = solved_count;
		state->solved_puzzles_catalog = (uint16_t)(state->solved_puzzles_catalog + solved_count);
		achievements_update_rule_progress(state, loop_rule);
	}

	set_current_puzzle_swap_rule(restore_rule);
	const puzzle_collection_t *restore_collection = get_puzzle_collection();
	if (restore_collection && active_index < restore_collection->count) {
		(void)get_puzzle_by_index(active_index);
	}

	achievements_update_catalog_progress(state);
}

void achievements_on_game_mode_changed(achievements_state_t *state, bool was_puzzle_mode, bool is_puzzle_mode) {
	if (!state) {
		return;
	}

	if (!was_puzzle_mode && is_puzzle_mode) {
		state->puzzle_session_solves = 0u;
		achievements_reset_puzzle_attempt(state);
	} else if (was_puzzle_mode && !is_puzzle_mode) {
		achievements_reset_puzzle_attempt(state);
	}
}

void achievements_on_freeplay_win(achievements_state_t *state,
								  uint8_t layout_id,
								  swap_rule_t rule,
								  ai_difficulty_t difficulty,
								  uint8_t move_count) {
	if (!state) {
		return;
	}

	if (state->freeplay_total_wins < UINT16_MAX) {
		++state->freeplay_total_wins;
	}

	state->progress_count[ACH_FREEPLAY_FIRST_WIN] = clamp_u16(state->freeplay_total_wins, 1u);
	state->progress_count[ACH_FREEPLAY_TEN_WINS] = clamp_u16(state->freeplay_total_wins, 10u);
	state->progress_count[ACH_FREEPLAY_HUNDRED_WINS] = clamp_u16(state->freeplay_total_wins, 100u);

	if (state->freeplay_total_wins >= 1u) {
		achievements_unlock(state, ACH_FREEPLAY_FIRST_WIN);
	}
	if (state->freeplay_total_wins >= 10u) {
		achievements_unlock(state, ACH_FREEPLAY_TEN_WINS);
	}
	if (state->freeplay_total_wins >= 100u) {
		achievements_unlock(state, ACH_FREEPLAY_HUNDRED_WINS);
	}

	if (layout_id < NUM_STARTING_LAYOUTS) {
		state->detail_bits[ACH_FREEPLAY_ALL_LAYOUTS] |= (uint8_t)(1u << layout_id);
		state->progress_count[ACH_FREEPLAY_ALL_LAYOUTS] = count_bits8(
			state->detail_bits[ACH_FREEPLAY_ALL_LAYOUTS]
		);
		if (state->progress_count[ACH_FREEPLAY_ALL_LAYOUTS] >= NUM_STARTING_LAYOUTS) {
			achievements_unlock(state, ACH_FREEPLAY_ALL_LAYOUTS);
		}
	}

	if (rule < NUMBER_OF_SWAP_RULES) {
		state->detail_bits[ACH_FREEPLAY_ALL_SWAP_RULES] |= (uint8_t)(1u << rule);
		state->progress_count[ACH_FREEPLAY_ALL_SWAP_RULES] = count_bits8(
			state->detail_bits[ACH_FREEPLAY_ALL_SWAP_RULES]
		);
		if (state->progress_count[ACH_FREEPLAY_ALL_SWAP_RULES] >= NUMBER_OF_SWAP_RULES) {
			achievements_unlock(state, ACH_FREEPLAY_ALL_SWAP_RULES);
		}
	}

	if (difficulty == AI_DIFFICULTY_EXPERT) {
		if (state->freeplay_expert_wins < UINT16_MAX) {
			++state->freeplay_expert_wins;
		}
		state->progress_count[ACH_FREEPLAY_EXPERT_WIN] = clamp_u16(state->freeplay_expert_wins, 1u);
		state->progress_count[ACH_FREEPLAY_EXPERT_TEN_WINS] = clamp_u16(state->freeplay_expert_wins, 10u);

		if (state->freeplay_expert_wins >= 1u) {
			achievements_unlock(state, ACH_FREEPLAY_EXPERT_WIN);
		}
		if (state->freeplay_expert_wins >= 10u) {
			achievements_unlock(state, ACH_FREEPLAY_EXPERT_TEN_WINS);
		}
	}

	if (move_count < 10u) {
		uint16_t previous_best = state->progress_count[ACH_FREEPLAY_UNDER_TEN_MOVES];
		if (!achievements_is_unlocked(state, ACH_FREEPLAY_UNDER_TEN_MOVES) ||
			move_count < previous_best || previous_best == 0u) {
			state->progress_count[ACH_FREEPLAY_UNDER_TEN_MOVES] = move_count;
		}
		achievements_unlock(state, ACH_FREEPLAY_UNDER_TEN_MOVES);
	}
}

void achievements_on_puzzle_loaded(achievements_state_t *state, const struct puzzle_t *puzzle) {
	if (!state) {
		return;
	}

	achievements_reset_puzzle_attempt(state);

	if (!puzzle) {
		return;
	}

	state->puzzle_attempt_active = 1u;
	state->puzzle_rule = (uint8_t)puzzle->swap_rule;
	state->puzzle_solution_length = puzzle->solution_length;
	state->puzzle_timer_active = 1u;
	state->puzzle_timer_expired = 0u;
	state->puzzle_hint_used = 0u;

	setAlarm(ACHIEVEMENT_PUZZLE_TIMER_TICKS);
}

void achievements_on_puzzle_hint(achievements_state_t *state) {
	if (!state) {
		return;
	}

	state->puzzle_hint_used = 1u;
}

void achievements_on_puzzle_attempt_completed(achievements_state_t *state,
											  const struct puzzle_t *puzzle,
											  bool qualifies_for_mark,
											  bool newly_marked) {
	if (!state) {
		return;
	}

	uint8_t solved_fast = (state->puzzle_timer_expired == 0u) ? 1u : 0u;
	uint8_t no_hint = (state->puzzle_hint_used == 0u) ? 1u : 0u;

	if (qualifies_for_mark) {
		if (state->puzzle_total_solves < UINT16_MAX) {
			++state->puzzle_total_solves;
		}
		state->progress_count[ACH_PUZZLE_FIRST_SOLVE] = clamp_u16(state->puzzle_total_solves, 1u);
		achievements_unlock(state, ACH_PUZZLE_FIRST_SOLVE);

		if (solved_fast) {
			if (state->puzzle_fast_solves < UINT16_MAX) {
				++state->puzzle_fast_solves;
			}
			state->progress_count[ACH_PUZZLE_FAST_TEN] = clamp_u16(state->puzzle_fast_solves, 10u);
			if (state->puzzle_fast_solves >= 10u) {
				achievements_unlock(state, ACH_PUZZLE_FAST_TEN);
			}
		}

		if (no_hint) {
			if (state->puzzle_no_hint_solves < UINT16_MAX) {
				++state->puzzle_no_hint_solves;
			}
			state->progress_count[ACH_PUZZLE_NO_HINT_THIRTY_FIVE] = clamp_u16(
				state->puzzle_no_hint_solves,
				35u
			);
			if (state->puzzle_no_hint_solves >= 35u) {
				achievements_unlock(state, ACH_PUZZLE_NO_HINT_THIRTY_FIVE);
			}

			if (puzzle) {
				if (puzzle->solution_length <= 3u) {
					achievements_unlock(state, ACH_PUZZLE_WIN_IN_THREE);
					state->progress_count[ACH_PUZZLE_WIN_IN_THREE] = 1u;
				} else if (puzzle->solution_length == 4u) {
					achievements_unlock(state, ACH_PUZZLE_WIN_IN_FOUR);
					state->progress_count[ACH_PUZZLE_WIN_IN_FOUR] = 1u;
				}
			}
		}

		if (state->puzzle_session_solves < 0xFFu) {
			++state->puzzle_session_solves;
		}
		state->progress_count[ACH_PUZZLE_SESSION_FIFTY] = clamp_u16(
			state->puzzle_session_solves,
			50u
		);
		if (state->puzzle_session_solves >= 50u) {
			achievements_unlock(state, ACH_PUZZLE_SESSION_FIFTY);
		}

		if (newly_marked && puzzle) {
			const swap_rule_t rule = puzzle->swap_rule;
			if (rule < NUMBER_OF_SWAP_RULES) {
				if (state->solved_puzzles_per_rule[rule] < UINT16_MAX) {
					++state->solved_puzzles_per_rule[rule];
				}
			}
			if (state->solved_puzzles_catalog < UINT16_MAX) {
				++state->solved_puzzles_catalog;
			}

			achievements_update_rule_progress(state, puzzle->swap_rule);
			achievements_update_catalog_progress(state);
		}
	}

	achievements_reset_puzzle_attempt(state);
}

void achievements_on_puzzle_failed(achievements_state_t *state) {
	achievements_reset_puzzle_attempt(state);
}

void achievements_update_timer(achievements_state_t *state, bool alarm_elapsed) {
	if (!state || !alarm_elapsed) {
		return;
	}

	if (state->puzzle_timer_active) {
		state->puzzle_timer_active = 0u;
		state->puzzle_timer_expired = 1u;
	}
}

static void write_u16(uint8_t **cursor, uint16_t value) {
	(*cursor)[0] = (uint8_t)(value & 0xFFu);
	(*cursor)[1] = (uint8_t)((value >> 8) & 0xFFu);
	*cursor += 2;
}

static uint16_t read_u16(const uint8_t **cursor) {
	uint16_t value = (uint16_t)(*cursor)[0];
	value |= (uint16_t)((*cursor)[1]) << 8;
	*cursor += 2;
	return value;
}

uint16_t achievements_storage_size(void) {
	return (uint16_t)(2u + /* version, reserved */
					  2u + /* unlocked mask */
					  (ACHIEVEMENT_COUNT * 2u) +
					  ACHIEVEMENT_COUNT +
					  2u + 2u + 2u + 2u + 2u +
					  2u + 2u +
					  (NUMBER_OF_SWAP_RULES * 2u) * 2u +
					  1u + 1u + 1u + 1u + 1u + 1u + 1u);
}

uint16_t achievements_serialize(const achievements_state_t *state, uint8_t *buffer, uint16_t max_bytes) {
	if (!state || !buffer) {
		return 0u;
	}

	const uint16_t required = achievements_storage_size();
	if (max_bytes < required) {
		return 0u;
	}

	uint8_t *cursor = buffer;
	cursor[0] = ACHIEVEMENTS_STORAGE_VERSION;
	cursor[1] = 0u;
	cursor += 2;

	write_u16(&cursor, state->unlocked_mask);

	for (uint8_t i = 0u; i < ACHIEVEMENT_COUNT; ++i) {
		write_u16(&cursor, state->progress_count[i]);
	}

	for (uint8_t i = 0u; i < ACHIEVEMENT_COUNT; ++i) {
		cursor[0] = state->detail_bits[i];
		++cursor;
	}

	write_u16(&cursor, state->freeplay_total_wins);
	write_u16(&cursor, state->freeplay_expert_wins);
	write_u16(&cursor, state->puzzle_total_solves);
	write_u16(&cursor, state->puzzle_fast_solves);
	write_u16(&cursor, state->puzzle_no_hint_solves);
	write_u16(&cursor, state->solved_puzzles_catalog);
	write_u16(&cursor, state->total_puzzles_catalog);

	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		write_u16(&cursor, state->solved_puzzles_per_rule[i]);
	}
	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		write_u16(&cursor, state->total_puzzles_per_rule[i]);
	}

	cursor[0] = state->puzzle_session_solves;
	cursor[1] = state->puzzle_timer_active;
	cursor[2] = state->puzzle_timer_expired;
	cursor[3] = state->puzzle_hint_used;
	cursor[4] = state->puzzle_attempt_active;
	cursor[5] = state->puzzle_solution_length;
	cursor[6] = state->puzzle_rule;
	cursor += 7;

	return required;
}

bool achievements_deserialize(achievements_state_t *state, const uint8_t *data, uint16_t length) {
	if (!state || !data) {
		return false;
	}

	const uint16_t required = achievements_storage_size();
	if (length < required) {
		return false;
	}

	achievements_init(state);

	const uint8_t *cursor = data;
	const uint8_t version = cursor[0];
	cursor += 2; // skip version + reserved

	if (version != ACHIEVEMENTS_STORAGE_VERSION) {
		return false;
	}

	state->unlocked_mask = read_u16(&cursor);

	for (uint8_t i = 0u; i < ACHIEVEMENT_COUNT; ++i) {
		state->progress_count[i] = read_u16(&cursor);
	}

	for (uint8_t i = 0u; i < ACHIEVEMENT_COUNT; ++i) {
		state->detail_bits[i] = cursor[0];
		++cursor;
	}

	state->freeplay_total_wins = read_u16(&cursor);
	state->freeplay_expert_wins = read_u16(&cursor);
	state->puzzle_total_solves = read_u16(&cursor);
	state->puzzle_fast_solves = read_u16(&cursor);
	state->puzzle_no_hint_solves = read_u16(&cursor);
	state->solved_puzzles_catalog = read_u16(&cursor);
	state->total_puzzles_catalog = read_u16(&cursor);

	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		state->solved_puzzles_per_rule[i] = read_u16(&cursor);
	}
	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		state->total_puzzles_per_rule[i] = read_u16(&cursor);
	}

	state->puzzle_session_solves = cursor[0];
	state->puzzle_timer_active = cursor[1];
	state->puzzle_timer_expired = cursor[2];
	state->puzzle_hint_used = cursor[3];
	state->puzzle_attempt_active = cursor[4];
	state->puzzle_solution_length = cursor[5];
	state->puzzle_rule = cursor[6];

	achievements_update_catalog_progress(state);
	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		achievements_update_rule_progress(state, (swap_rule_t)i);
	}

	return true;
}
