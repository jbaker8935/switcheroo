#include "../src/platform_f256.h"
#include "../include/f256lib.h"
#include <stdint.h>
#include <stddef.h>
#include "../src/board.h"
#include "../src/mouse_pointer.h"
#include "../src/video.h"
#include "../src/text_display.h"

// EMBED statements for assets at specific memory addresses
EMBED(puzzle_catalog, "../assets/generated/puzzle_data.bin", 0x20000);
EMBED(board_bitmap, "../assets/ui/ui_board.bin", 0x2FA00);
EMBED(splash_bitmap, "../assets/ui/ui_splash.bin", 0x42600);
EMBED(achievement_bitmap, "../assets/ui/ui_achievements.bin", 0x55200);
EMBED(board_palette, "../assets/ui/ui_board_palette.bin", 0x67E00);
EMBED(pieces_palette, "../assets/ui/ui_pieces_palette.bin", 0x68200);
EMBED(menu_icon_palette, "../assets/ui/ui_menu_palette.bin", 0x68600);
EMBED(splash_palette, "../assets/ui/ui_splash_palette.bin", 0x68A00);
EMBED(achieve_base_palette, "../assets/ui/ui_achieve_base_palette.bin", 0x68E00);
EMBED(achieve_color_palette, "../assets/ui/ui_achieve_color_palette.bin", 0x69200);
EMBED(achieve_grey_palette, "../assets/ui/ui_achieve_grey_palette.bin", 0x69600);
EMBED(icon_game_mode_ai, "../assets/ui/ui_menu_robot_32x32.bin", 0x69A00);
EMBED(icon_game_mode_puzzle, "../assets/ui/ui_menu_puzzle_32x32.bin", 0x69E00);
EMBED(icon_reset, "../assets/ui/ui_menu_retry_32x32.bin", 0x6A200);
EMBED(icon_previous, "../assets/ui/ui_menu_left_32x32.bin", 0x6A600);
EMBED(icon_next, "../assets/ui/ui_menu_right_32x32.bin", 0x6AA00);
EMBED(icon_swap_mode, "../assets/ui/ui_menu_swap_mode_32x32.bin", 0x6AE00);
EMBED(icon_difficulty, "../assets/ui/ui_menu_difficulty_32x32.bin", 0x6B200);
EMBED(icon_hint, "../assets/ui/ui_menu_hint_32x32.bin", 0x6B600);
EMBED(icon_exit, "../assets/ui/ui_menu_exit_32x32.bin", 0x6BA00);
EMBED(achieve_award, "../assets/ui/award.bin", 0x6BE00);
EMBED(achieve_100, "../assets/ui/achieve_100.bin", 0x6C040);
EMBED(achieve_runner, "../assets/ui/runner.bin", 0x6C280);
EMBED(achieve_brain, "../assets/ui/brain.bin", 0x6C4C0);
EMBED(achieve_puzzle, "../assets/ui/achieve_puzzle.bin", 0x6C700);
EMBED(achieve_thinker, "../assets/ui/thinker.bin", 0x6C940);
EMBED(achieve_lightning, "../assets/ui/lightning.bin", 0x6CB80);
EMBED(achieve_bullseye, "../assets/ui/bullseye.bin", 0x6CDC0);
EMBED(achieve_sword, "../assets/ui/sword.bin", 0x6D000);
EMBED(achieve_arm_flex, "../assets/ui/arm_flex.bin", 0x6D240);
EMBED(achieve_dice, "../assets/ui/dice.bin", 0x6D480);
EMBED(achieve_flame, "../assets/ui/flame.bin", 0x6D6C0);
EMBED(achieve_medal, "../assets/ui/medal.bin", 0x6D900);
EMBED(achieve_crown, "../assets/ui/crown.bin", 0x6DB40);
EMBED(piece_a_normal_light, "../assets/ui/playerA_normal_light.bin", 0x6DD80);
EMBED(piece_a_swapped_light, "../assets/ui/playerA_swapped_light.bin", 0x6DFC0);
EMBED(piece_b_normal_light, "../assets/ui/playerB_normal_light.bin", 0x6E200);
EMBED(piece_b_swapped_light, "../assets/ui/playerB_swapped_light.bin", 0x6E440);
EMBED(piece_a_normal_dark, "../assets/ui/playerA_normal_dark.bin", 0x6E680);
EMBED(piece_a_swapped_dark, "../assets/ui/playerA_swapped_dark.bin", 0x6E8C0);
EMBED(piece_b_normal_dark, "../assets/ui/playerB_normal_dark.bin", 0x6EB00);
EMBED(piece_b_swapped_dark, "../assets/ui/playerB_swapped_dark.bin", 0x6ED40);
EMBED(highlight_empty, "../assets/ui/highlight_empty.bin", 0x6EF80);
EMBED(highlight_occupied, "../assets/ui/highlight_occupied.bin", 0x6F1C0);
EMBED(focus_piece, "../assets/ui/cell_focus.bin", 0x6F400);


