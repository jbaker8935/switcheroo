#include "file_io.h"
#include "puzzle_data.h"
#include "achievements.h"
#include "game_state.h"
#include "sram_assets.h"
#include <stdint.h>
#include <stdbool.h>
#include "f256lib.h"
#include "overlay_config.h"

// Undefine EOF to allow struct member access for kernelEvent(file.EOF)
#undef EOF

#define PUZZLE_DATA_SIZE PUZZLE_SOLVED_BYTES // 600 puzzles, 1 bit each + padding
#define ACHIEVEMENT_DATA_SIZE 89  // Calculated from achievements_state_t structure size
#define PUZZLE_SIGNATURE_SIZE PUZZLE_CATALOG_SIGNATURE_BYTES
#define LEGACY_DATA_FILE_SIZE (PUZZLE_DATA_SIZE + ACHIEVEMENT_DATA_SIZE)
#define DATA_FILE_SIZE (PUZZLE_SIGNATURE_SIZE + LEGACY_DATA_FILE_SIZE)
#define PUZZLE_FILE_CHUNK_SIZE 255u
#define PUZZLE_CATALOG_MAX_BYTES 64000u

// F256 target system
#include <stdlib.h>
#include <string.h>
#include "platform_f256.h"
#include "text_display.h"

extern game_state_t g_game_state;
extern char g_base_dir[256];

static char *const SAVE_FILE_NAME = "switcheroo.dat";
static char *const PUZZLE_FILE_NAME = "switcheroo.puz";
static char file_name_buffer[256];

static int16_t kernelWriteC(uint8_t fd, void *buf, uint16_t nbytes) {
    kernelArgs->u.file.write.stream = fd;
    kernelArgs->u.common.buf = buf;
    kernelArgs->u.common.buflen = nbytes;
    kernelCall(File.Write);
    if (kernelError) return -1;

    for (;;) {
        kernelNextEvent();
        if (kernelEventData.type == kernelEvent(file.WROTE)) return kernelEventData.u.file.u.data.delivered;
        if (kernelEventData.type == kernelEvent(file.ERROR)) return -1;
    }
}

static int16_t kernelReadC(uint8_t fd, void *buf, uint16_t nbytes) {

	kernelArgs->u.file.read.stream = fd;
	kernelArgs->u.file.read.buflen = nbytes;
	kernelCall(File.Read);
	if (kernelError) return -1;

	for(;;) {
		kernelNextEvent();
		if (kernelEventData.type == kernelEvent(file.DATA)) {
			kernelArgs->u.common.buf = buf;
			kernelArgs->u.common.buflen = kernelEventData.u.file.u.data.delivered;
			kernelCall(ReadData);
			return kernelEventData.u.file.u.data.delivered;
		} else if (kernelEventData.type == kernelEvent(file.EOF)) {
			return 0;
		} else if (kernelEventData.type == kernelEvent(file.ERROR)) {
			return -1;
		}
	}
}

static uint8_t s_buffer[DATA_FILE_SIZE];
static uint8_t s_puzzle_chunk[PUZZLE_FILE_CHUNK_SIZE];

static sig64_t read_sig64(const uint8_t *data) {
    sig64_t s;
    for (uint8_t i = 0u; i < 8u; ++i) {
        s.bytes[i] = data[i];
    }
    return s;
}

static void write_sig64(uint8_t *data, sig64_t s) {
    for (uint8_t i = 0u; i < 8u; ++i) {
        data[i] = s.bytes[i];
    }
}

static bool sig64_eq(sig64_t a, sig64_t b) {
    return memcmp(a.bytes, b.bytes, 8) == 0;
}


static void load_puzzle_catalog_from_file(void) {
    strcpy(file_name_buffer, g_base_dir);
    strcat(file_name_buffer, PUZZLE_FILE_NAME);

    uint8_t *fd = fileOpen(file_name_buffer, "r");
    if (!fd) {
        return;
    }

    puzzle_catalog_invalidate_cache();

    uint32_t offset = 0u;
    bool truncated = false;

    for (;;) {
        int16_t chunk = kernelReadC(*fd, s_puzzle_chunk, PUZZLE_FILE_CHUNK_SIZE);
        if (chunk <= 0) {
            break;
        }

        for (int16_t i = 0; i < chunk; ++i) {
            if (offset >= PUZZLE_CATALOG_MAX_BYTES) {
                truncated = true;
                break;
            }
            FAR_POKE(SRAM_PUZZLE_CATALOG + offset, s_puzzle_chunk[i]);
            ++offset;
        }

        if (truncated || chunk < (int16_t)PUZZLE_FILE_CHUNK_SIZE) {
            break;
        }
    }

    fileClose(fd);
    (void)truncated;
}


