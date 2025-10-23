#include "../src/platform_f256.h"
#include <stdint.h>
#include <stddef.h>
#include "../src/board.h"
#include "../src/mouse_pointer.h"
#include "../src/video.h"

// EMBED statements for assets at specific memory addresses
EMBED(puzzle_catalog, "../assets/generated/puzzle_data.bin", SRAM_PUZZLE_CATALOG);
EMBED(board_bitmap, "../assets/ui/ui_board.bin", SRAM_BITMAP_BASE);
EMBED(piece_a_normal_light, "../assets/ui/playerA_normal_light.bin", SRAM_PIECE_A_NORMAL_LIGHT);
EMBED(piece_a_swapped_light, "../assets/ui/playerA_swapped_light.bin", SRAM_PIECE_A_SWAPPED_LIGHT);
EMBED(piece_b_normal_light, "../assets/ui/playerB_normal_light.bin", SRAM_PIECE_B_NORMAL_LIGHT);
EMBED(piece_b_swapped_light, "../assets/ui/playerB_swapped_light.bin", SRAM_PIECE_B_SWAPPED_LIGHT);
EMBED(piece_a_normal_dark, "../assets/ui/playerA_normal_dark.bin", SRAM_PIECE_A_NORMAL_DARK);
EMBED(piece_a_swapped_dark, "../assets/ui/playerA_swapped_dark.bin", SRAM_PIECE_A_SWAPPED_DARK);
EMBED(piece_b_normal_dark, "../assets/ui/playerB_normal_dark.bin", SRAM_PIECE_B_NORMAL_DARK);
EMBED(piece_b_swapped_dark, "../assets/ui/playerB_swapped_dark.bin", SRAM_PIECE_B_SWAPPED_DARK);

// Icon bitmaps
EMBED(icon_game_ai_mode, "../assets/ui/ui_menu_robot_32x32.bin", SRAM_ICON_GAME_MODE_AI);
EMBED(icon_game_puzzle_mode, "../assets/ui/ui_menu_puzzle_32x32.bin", SRAM_ICON_GAME_MODE_PUZZLE);
EMBED(icon_reset, "../assets/ui/ui_menu_retry_32x32.bin", SRAM_ICON_RESET);
EMBED(icon_previous, "../assets/ui/ui_menu_left_32x32.bin", SRAM_ICON_PREVIOUS);
EMBED(icon_next, "../assets/ui/ui_menu_right_32x32.bin", SRAM_ICON_NEXT);
EMBED(icon_swap_mode, "../assets/ui/ui_menu_swap_mode_32x32.bin", SRAM_ICON_SWAP_MODE);
EMBED(icon_difficulty, "../assets/ui/ui_menu_difficulty_32x32.bin", SRAM_ICON_DIFFICULTY);
EMBED(icon_info, "../assets/ui/ui_menu_hint_32x32.bin", SRAM_ICON_HINT);
EMBED(icon_exit, "../assets/ui/ui_menu_exit_32x32.bin", SRAM_ICON_EXIT);

// Move Highlight sprite bitmaps
EMBED(highlight_empty_bitmap, "../assets/ui/highlight_empty.bin", SRAM_HIGHLIGHT_EMPTY);
EMBED(highlight_occupied_bitmap, "../assets/ui/highlight_occupied.bin", SRAM_HIGHLIGHT_OCCUPIED);
// Focus indicator bitmaps
EMBED(focus_piece_bitmap, "../assets/ui/cell_focus.bin", SRAM_FOCUS_PIECE);
EMBED(focus_icon_bitmap, "../assets/generated/focus_icon_bitmap.bin", SRAM_FOCUS_ICON);

// Palette VRAM areas
EMBED(board_palette_data, "../assets/ui/ui_board_palette.bin", SRAM_BOARD_PALETTE);
EMBED(pieces_palette_data, "../assets/ui/ui_pieces_palette.bin", SRAM_PIECES_PALETTE);
EMBED(menu_palette_data, "../assets/ui/ui_menu_palette.bin", SRAM_MENU_PALETTE);


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
static void video_position_sprites(void);

void clear_text_matrix(void) {
    // set i/o page to 2
    POKE(MMU_IO_CTRL, 2);
    for (uint8_t row = 0; row < 60; ++row) {
        for (uint8_t col = 0; col < 80; ++col) {
            POKE(0xC000 + row * 80 + col, 0x20); // Clear text matrix
        }
    }
    POKE(MMU_IO_CTRL, 0); // Restore i/o page to 0
}

void video_init(void) {
    // Set up configuration
    
    clear_text_matrix();


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
    
    for (uint16_t i = 0; i < VIDEO_ICON_COUNT; ++i) {
        const uint16_t icon_x = icon_start_x + ((i % 2) ? icon_spacing_horizontal : 0);
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_ICON_BASE + i);
        uint16_t icon_y = (uint16_t)(icon_start_y + ((i / 2) * icon_spacing_vertical));

        spriteDefine(sprite_id, s_video_icon_vram_addrs[i], VIDEO_ICON_SPRITE_SIZE, VIDEO_MENU_CLUT, VIDEO_SPRITE_ICON_LAYER);
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + icon_x, VIDEO_SPRITE_OFFSET + icon_y);
        spriteSetVisible(sprite_id, 1);
    }    

    // Force black graphics background
    POKE(0xD00D, 0x00);
    POKE(0xD00E, 0x00);
    POKE(0xD00F, 0x00);

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
    // Calculate board layout
    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11; // Extra for border
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15; // Extra for border
    const int16_t board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    const int16_t board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;
    spriteDefine(sprite_id, bitmap_addr, VIDEO_ICON_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_ICON_LAYER);
    spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + board_x + board_width + 16, VIDEO_SPRITE_OFFSET + board_y + 8);
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