// Function declarations 
uint8_t video_board_palette_index(uint8_t row, uint8_t col);
void video_reset_board_cell_color(uint8_t row, uint8_t col);
void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player);
void video_reset_all_board_cell_colors(void);

// Order Icons 2 columns by 4 rows
const uint32_t s_video_icon_vram_addrs[VIDEO_ICON_COUNT] = {
    SRAM_ICON_GAME_MODE_AI,  // DEFAULT ICON is AI MODE, BITMAP SWITCHED BASED ON MODE
    SRAM_ICON_RESET,
    SRAM_ICON_PREVIOUS,
    SRAM_ICON_NEXT,
    SRAM_ICON_SWAP_MODE,
    SRAM_ICON_DIFFICULTY,
    SRAM_ICON_HINT,
    SRAM_ICON_EXIT
};



uint8_t video_board_palette_index(uint8_t row, uint8_t col) {
    // Map board position to CLUT index (1-32 for board cells)
    return 1 + (row * VIDEO_BOARD_COLUMNS + col);
}


// Function prototypes
static void video_setup_clut(void);

void clear_text_matrix(void) {
    // set i/o page to 2
    POKE(MMU_IO_CTRL, 2);
    for (uint8_t row = 0; row < 60; ++row) {
        for (uint8_t col = 0; col < 80; ++col) {
            POKE(0xC000 + row * 80 + col, 0x20); // Clear text matrix
        }
    }

        // set i/o page to 3
    POKE(MMU_IO_CTRL, 3);
    for (uint8_t row = 0; row < 60; ++row) {
        for (uint8_t col = 0; col < 80; ++col) {
            POKE(0xC000 + row * 80 + col, 0x11); // set color matrix
        }
    }

    POKE(MMU_IO_CTRL, 0); // Restore i/o page to 0
}

void video_text_overlay_on(bool enable) {
    // Enable or disable text overlay in master control
    uint8_t ctrl1 = PEEK(VKY_MSTR_CTRL_1);
    if (enable) {
        ctrl1 &= ~0b00010000; // Clear FON_OVLY bit
    } else {
        ctrl1 |= 0b00010000; // Set FON_OVLY bit
    }
    POKE(VKY_MSTR_CTRL_1, ctrl1);
}

void video_show_splash() {
    // Set bitmap address to splash data
    clear_text_matrix();
    // setup clut 0xDC00
    POKE(MMU_IO_CTRL, 1);    
    for (uint16_t i = 0; i < 1024; ++i) {
        uint8_t color_component = FAR_PEEK(SRAM_SPLASH_PALETTE + i);
        POKE(0xDC00 + i, color_component);
        
    }
    
    POKE(MMU_IO_CTRL, 0);
    
    // Set master control exactly like the example
    // XXX GAMMA  SPRITE   TILE  | BITMAP  GRAPH  OVRLY  TEXT
    POKE(VKY_MSTR_CTRL_0, 0b00101111); // sprite, bitmap, graph enabled 
    // XXX XXX  FON_SET FON_OVLY | MON_SLP DBL_Y  DBL_X  CLK_70
    POKE(VKY_MSTR_CTRL_1, 0b00000000); // 320x240 at 60 Hz with font overlay
    
    
    
    graphicsSetLayerBitmap(VIDEO_SPLASH_PAGE, 0);
    bitmapSetActive(VIDEO_SPLASH_PAGE);
    
    bitmapSetCLUT(VIDEO_SPLASH_CLUT);
    
    bitmapSetVisible(1, false);
    bitmapSetVisible(2, false);
    
    bitmapSetAddress(VIDEO_SPLASH_PAGE, SRAM_SPLASH_BASE);
    bitmapSetVisible(VIDEO_SPLASH_PAGE, true);
    
    disable_mouse();
    
}

void video_hide_splash() {

    bitmapSetVisible(VIDEO_SPLASH_PAGE, false);

}

