#include "platform_f256.h"
#include "f256lib.h"
#include <stdint.h>
#include <stddef.h>
#include "board.h"
#include "mouse_pointer.h"
#include "video.h"
#include "text_display.h"
#include "sram_assets.h"
#include "dma_copy.h"
#include "overlay_config.h"

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

void video_splash_continue(void) {
    // copy in "Space to continue" at Start x 106 y 211 w 106 h 14
    // uint16_t byte_offset = 0;
    // for (uint16_t row= 0; row < 14; row++) {
    //     for (uint16_t col = 0; col < 106; col++) {
    //         uint8_t byte = FAR_PEEK(SRAM_UI_SPLASH_CONTINUE + byte_offset);
    //         uint16_t screen_x = 106 + col;
    //         uint16_t screen_y = 209 + row;
    //         uint32_t addr_offset = mathUnsignedAddition(mathUnsignedMultiply(screen_y,320), screen_x);
    //         FAR_POKE(SRAM_SPLASH_BASE + addr_offset, byte);
    //         byte_offset++;
    //     }
    // } 
    dma2dCopy(SRAM_SPLASH_BASE + ((uint32_t) 320 * 209) + 106,SRAM_UI_SPLASH_CONTINUE, 106, 14, 106, 320);
}


#pragma code(ovl13_code)
void FAR_video_init(void) {
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

    // grey graphics background
    POKE(0xD00D, 0x33);
    POKE(0xD00E, 0x33);
    POKE(0xD00F, 0x33);

    // Initialize PS/2 mouse hardware
    // Mouse coordinate system is always 640x480 regardless of video mode
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    enable_mouse();
    center_mouse();

}
#pragma code(code)

void video_init(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_video_init();
    POKE(OVERLAY_MMU_REG, saved);
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

// Set board cell CLUT to hover highlight color
void video_set_board_cell_hover_color(uint8_t row, uint8_t col) {
    uint8_t clut_index = video_board_palette_index(row, col);
    if ((row+col) % 2 == 0) {
        graphicsDefineColor(VIDEO_BOARD_CLUT, clut_index, 196,223,243);        
    } else {
        graphicsDefineColor(VIDEO_BOARD_CLUT, clut_index, 102,124,142);        
    }
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
