#include "../src/file_io.h"
#include "../src/puzzle_data.h"
#include "../src/achievements.h"
#include "../src/game_state.h"
#include "../src/video.h"
#include <stdint.h>
#include <stdbool.h>
#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif

#define PUZZLE_DATA_SIZE PUZZLE_SOLVED_BYTES // 600 puzzles, 1 bit each + padding
#define ACHIEVEMENT_DATA_SIZE 89  // Calculated from achievements_state_t structure size
#define PUZZLE_SIGNATURE_SIZE PUZZLE_CATALOG_SIGNATURE_BYTES
#define LEGACY_DATA_FILE_SIZE (PUZZLE_DATA_SIZE + ACHIEVEMENT_DATA_SIZE)
#define DATA_FILE_SIZE (PUZZLE_SIGNATURE_SIZE + LEGACY_DATA_FILE_SIZE)
#define PUZZLE_FILE_CHUNK_SIZE 255u
#define PUZZLE_CATALOG_MAX_BYTES 65536u

#if defined(AI_AGENT_HOST_TEST)
// Host test stubs - no file I/O
void file_io_init(void) {
    // Stub for host testing
}

void file_io_save(void) {
    // Stub for host testing
}
#else
// F256 target system - 
#include <stdlib.h>
#include <string.h>
#include "../src/platform_f256.h"
#include "../src/text_display.h"

extern game_state_t g_game_state;

static char *const SAVE_FILE_NAME = "switcheroo.dat";
static char *const PUZZLE_FILE_NAME = "switcheroo.puz";

#if defined(__llvm_mos__)
__attribute__((noinline, section(".block12")))
static int16_t kernelWriteC(uint8_t fd, void *buf, uint16_t nbytes) {
    kernelArgs->file.write.stream = fd;
    kernelArgs->common.buf = buf;
    kernelArgs->common.buflen = nbytes;
    kernelCall(File.Write);
    if (kernelError) return -1;

    for (;;) {
        kernelNextEvent();
        if (kernelEventData.type == kernelEvent(file.WROTE)) return kernelEventData.file.data.delivered;
        if (kernelEventData.type == kernelEvent(file.ERROR)) return -1;
    }
}

#pragma push_macro("EOF")
#undef EOF
__attribute__((noinline, section(".block12")))
static int16_t kernelReadC(uint8_t fd, void *buf, uint16_t nbytes) {

	kernelArgs->file.read.stream = fd;
	kernelArgs->file.read.buflen = nbytes;
	kernelCall(File.Read);
	if (kernelError) return -1;

	for(;;) {
		kernelNextEvent();
		switch (kernelEventData.type) {
			case kernelEvent(file.DATA):
				kernelArgs->common.buf = buf;
				kernelArgs->common.buflen = kernelEventData.file.data.delivered;
				kernelCall(ReadData);
				return kernelEventData.file.data.delivered;
			case kernelEvent(file.EOF):
				return 0;
			case kernelEvent(file.ERROR):
				return -1;
			default:
				continue;
		}
	}
}
#pragma pop_macro("EOF")


#else
static int16_t kernelWriteC(uint8_t fd, void *buf, uint16_t nbytes) {
    (void)fd;
    (void)buf;
    (void)nbytes;
    return -1;
}
static int16_t kernelReadC(uint8_t fd, void *buf, uint16_t nbytes) {
    (void)fd;
    (void)buf;
    (void)nbytes;
    return -1;
}
#endif

static uint8_t s_buffer[DATA_FILE_SIZE];
static uint8_t s_puzzle_chunk[PUZZLE_FILE_CHUNK_SIZE];

__attribute__((noinline, section(".block12")))
static uint64_t read_le64(const uint8_t *data) {
    uint64_t value = 0u;
    for (uint8_t i = 0u; i < 8u; ++i) {
        value |= ((uint64_t)data[i] << (uint64_t)(i * 8u));
    }
    return value;
}

__attribute__((noinline, section(".block12")))
static void write_le64(uint8_t *data, uint64_t value) {
    for (uint8_t i = 0u; i < 8u; ++i) {
        data[i] = (uint8_t)((value >> (uint64_t)(i * 8u)) & 0xFFu);
    }
}


__attribute__((noinline, section(".block12")))
static void load_puzzle_catalog_from_file(void) {
    uint8_t *fd = fileOpen(PUZZLE_FILE_NAME, "r");
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


void FAR12_file_io_init(void);

#pragma clang optimize off
__attribute__((noinline))
void file_io_init(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 12;
    FAR12_file_io_init();
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on
__attribute__((noinline, section(".block12")))
void FAR12_file_io_init(void) {

    load_puzzle_catalog_from_file();

    const uint64_t current_signature = puzzle_catalog_signature();
    bool achievements_loaded = false;
    bool has_saved_signature = false;
    uint64_t saved_signature = 0u;

    uint8_t *fd = fileOpen(SAVE_FILE_NAME, "r");
    if (fd) {
        int16_t file_size = kernelReadC(*fd, s_buffer, DATA_FILE_SIZE);

        // Ignore legacy or malformed saves unless they match the expected size
        if (file_size == DATA_FILE_SIZE) {
            has_saved_signature = true;
            saved_signature = read_le64(s_buffer);

            const uint8_t *puzzle_bytes = s_buffer + PUZZLE_SIGNATURE_SIZE;
            const uint8_t *achievement_bytes = puzzle_bytes + PUZZLE_DATA_SIZE;

            if (saved_signature == current_signature) {
                (void)puzzle_catalog_deserialize_solved(puzzle_bytes, PUZZLE_DATA_SIZE);
            } else {
                puzzle_catalog_clear_solved_state();
            }

            if (achievements_deserialize(&g_game_state.achievements,
                                          achievement_bytes,
                                          ACHIEVEMENT_DATA_SIZE)) {
                achievements_loaded = true;
                if (saved_signature != current_signature) {
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

    if (!achievements_loaded || !has_saved_signature || saved_signature == current_signature) {
        achievements_refresh_catalog(&g_game_state.achievements,
                                     g_game_state.prefs.swap_rule,
                                     g_game_state.prefs.current_puzzle_index);
    }
}

void FAR12_file_io_save(void);

#pragma clang optimize off
__attribute__((noinline))
void file_io_save(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 12;
    FAR12_file_io_save();
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on
__attribute__((noinline, section(".block12")))
void FAR12_file_io_save(void) {


    write_le64(s_buffer, puzzle_catalog_signature());

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

    uint8_t *fd = fileOpen(SAVE_FILE_NAME, "w");
    if (fd) {
        int16_t bytes_written = kernelWriteC(*fd, s_buffer, DATA_FILE_SIZE);
        fileClose(fd);
        if (bytes_written != DATA_FILE_SIZE) {
            // Save failed - could retry or handle error
        }
    }
}
#endif
