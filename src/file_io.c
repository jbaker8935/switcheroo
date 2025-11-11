#include "../src/file_io.h"
#include "../src/puzzle_data.h"
#include "../src/achievements.h"
#include "../src/game_state.h"
#include <stdint.h>
#include <stdbool.h>
#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif

#define PUZZLE_DATA_SIZE PUZZLE_SOLVED_BYTES // 600 puzzles, 1 bit each + padding
#define ACHIEVEMENT_DATA_SIZE 89  // Calculated from achievements_state_t structure size
#define DATA_FILE_SIZE (PUZZLE_DATA_SIZE + ACHIEVEMENT_DATA_SIZE)

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

const char SAVE_FILE_NAME[] = "f256swdata";
char * save_file_name = (char *) SAVE_FILE_NAME;

#if defined(__llvm_mos__)
static int16_t kernelWrite(uint8_t fd, void *buf, uint16_t nbytes) {
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
static int16_t kernelRead(uint8_t fd, void *buf, uint16_t nbytes) {

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
				if (!kernelEventData.file.data.delivered) return 256;
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
static int16_t kernelWrite(uint8_t fd, void *buf, uint16_t nbytes) {
    (void)fd;
    (void)buf;
    (void)nbytes;
    return -1;
}
static int16_t kernelRead(uint8_t fd, void *buf, uint16_t nbytes) {
    (void)fd;
    (void)buf;
    (void)nbytes;
    return -1;
}
#endif

static uint8_t s_buffer[DATA_FILE_SIZE];

void FAR8_file_io_init(void);

#pragma clang optimize off
__attribute__((noinline))
void file_io_init(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_file_io_init();
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on
__attribute__((noinline, section(".block8")))

void FAR8_file_io_init(void) {


    uint8_t *fd = fileOpen(save_file_name, "r");
    if (fd) {

        int16_t file_size = kernelRead(*fd, s_buffer, DATA_FILE_SIZE);
        
        if (file_size == DATA_FILE_SIZE) {

            // Deserialize puzzle data first
            uint8_t puzzle_result = puzzle_catalog_deserialize_solved(s_buffer, PUZZLE_DATA_SIZE);

            if (puzzle_result != 0) {
                // Deserialize achievement data
                bool achievement_result = achievements_deserialize(&g_game_state.achievements, s_buffer + PUZZLE_DATA_SIZE, ACHIEVEMENT_DATA_SIZE);
                (void)achievement_result; // Ignore for now
            } 
        } 
        fileClose(fd);
    } 
}

void FAR8_file_io_save(void);

#pragma clang optimize off
__attribute__((noinline))
void file_io_save(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 8;
    FAR8_file_io_save();
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on
__attribute__((noinline, section(".block8")))

void FAR8_file_io_save(void) {


    // Serialize puzzle data
    size_t actual_puzzle_size = puzzle_catalog_serialize_solved(s_buffer, PUZZLE_DATA_SIZE);
    if (actual_puzzle_size == PUZZLE_DATA_SIZE) {
        // Serialize achievement data
        uint16_t actual_achievement_size = achievements_serialize(&g_game_state.achievements, s_buffer + PUZZLE_DATA_SIZE, ACHIEVEMENT_DATA_SIZE);
        if (actual_achievement_size == ACHIEVEMENT_DATA_SIZE) {
            // Write to file
            uint8_t *fd = fileOpen(save_file_name, "w");
            if (fd) {
                int16_t bytes_written = kernelWrite(*fd, s_buffer, DATA_FILE_SIZE);
                fileClose(fd);
                // Check if all bytes were written
                if (bytes_written != DATA_FILE_SIZE) {
                    // Save failed - could retry or handle error
                    // For now, just continue
                }
            }
            // fileUnlink(save_file_name);
            // fileRename("f256_tmp_dat", save_file_name);
        }

    }
}
#endif
