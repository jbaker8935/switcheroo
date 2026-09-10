#include "f256lib.h"
#include "achievements.h"
#include "sram_assets.h"
#include "video.h"
#include "text_display.h"
#include <string.h>
#include "overlay_config.h"

static char *s_strtok_next = NULL;
static char *local_strtok(char *str, const char *delim) {
	char *start = str ? str : s_strtok_next;
	if (!start) return NULL;
	while (*start && strchr(delim, *start)) ++start;
	if (!*start) { s_strtok_next = NULL; return NULL; }
	char *end = start;
	while (*end && !strchr(delim, *end)) ++end;
	if (*end) { *end = '\0'; s_strtok_next = end + 1; }
	else { s_strtok_next = NULL; }
	return start;
}

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

static char* uint16_to_str(uint16_t value, char* buffer) {
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
static char * progress_str(uint16_t progress, uint16_t total) {
	static char progress_buffer[10]; // enough for "XXX / XXX"
	
	uint8_t p_length = count_digits(progress);
	uint16_to_str(progress, progress_buffer);
	progress_buffer[p_length] = ' ';
	progress_buffer[p_length + 1] = '/';
	progress_buffer[p_length + 2] = ' ';
	uint16_to_str(total, &progress_buffer[p_length + 3]);
	return progress_buffer;
}



#pragma code(ovl14_code)
void FAR_display_achievements_screen(achievements_state_t *state, uint8_t page){

	uint8_t first = page == 0u ? 0u : 8u;
	uint8_t last = page == 0u ? 8u : ACHIEVEMENT_COUNT;
	const char* page_footer = page == 0u ? "Free Play Achievements 1/2" : "Puzzle Achievements 2/2";
	const char* page_instruction = "Press [^6A^1] to switch pages, [^6SPACE^1] to exit";
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
		char* line = local_strtok(desc_buffer, "|");
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
			line = local_strtok(NULL, "|");
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
#pragma code(code)

void display_achievements_screen(achievements_state_t *state, uint8_t page) {
	volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
	POKE(OVERLAY_MMU_REG, BLOCK_14);
	FAR_display_achievements_screen(state, page);
	POKE(OVERLAY_MMU_REG, saved);
}
