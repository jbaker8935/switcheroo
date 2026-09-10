#ifndef OVERLAY_CONFIG_H
#define OVERLAY_CONFIG_H

#include "f256lib.h"

#define OVERLAY_MMU_REG  MMU_MEM_BANK_5

#define BLOCK_8   8
#define BLOCK_9   9
#define BLOCK_10 10
#define BLOCK_11 11
#define BLOCK_12 12
#define BLOCK_13 13
#define BLOCK_14 14
#define BLOCK_15 15

// Block 8: achievements
#pragma section(ovl8_code, 0)
#pragma region(ovl8, 0x10000, 0x12000, , , { ovl8_code }, 0xA000)

// Block 9: AI evaluation
#pragma section(ovl9_code, 0)
#pragma region(ovl9, 0x12000, 0x14000, , , { ovl9_code }, 0xA000)

// Block 10: AI move generation
#pragma section(ovl10_code, 0)
#pragma region(ovl10, 0x14000, 0x16000, , , { ovl10_code }, 0xA000)

// Block 11: AI evaluate_moves
#pragma section(ovl11_code, 0)
#pragma region(ovl11, 0x16000, 0x18000, , , { ovl11_code }, 0xA000)

// Block 12: file_io + game_state
#pragma section(ovl12_code, 0)
#pragma region(ovl12, 0x18000, 0x1A000, , , { ovl12_code }, 0xA000)

// Block 13: puzzle_data + exit_screen + render + video
#pragma section(ovl13_code, 0)
#pragma region(ovl13, 0x1A000, 0x1C000, , , { ovl13_code }, 0xA000)

// Block 14: achievements_screen
#pragma section(ovl14_code, 0)
#pragma region(ovl14, 0x1C000, 0x1E000, , , { ovl14_code }, 0xA000)

// Block 15: main_loop
#pragma section(ovl15_code, 0)
#pragma region(ovl15, 0x1E000, 0x20000, , , { ovl15_code }, 0xA000)

#endif
