#include "../src/achievements.h"
#include "../src/puzzle_data.h"
#include "../src/timer.h"
#include "../src/video.h"
#include "../src/mouse_pointer.h"
#include "../src/text_display.h"
#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif
#include <string.h>

enum {
	ACHIEVEMENTS_STORAGE_VERSION = 1u
};

const uint32_t s_video_achievement_vram_addrs[ACHIEVEMENT_COUNT] = {
	SRAM_ACHIEVE_MEDAL,
	SRAM_ACHIEVE_FLAME,
	SRAM_ACHIEVE_DICE,
	SRAM_ACHIEVE_ARM_FLEX,
	SRAM_ACHIEVE_SWORD,
	SRAM_ACHIEVE_CROWN,
	SRAM_ACHIEVE_LIGHTNING,
	SRAM_ACHIEVE_100,
	SRAM_ACHIEVE_BULLSEYE,
	SRAM_ACHIEVE_LIGHTNING,
	SRAM_ACHIEVE_THINKER,
	SRAM_ACHIEVE_BRAIN,
	SRAM_ACHIEVE_PUZZLE,
	SRAM_ACHIEVE_RUNNER,
	SRAM_ACHIEVE_100,
	SRAM_ACHIEVE_AWARD
};

const char* s_achievement_names[ACHIEVEMENT_COUNT] = {
	"You Win",
	"Untouchable",
	"Versatile",
	"Unbreakable",
	"Slayer",
	"Master Slayer",
	"Speedster",
	"Century Club",
	"First Blood",
	"Speed Demon",
	"Thinker",
	"Deep Thinker",
	"Puzzlemeister",
	"Runner",
	"Perfectionist",
	"Completionist"
};

const char* s_achievement_descriptions[ACHIEVEMENT_COUNT] = {
	"Win first game",
	"Win 10|in a row",
	"Win all|starting|boards",
	"Win all|game rules",
	"Win against|Expert",
	"Win 10 against|Expert",
	"Win under 10|moves",
	"Win 100|games",
	"Solve|first puzzle|no hints",
	"Solve 10|in 30s each|no hints",
	"Solve Win in 3|no hints",
	"Solve Win in 4|no hints",
	"Solve 25|no hints",
	"Solve 50|in one session",
	"Solve all|for a rule",
	"Solve all|puzzles"
};

const uint8_t s_achievement_char_x[ACHIEVEMENT_COUNT] = {
	3, 22, 42, 61,
	3, 22, 42, 61,
	3, 22, 42, 61,
	3, 22, 42, 61
};
const uint8_t s_achievement_char_y[ACHIEVEMENT_COUNT] = {
	9, 9, 9, 9,
	34, 34, 34, 34,
	9, 9, 9, 9,
	34, 34, 34, 34
};
const uint8_t center_offset_x = 7;
const uint8_t title_offset_y = 10;
const uint8_t desc_offset_y = 14;
const uint8_t progress_offset_y = 19;
const uint8_t icon_offset_x_px = 20;
const uint8_t icon_offset_y_px = 8;


static uint16_t clamp_u16(uint16_t value, uint16_t limit) {
	return (value > limit) ? limit : value;
}



static uint8_t count_bits8(uint8_t value) {
	uint8_t count = 0u;
	while (value != 0u) {
		value &= (uint8_t)(value - 1u);
		++count;
	}
	return count;
}

__attribute__((noinline, section(".block8")))
char* uint16_to_str(uint16_t value, char* buffer) {
    char temp[6]; // enough for 16-bit unsigned int (max 5 digits + '\0')
    uint8_t i = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }
    while (value > 0) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }
    for (uint8_t j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[i] = '\0';

    return buffer;
}
__attribute__((noinline, section(".block8")))
char * progress_str(uint16_t progress, uint16_t total) {
	static char progress_buffer[10]; // enough for "XXX / XXX"
	
	uint8_t p_length = count_digits(progress);
	uint16_to_str(progress, progress_buffer);
	progress_buffer[p_length] = ' ';
	progress_buffer[p_length + 1] = '/';
	progress_buffer[p_length + 2] = ' ';
	uint16_to_str(total, &progress_buffer[p_length + 3]);
	return progress_buffer;
}