#pragma code(ovl12_code)
void FAR_file_io_init(void) {

    load_puzzle_catalog_from_file();

    const sig64_t current_signature = puzzle_catalog_signature();
    bool achievements_loaded = false;
    bool has_saved_signature = false;
    sig64_t saved_signature = {{0}};
    strcpy(file_name_buffer, g_base_dir);
    strcat(file_name_buffer, SAVE_FILE_NAME);
   
    uint8_t *fd = fileOpen(file_name_buffer, "r");
    if (fd) {
        int16_t file_size = kernelReadC(*fd, s_buffer, DATA_FILE_SIZE);

        // Ignore legacy or malformed saves unless they match the expected size
        if (file_size == DATA_FILE_SIZE) {
            has_saved_signature = true;
            saved_signature = read_sig64(s_buffer);

            const uint8_t *puzzle_bytes = s_buffer + PUZZLE_SIGNATURE_SIZE;
            const uint8_t *achievement_bytes = puzzle_bytes + PUZZLE_DATA_SIZE;

            if (sig64_eq(saved_signature, current_signature)) {
                (void)puzzle_catalog_deserialize_solved(puzzle_bytes, PUZZLE_DATA_SIZE);
            } else {
                puzzle_catalog_clear_solved_state();
            }

            if (achievements_deserialize(&g_game_state.achievements,
                                          achievement_bytes,
                                          ACHIEVEMENT_DATA_SIZE)) {
                achievements_loaded = true;
                if (!sig64_eq(saved_signature, current_signature)) {
                    achievements_reset_puzzle_progress(&g_game_state.achievements,
                                                       g_game_state.prefs.swap_rule,
                                                       g_game_state.prefs.current_puzzle_index);
                }
            }
        }

        fileClose(fd);
    }

    set_current_puzzle_swap_rule(g_game_state.prefs.swap_rule);
    const puzzle_collection_t *collection = get_puzzle_collection();
    if (collection && collection->count > 0u &&
        g_game_state.prefs.current_puzzle_index >= collection->count) {
        g_game_state.prefs.current_puzzle_index = 0u;
    }

    if (!achievements_loaded || !has_saved_signature || sig64_eq(saved_signature, current_signature)) {
        achievements_refresh_catalog(&g_game_state.achievements,
                                     g_game_state.prefs.swap_rule,
                                     g_game_state.prefs.current_puzzle_index);
    }
}

void FAR_file_io_save(void) {


    write_sig64(s_buffer, puzzle_catalog_signature());

    size_t actual_puzzle_size = puzzle_catalog_serialize_solved(
        s_buffer + PUZZLE_SIGNATURE_SIZE,
        PUZZLE_DATA_SIZE
    );
    if (actual_puzzle_size != PUZZLE_DATA_SIZE) {
        return;
    }

    uint16_t actual_achievement_size = achievements_serialize(
        &g_game_state.achievements,
        s_buffer + PUZZLE_SIGNATURE_SIZE + PUZZLE_DATA_SIZE,
        ACHIEVEMENT_DATA_SIZE
    );
    if (actual_achievement_size != ACHIEVEMENT_DATA_SIZE) {
        return;
    }
    strcpy(file_name_buffer, g_base_dir);
    strcat(file_name_buffer, SAVE_FILE_NAME);

    uint8_t *fd = fileOpen(file_name_buffer, "w");
    if (fd) {
        int16_t bytes_written = kernelWriteC(*fd, s_buffer, DATA_FILE_SIZE);
        fileClose(fd);
        if (bytes_written != DATA_FILE_SIZE) {
            // Save failed - could retry or handle error
        }
    }
}
#pragma code(code)

void file_io_init(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_12);
    FAR_file_io_init();
    POKE(OVERLAY_MMU_REG, saved);
}

void file_io_save(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_12);
    FAR_file_io_save();
    POKE(OVERLAY_MMU_REG, saved);
}
