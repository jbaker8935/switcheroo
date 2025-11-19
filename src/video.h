/**
 * @file video.h
 * @brief Video system constants and definitions
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include "../src/sram_assets.h"

// Constants
#define VIDEO_BOARD_CLUT 0
#define VIDEO_PIECES_CLUT 1
#define VIDEO_MENU_CLUT 2
#define VIDEO_BITMAP_PAGE 2
#define VIDEO_ACHIEVEMENT_PAGE 2 // use same page/layer as bitmap
#define VIDEO_SPLASH_PAGE 0
#define VIDEO_SPLASH_CLUT 3
// reuse CLUT for achievements
#define VIDEO_ACHIEVEMENT_BASE_CLUT 1
#define VIDEO_ACHIEVEMENT_CLUT_COLOR 2
#define VIDEO_ACHIEVEMENT_CLUT_GREY 3

#define VIDEO_SCREEN_WIDTH 320u
#define VIDEO_SCREEN_HEIGHT 240u
#define VIDEO_BOARD_CELL_SIZE 28u
#define VIDEO_BOARD_CELL_SEPARATOR 1u
#define VIDEO_BOARD_INSIDE_WIDTH (VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 3 * VIDEO_BOARD_CELL_SEPARATOR)  
#define VIDEO_BOARD_INSIDE_HEIGHT (VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 7 * VIDEO_BOARD_CELL_SEPARATOR)
#define VIDEO_PIECE_SPRITE_SIZE 24u
#define VIDEO_FOCUS_SPRITE_SIZE 32u
#define VIDEO_ACHIEVEMENT_SPRITE_SIZE 24u
#define VIDEO_ICON_SPRITE_SIZE 32u
#define VIDEO_ICON_SELECT_SIZE 27u
#define VIDEO_BOARD_COLUMNS 4u
#define VIDEO_BOARD_ROWS 8u

#define VIDEO_BOARD_FIRST_CELL_X 95u
#define VIDEO_BOARD_FIRST_CELL_Y 5u

#define VIDEO_MENU_FIRST_ICON_X 240u
#define VIDEO_MENU_FIRST_ICON_Y 44u
#define VIDEO_MENU_SPACING_HORIZONTAL 30u
#define VIDEO_MENU_SPACING_VERTICAL 42u

#define VIDEO_ACHIEVEMENT_FIRST_ICON_X 32u
#define VIDEO_ACHIEVEMENT_FIRST_ICON_Y 44u
#define VIDEO_ACHIEVEMENT_SPACING_HORIZONTAL 77u
#define VIDEO_ACHIEVEMENT_SPACING_VERTICAL 100u

#define VIDEO_SPRITE_PIECE_LAYER 2
#define VIDEO_SPRITE_ICON_LAYER 2
#define VIDEO_SPRITE_HIGHLIGHT_LAYER 1
#define VIDEO_SPRITE_FOCUS_LAYER 0



// Sprite ID assignments
#define VIDEO_SPRITE_FOCUS_PIECE 0u
#define VIDEO_SPRITE_RESERVED 1u
#define VIDEO_SPRITE_HIGHLIGHT_BASE 2u  // After focus sprites
#define VIDEO_SPRITE_PIECE_BASE 18u
#define VIDEO_SPRITE_ICON_BASE 34u
#define VIDEO_SPRITE_OFFSET 32u  // Offset to avoid clipping at screen edges

// Type definitions needed for video system

typedef struct {
    uint8_t r, g, b;
} video_rgb_t;

typedef enum {
    VIDEO_PIECE_BITMAP_A_NORMAL_LIGHT = 0,
    VIDEO_PIECE_BITMAP_A_SWAPPED_LIGHT = 1,
    VIDEO_PIECE_BITMAP_B_NORMAL_LIGHT = 2,
    VIDEO_PIECE_BITMAP_B_SWAPPED_LIGHT = 3,
    VIDEO_PIECE_BITMAP_A_NORMAL_DARK = 4,
    VIDEO_PIECE_BITMAP_A_SWAPPED_DARK = 5,
    VIDEO_PIECE_BITMAP_B_NORMAL_DARK = 6,
    VIDEO_PIECE_BITMAP_B_SWAPPED_DARK = 7,
    VIDEO_PIECE_BITMAP_COUNT = 8
} video_piece_bitmap_id_t;

typedef enum {
    VIDEO_SPRITE_A_0 = 0, VIDEO_SPRITE_A_1 = 1, VIDEO_SPRITE_A_2 = 2, VIDEO_SPRITE_A_3 = 3,
    VIDEO_SPRITE_A_4 = 4, VIDEO_SPRITE_A_5 = 5, VIDEO_SPRITE_A_6 = 6, VIDEO_SPRITE_A_7 = 7,
    VIDEO_SPRITE_B_0 = 8, VIDEO_SPRITE_B_1 = 9, VIDEO_SPRITE_B_2 = 10, VIDEO_SPRITE_B_3 = 11,
    VIDEO_SPRITE_B_4 = 12, VIDEO_SPRITE_B_5 = 13, VIDEO_SPRITE_B_6 = 14, VIDEO_SPRITE_B_7 = 15,
    VIDEO_SPRITE_PIECE_COUNT = 16
} video_sprite_id_t;

typedef enum {
    VIDEO_ICON_GAME_MODE = 0, 
    VIDEO_ICON_RESET = 1, 
    VIDEO_ICON_PREVIOUS = 2, 
    VIDEO_ICON_NEXT = 3, 
    VIDEO_ICON_SWAP_MODE = 4,
    VIDEO_ICON_DIFFICULTY = 5,
    VIDEO_ICON_INFO = 6, 
    VIDEO_ICON_EXIT = 7,
    VIDEO_ICON_COUNT = 8
} video_icon_id_t;

extern const uint32_t s_video_icon_vram_addrs[VIDEO_ICON_COUNT];
extern void video_init(void);

void video_text_overlay_on(bool enable);
void clear_text_matrix(void);

void video_show_splash(void);
void video_hide_splash();
void video_wait_vblank(void);

void video_set_board_cell_hover_color(uint8_t row, uint8_t col);
void video_reset_board_cell_color(uint8_t row, uint8_t col);

void video_show_exit();
void video_splash_continue();


#endif // VIDEO_H