void video_init(void) {
    // Set up configuration
    
    clear_text_matrix();

    spriteReset();

    video_setup_clut();


    POKE(MMU_IO_CTRL, 0);
    
    // Set master control exactly like the example
    // XXX GAMMA  SPRITE   TILE  | BITMAP  GRAPH  OVRLY  TEXT
    POKE(VKY_MSTR_CTRL_0, 0b00101111); // sprite, bitmap, graph enabled 
    // XXX XXX  FON_SET FON_OVLY | MON_SLP DBL_Y  DBL_X  CLK_70
    POKE(VKY_MSTR_CTRL_1, 0b00000000); // 320x240 at 60 Hz with font overlay


    bitmapSetAddress(VIDEO_BITMAP_PAGE, SRAM_BITMAP_BASE);

    graphicsSetLayerBitmap(VIDEO_BITMAP_PAGE, 2);
    bitmapSetActive(VIDEO_BITMAP_PAGE);

    bitmapSetCLUT(VIDEO_BOARD_CLUT);
    
    bitmapSetVisible(VIDEO_BITMAP_PAGE, true);
    bitmapSetVisible(0, false);
    bitmapSetVisible(1, false);

    // Define and position icon sprites (right panel) using EMBED addresses
    // Icons are arranged in 2 columns by 4 rows
    const uint16_t icon_start_x = VIDEO_MENU_FIRST_ICON_X;
    const uint16_t icon_start_y = VIDEO_MENU_FIRST_ICON_Y;
    const uint16_t icon_spacing_horizontal = VIDEO_MENU_SPACING_HORIZONTAL;
    const uint16_t icon_spacing_vertical = VIDEO_MENU_SPACING_VERTICAL;
    
    for (uint8_t i = 0; i < VIDEO_ICON_COUNT; ++i) {
        uint16_t icon_x = icon_start_x + ((i % 2) ? icon_spacing_horizontal : 0);
        uint8_t sprite_id = (VIDEO_SPRITE_ICON_BASE + i);
        uint16_t icon_y = (uint16_t)(icon_start_y + ((i / 2) * icon_spacing_vertical));

        spriteDefine(sprite_id, s_video_icon_vram_addrs[i], VIDEO_ICON_SPRITE_SIZE, VIDEO_MENU_CLUT, VIDEO_SPRITE_ICON_LAYER);
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + icon_x, VIDEO_SPRITE_OFFSET + icon_y);
        spriteSetVisible(sprite_id, 1);

    }

    // White graphics background
    POKE(0xD00D, 0xFF);
    POKE(0xD00E, 0xFF);
    POKE(0xD00F, 0xFF);

    // Initialize PS/2 mouse hardware
    // Mouse coordinate system is always 640x480 regardless of video mode
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    enable_mouse();
    center_mouse();

}

static void video_setup_clut() {

    // set i/o page to 1
    POKE(MMU_IO_CTRL, 1);

    // load board palette into clut 0

    for (uint16_t i = 0; i < 1024; ++i) {
        uint8_t color_component = FAR_PEEK(SRAM_BOARD_PALETTE + i);
        POKE(0xD000 + i, color_component);

    }
    
    
    // load pieces palette into clut 1
    
    for (uint16_t i = 0; i < 1024; ++i) {
        uint8_t color_component = FAR_PEEK(SRAM_PIECES_PALETTE + i);
        POKE(0xD400 + i, color_component);
        
    }
    
    // load menu palette into clut 2
    
    for (uint16_t i = 0; i < 1024; ++i) {
        uint8_t color_component = FAR_PEEK(SRAM_MENU_PALETTE + i);
        POKE(0xD800 + i, color_component);
        
    }
        
    POKE(MMU_IO_CTRL, 0); // Restore i/o page to 0

}

void video_wait_vblank(void) {
    graphicsWaitVerticalBlank();
}

void video_set_game_mode_icon_bitmap(bool is_puzzle_mode) {
    uint32_t bitmap_addr = is_puzzle_mode ? SRAM_ICON_GAME_MODE_PUZZLE : SRAM_ICON_GAME_MODE_AI;
    uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_ICON_BASE + VIDEO_ICON_GAME_MODE);

    spriteDefine(sprite_id, bitmap_addr, VIDEO_ICON_SPRITE_SIZE, VIDEO_MENU_CLUT, VIDEO_SPRITE_ICON_LAYER);
    spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + VIDEO_MENU_FIRST_ICON_X, VIDEO_SPRITE_OFFSET + VIDEO_MENU_FIRST_ICON_Y);
    spriteSetVisible(sprite_id, 1);
}

// Reset board cell CLUT to original checkerboard color
void video_reset_board_cell_color(uint8_t row, uint8_t col) {
    uint8_t clut_index = video_board_palette_index(row, col);
    const uint8_t b = FAR_PEEK(SRAM_BOARD_PALETTE + clut_index * 4);
    const uint8_t g = FAR_PEEK(SRAM_BOARD_PALETTE + clut_index * 4 + 1);
    const uint8_t r = FAR_PEEK(SRAM_BOARD_PALETTE + clut_index * 4 + 2);

    graphicsDefineColor(VIDEO_BOARD_CLUT, clut_index, r,g,b);
}

// Set board cell CLUT to win highlight color for a player
void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player) {
    uint8_t clut_index = video_board_palette_index(row, col);
    if (player == PLAYER_WHITE) {
        graphicsDefineColor(VIDEO_BOARD_CLUT, clut_index, 0xF4,0xC2,0x44);        
    } else {
        graphicsDefineColor(VIDEO_BOARD_CLUT, clut_index, 0xF0,0x7C,0x40);        
    }
    

}

// Reset all board cell CLUTs to original checkerboard colors
void video_reset_all_board_cell_colors(void) {
    
    for (uint8_t row = 0; row < VIDEO_BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < VIDEO_BOARD_COLUMNS; ++col) {
            video_reset_board_cell_color(row, col);
        }
    }
}
