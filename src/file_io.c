#include "../src/file_io.h"
#include "../src/puzzle_data.h"
#include "../src/achievements.h"
#include "../src/game_state.h"
#include <stdint.h>
#include <stdbool.h>

#define PUZZLE_DATA_SIZE (600+1)/8 // 600 puzzles, 1 bit each + padding
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

extern game_state_t g_game_state;

#define SAVE_FILE_NAME "f256_switch.dat"

// void FAR9_ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out);

// #if defined(AI_AGENT_HOST_TEST)

// void ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out) {
//     FAR9_ai_compute_connection_metrics(board, player, out);
// }

// #else

// #pragma clang optimize off
// __attribute__((noinline))

// void ai_compute_connection_metrics(const board_t *board, player_t player, ai_connection_metrics_t *out) {
//     volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
//     *(volatile unsigned char *)0x000d = 9;
//     FAR9_ai_compute_connection_metrics(board, player, out);
//     *(volatile unsigned char *)0x000d = ___mmu;
// }
// #pragma clang optimize on

// #endif

// __attribute__((noinline, section(".block9"))) void FAR9_ai_compute_connection_metrics(const board_t *board,


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
    uint8_t *fd = fileOpen(SAVE_FILE_NAME, "r");
    if (fd) {
        uint8_t buffer[DATA_FILE_SIZE];
        int16_t file_size = fileRead(buffer, 1, DATA_FILE_SIZE, fd);
        if (file_size == DATA_FILE_SIZE) {


            // Deserialize puzzle data first
            uint8_t puzzle_result = puzzle_catalog_deserialize_solved(buffer, PUZZLE_DATA_SIZE);
            if (puzzle_result != 0) {
                // Deserialize achievement data
                bool achievement_result = achievements_deserialize(&g_game_state.achievements, buffer + PUZZLE_DATA_SIZE, ACHIEVEMENT_DATA_SIZE);
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
    uint8_t buffer[DATA_FILE_SIZE];

    // Serialize puzzle data
    size_t actual_puzzle_size = puzzle_catalog_serialize_solved(buffer, PUZZLE_DATA_SIZE);
    if (actual_puzzle_size == PUZZLE_DATA_SIZE) {
        // Serialize achievement data
        uint16_t actual_achievement_size = achievements_serialize(&g_game_state.achievements, buffer + PUZZLE_DATA_SIZE, ACHIEVEMENT_DATA_SIZE);
        if (actual_achievement_size == ACHIEVEMENT_DATA_SIZE) {
            // Write to file
            uint8_t *fd = fileOpen(SAVE_FILE_NAME, "w");
            if (fd) {
                fileWrite(buffer, 1, DATA_FILE_SIZE, fd);
                fileClose(fd);
            }
        }

    }
}
#endif