// void numbytestohex(uint8_t byte_count, const uint8_t *data, char *out_hex) {
// 	const char hex_chars[] = "0123456789ABCDEF";
// 	for (uint8_t i = 0u; i < byte_count; ++i) {
// 		const uint8_t byte = data[i];
// 		out_hex[i * 2u] = hex_chars[(byte >> 4u) & 0x0Fu];
// 		out_hex[i * 2u + 1u] = hex_chars[byte & 0x0Fu];
// 	}
// 	out_hex[byte_count * 2u] = '\0';
// }

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

// ACH_PUZZLE_RULE_COMPLETE counts progress per swap rule, display logic assumes
// ALL rules have the same number of puzzles
// CODE would need to be changed to handle differing counts per rule

static void achievements_update_rule_progress(achievements_state_t *state, swap_rule_t rule) {
	if (!state || rule >= NUMBER_OF_SWAP_RULES) {
		return;
	}

	if (achievements_is_unlocked(state, ACH_PUZZLE_RULE_COMPLETE)) {
		return;
	}

	uint16_t max_solved = 0u;
	bool any_rule_complete = false;

	for (uint8_t r = 0u; r < NUMBER_OF_SWAP_RULES; ++r) {
		const uint16_t solved = state->solved_puzzles_per_rule[r];
		const uint16_t total = state->total_puzzles_per_rule[r];

		if (solved > max_solved) {
			max_solved = solved;
		}

		if (total > 0u && solved >= total) {
			state->detail_bits[ACH_PUZZLE_RULE_COMPLETE] |= (uint8_t)(1u << r);
			any_rule_complete = true;
		} else {
			state->detail_bits[ACH_PUZZLE_RULE_COMPLETE] &= (uint8_t)~(1u << r);
		}
	}

	state->progress_count[ACH_PUZZLE_RULE_COMPLETE] = max_solved;

	if (any_rule_complete) {
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

static void achievements_backfill_totals_if_missing(achievements_state_t *state) {
	if (!state) {
		return;
	}

	bool any_rule_missing = false;
	for (uint8_t r = 0u; r < NUMBER_OF_SWAP_RULES; ++r) {
		if (state->total_puzzles_per_rule[r] == 0u) {
			any_rule_missing = true;
			break;
		}
	}

	bool catalog_missing = (state->total_puzzles_catalog == 0u);
	if (!catalog_missing && !any_rule_missing) {
		return;
	}

	const swap_rule_t original_rule = get_current_puzzle_swap_rule();
	state->total_puzzles_catalog = 0u;

	for (uint8_t rule = 0u; rule < NUMBER_OF_SWAP_RULES; ++rule) {
		const swap_rule_t loop_rule = (swap_rule_t)rule;
		set_current_puzzle_swap_rule(loop_rule);

		uint16_t count = 0u;
		const puzzle_collection_t *collection = get_puzzle_collection();
		if (collection && collection->count <= UINT16_MAX) {
			count = (uint16_t)collection->count;
		}

		state->total_puzzles_per_rule[rule] = count;
		state->total_puzzles_catalog = (uint16_t)(state->total_puzzles_catalog + count);
	}

	set_current_puzzle_swap_rule(original_rule);

	achievements_update_catalog_progress(state);
	for (uint8_t rule = 0u; rule < NUMBER_OF_SWAP_RULES; ++rule) {
		achievements_update_rule_progress(state, (swap_rule_t)rule);
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


void FAR8_achievements_on_freeplay_win(achievements_state_t *state,
								  uint8_t layout_id,
								  swap_rule_t rule,
								  ai_difficulty_t difficulty,
								  uint8_t move_count);

#pragma clang optimize off
__attribute__((noinline))
void achievements_on_freeplay_win(achievements_state_t *state,
								  uint8_t layout_id,
								  swap_rule_t rule,
								  ai_difficulty_t difficulty,
								  uint8_t move_count) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_achievements_on_freeplay_win(state, layout_id, rule, difficulty, move_count);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

__attribute__((noinline, section(".block8")))

void FAR8_achievements_on_freeplay_win(achievements_state_t *state,
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

	if (state->progress_count[ACH_FREEPLAY_FIRST_WIN] >= 1u) {
		achievements_unlock(state, ACH_FREEPLAY_FIRST_WIN);
	}
	if (state->progress_count[ACH_FREEPLAY_TEN_WINS] >= 10u) {
		achievements_unlock(state, ACH_FREEPLAY_TEN_WINS);
	}
	if (state->progress_count[ACH_FREEPLAY_HUNDRED_WINS] >= 100u) {
		achievements_unlock(state, ACH_FREEPLAY_HUNDRED_WINS);
	}

	state->detail_bits[ACH_FREEPLAY_ALL_LAYOUTS] |= (uint8_t)(1u << layout_id);
	state->progress_count[ACH_FREEPLAY_ALL_LAYOUTS] = count_bits8(
		state->detail_bits[ACH_FREEPLAY_ALL_LAYOUTS]
	);
	if (state->progress_count[ACH_FREEPLAY_ALL_LAYOUTS] >= NUM_STARTING_LAYOUTS) {
		achievements_unlock(state, ACH_FREEPLAY_ALL_LAYOUTS);
	}


	state->detail_bits[ACH_FREEPLAY_ALL_SWAP_RULES] |= (uint8_t)(1u << rule);
	state->progress_count[ACH_FREEPLAY_ALL_SWAP_RULES] = count_bits8(
		state->detail_bits[ACH_FREEPLAY_ALL_SWAP_RULES]
	);
	if (state->progress_count[ACH_FREEPLAY_ALL_SWAP_RULES] >= NUMBER_OF_SWAP_RULES) {
		achievements_unlock(state, ACH_FREEPLAY_ALL_SWAP_RULES);
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


void FAR8_achievements_on_puzzle_attempt_completed(achievements_state_t *state,
											  const struct puzzle_t *puzzle,
											  bool qualifies_for_mark);

#pragma clang optimize off
__attribute__((noinline))
void achievements_on_puzzle_attempt_completed(achievements_state_t *state,
											  const struct puzzle_t *puzzle,
											  bool qualifies_for_mark) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_achievements_on_puzzle_attempt_completed(state, puzzle, qualifies_for_mark);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

__attribute__((noinline, section(".block8")))

void FAR8_achievements_on_puzzle_attempt_completed(achievements_state_t *state,
											  const struct puzzle_t *puzzle,
											  bool qualifies_for_mark) {
	if (!state) {
		return;
	}

	uint8_t solved_fast = (state->puzzle_timer_expired == 0u) ? 1u : 0u;
	uint8_t no_hint = (state->puzzle_hint_used == 0u) ? 1u : 0u;

	if (qualifies_for_mark) {  // only if the puzzle was solved within allowed moves
		if (state->puzzle_total_solves < UINT16_MAX) {
			++state->puzzle_total_solves;
		}
		
		if (no_hint && solved_fast) {
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
			
			state->progress_count[ACH_PUZZLE_FIRST_SOLVE] = clamp_u16(state->puzzle_no_hint_solves, 1u);
			achievements_unlock(state, ACH_PUZZLE_FIRST_SOLVE);

			state->progress_count[ACH_PUZZLE_NO_HINT_TWENTY_FIVE] = clamp_u16(
				state->puzzle_no_hint_solves,
				25u
			);
			if (state->puzzle_no_hint_solves >= 25u) {
				achievements_unlock(state, ACH_PUZZLE_NO_HINT_TWENTY_FIVE);
			}

			if (puzzle) {
				if (puzzle->difficulty == 3u) {
					achievements_unlock(state, ACH_PUZZLE_WIN_IN_THREE);
					state->progress_count[ACH_PUZZLE_WIN_IN_THREE] = 1u;
				} else if (puzzle->difficulty == 4u) {
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

	achievements_backfill_totals_if_missing(state);

	achievements_update_catalog_progress(state);
	for (uint8_t i = 0u; i < NUMBER_OF_SWAP_RULES; ++i) {
		achievements_update_rule_progress(state, (swap_rule_t)i);
	}

	return true;
}


void FAR8_display_achievements_screen(achievements_state_t *state, uint8_t page);

#pragma clang optimize off
__attribute__((noinline))
void display_achievements_screen(achievements_state_t *state, uint8_t page) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_display_achievements_screen(state, page);
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

__attribute__((noinline, section(".block8")))
void FAR8_display_achievements_screen(achievements_state_t *state, uint8_t page){

	uint8_t first = page == 0u ? 0u : 8u;
	uint8_t last = page == 0u ? 8u : ACHIEVEMENT_COUNT;
	const char* page_footer = page == 0u ? "Free Play Achievements 1/2" : "Puzzle Achievements 2/2";
	const char* page_instruction = "Press [A] to switch pages, [SPACE] to exit";
	// Hide All Sprites
	spriteReset();

	clear_text_matrix();
	// Update CLUTs 1,2,3

	POKE(MMU_IO_CTRL, 1);
    for (uint16_t i = 0; i < 1024; ++i) {
        uint8_t color_component = FAR_PEEK(SRAM_ACHIEVE_BASE_PALETTE + i);
        POKE(0xD400 + i, color_component);
		color_component = FAR_PEEK(SRAM_ACHIEVE_COLOR_PALETTE + i);
		POKE(0xD800 + i, color_component);
		color_component = FAR_PEEK(SRAM_ACHIEVE_GREY_PALETTE + i);
		POKE(0xDC00 + i, color_component);		
    }
    POKE(MMU_IO_CTRL, 0);
	// Reusing board bitmap page/layer 2 for achievements screen
	graphicsSetLayerBitmap(VIDEO_ACHIEVEMENT_PAGE, 2);
	bitmapSetActive(VIDEO_ACHIEVEMENT_PAGE);
	bitmapSetCLUT(VIDEO_ACHIEVEMENT_BASE_CLUT);
	bitmapSetAddress(VIDEO_ACHIEVEMENT_PAGE, SRAM_ACHIEVEMENT_BASE);
	bitmapSetVisible(VIDEO_ACHIEVEMENT_PAGE, true);



	for (uint8_t i=first, j=0; i < last; ++i, ++j) {
		uint16_t icon_x = s_achievement_char_x[i] * 4 + icon_offset_x_px;
		uint8_t sprite_id = (VIDEO_SPRITE_PIECE_BASE + i);
		uint16_t icon_y = s_achievement_char_y[i] * 4 + icon_offset_y_px;
		bool is_unlocked = achievements_is_unlocked(state, (achievement_id_t) i );
		spriteDefine(sprite_id, s_video_achievement_vram_addrs[i], VIDEO_ACHIEVEMENT_SPRITE_SIZE, is_unlocked ? VIDEO_ACHIEVEMENT_CLUT_COLOR : VIDEO_ACHIEVEMENT_CLUT_GREY, VIDEO_SPRITE_PIECE_LAYER);
		spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + icon_x, VIDEO_SPRITE_OFFSET + icon_y);
		spriteSetVisible(sprite_id, 1);	
		
		const char* title = s_achievement_names[i];
		const char* description = s_achievement_descriptions[i];
		uint8_t title_x = s_achievement_char_x[i] + center_offset_x;
		uint8_t title_y = s_achievement_char_y[i] + title_offset_y;
		uint8_t desc_x = s_achievement_char_x[i] + center_offset_x;
		uint8_t desc_y = s_achievement_char_y[i] + desc_offset_y;
		uint8_t progress_y = s_achievement_char_y[i] + progress_offset_y;
		char desc_buffer[32];
		if(is_unlocked) {
			textSetColor(4,1); // blue text for unlocked
		} else {
			textSetColor(1,1); // normal text for locked
		}
		print_formatted_text(title_x-strlen(title)/2, title_y, title);
		// parse description for '|' line breaks
		strcpy(desc_buffer, description);
		char* line = strtok(desc_buffer, "|");
		if(is_unlocked) {
			textSetColor(5,1); // purple text for unlocked
		} else {
			textSetColor(1,1); // normal text for locked
		}
		while (line != NULL) {
			const uint8_t len = strlen(line);
			const uint8_t even = len % 2 == 0 ? 1 : 0;
			print_formatted_text(desc_x - len / 2 + even, desc_y, line);
			desc_y += 1; // move down for next line
			line = strtok(NULL, "|");
		}

		// Progress display for certain achievements
		switch(i) {
			case ACH_FREEPLAY_TEN_WINS:
			case ACH_FREEPLAY_EXPERT_TEN_WINS:
			case ACH_PUZZLE_FAST_TEN:			
			{
				char progress_text[]=" 0 / 10";
				uint16_t progress = state->progress_count[i];

				if (progress >= 10u) {
					progress_text[0] = (char)('0' + (progress / 10u));
					progress_text[1] = (char)('0' + (progress % 10u));
				} else {
					progress_text[0] = ' ';
					progress_text[1] = (char)('0' + (progress));
				}
				print_formatted_text(desc_x - strlen(progress_text)/2, progress_y, progress_text);
				break;
			}
			case ACH_FREEPLAY_HUNDRED_WINS:
			{
				char progress_text[]="  0 / 100";
				uint16_t progress = state->progress_count[i];
				if (progress >= 100u) {
					progress_text[0] = (char)('0' + (progress / 100u));
					progress_text[1] = (char)('0' + ((progress / 10u) % 10u));
					progress_text[2] = (char)('0' + (progress % 10u));
				} else if (progress >= 10u) {
					progress_text[0] = ' ';
					progress_text[1] = (char)('0' + (progress / 10u));
					progress_text[2] = (char)('0' + (progress % 10u));
				} else {
					progress_text[0] = ' ';
					progress_text[1] = ' ';
					progress_text[2] = (char)('0' + (progress));
				}
				print_formatted_text(desc_x - strlen(progress_text)/2, progress_y, progress_text);
				break;
			}

			case ACH_FREEPLAY_ALL_LAYOUTS:
			case ACH_FREEPLAY_ALL_SWAP_RULES:
			{
				char progress_text[]="0 / 0";
				uint8_t progress = state->progress_count[i];
				uint8_t total = (i == ACH_FREEPLAY_ALL_LAYOUTS) ? NUM_STARTING_LAYOUTS : NUMBER_OF_SWAP_RULES;
				progress_text[0] = (char)('0' + (progress));
				progress_text[4] = (char)('0' + (total));
				print_formatted_text(desc_x - strlen(progress_text)/2, progress_y, progress_text);
				break;
			}

			case ACH_PUZZLE_NO_HINT_TWENTY_FIVE:
			{
				char progress_text[]="00 / 25";
				uint8_t progress = (uint8_t)state->progress_count[i];
				if (progress >= 10u) {
					progress_text[0] = (char)('0' + (progress / 10u));
					progress_text[1] = (char)('0' + (progress % 10u));
				} else {
					progress_text[0] = ' ';
					progress_text[1] = (char)('0' + (progress));
				}
				print_formatted_text(desc_x - strlen(progress_text)/2, progress_y, progress_text);
				break;
			}

			case ACH_PUZZLE_SESSION_FIFTY:
			{
				char progress_text[]="00 / 50";
				uint8_t progress = (uint8_t)state->progress_count[i];
				if (progress >= 10u) {
					progress_text[0] = (char)('0' + (progress / 10u));
					progress_text[1] = (char)('0' + (progress % 10u));
				} else {
					progress_text[0] = ' ';
					progress_text[1] = (char)('0' + (progress));
				}
				print_formatted_text(desc_x - strlen(progress_text)/2, progress_y, progress_text);
				break;
			}

			case ACH_PUZZLE_RULE_COMPLETE:
			{
				uint16_t progress = state->progress_count[i];
				uint16_t total = state->total_puzzles_per_rule[0]; // all rules assumed to have same total
				char *progress_text = progress_str(progress, total);
				uint8_t pt_len = strlen(progress_text);
				
				print_formatted_text(desc_x - pt_len/2 + ((pt_len & 0x01) == 0 ? 0 : 1), progress_y, progress_text);
				break;
			}
			case ACH_PUZZLE_CATALOG_COMPLETE:
			{
				uint16_t progress = state->progress_count[i];
				uint16_t total = state->total_puzzles_catalog;
				char *progress_text = progress_str(progress, total);
				uint8_t pt_len = strlen(progress_text);		
				print_formatted_text(desc_x - pt_len/2 + ((pt_len & 0x01) == 0 ? 0 : 1), progress_y, progress_text);
				break;
			}
			default:
				// no progress display
				break;
		}

	}

	print_formatted_text(3, 57, page_footer);
	print_formatted_text(3, 58, page_instruction);

}

void hide_achievements_screen(void) {
	// Hide All Sprites
	spriteReset();
	clear_text_matrix();
	// Hide bitmap layer
	bitmapSetVisible(VIDEO_ACHIEVEMENT_PAGE, false);
}
