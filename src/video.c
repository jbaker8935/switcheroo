#include "../src/platform_f256.h"
#include <stdint.h>
#include <stddef.h>
#include "../src/board.h"
#include "../src/mouse_pointer.h"



// EMBED statements for assets at specific memory addresses
EMBED(puzzle_catalog, "../assets/generated/puzzle_data.bin", 0x30000);
EMBED(board_bitmap, "../assets/generated/board_bitmap.bin", 0x44000);
EMBED(piece_bitmap_a_normal, "../assets/generated/piece_bitmap_a_normal.bin", 0x56c00);
EMBED(piece_bitmap_a_swapped, "../assets/generated/piece_bitmap_a_swapped.bin", 0x58000);
EMBED(piece_bitmap_b_normal, "../assets/generated/piece_bitmap_b_normal.bin", 0x59400);
EMBED(piece_bitmap_b_swapped, "../assets/generated/piece_bitmap_b_swapped.bin", 0x5a800);

// Icon bitmaps
EMBED(icon_reset, "../assets/generated/reset.bin", 0x5bc00);
EMBED(icon_info, "../assets/generated/icon_info.bin", 0x5c400);
EMBED(icon_difficulty, "../assets/generated/difficulty_slider.bin", 0x5cc00);
EMBED(icon_next, "../assets/generated/next.bin", 0x5d400);
EMBED(icon_swap_mode, "../assets/generated/swap_mode.bin", 0x5dc00);
EMBED(icon_exit, "../assets/generated/icon_exit.bin", 0x5e400);
EMBED(icon_game_ai_mode, "../assets/generated/ai_mode.bin", 0x5f100);
EMBED(icon_game_puzzle_mode, "../assets/generated/icon_puzzle.bin", 0x5f200);
EMBED(icon_previous, "../assets/generated/previous.bin", 0x5f300);

// Move Highlight sprite bitmaps
EMBED(highlight_empty_bitmap, "../assets/generated/highlight_empty_bitmap.bin", 0x5e500);
EMBED(highlight_occupied_bitmap, "../assets/generated/highlight_occupied_bitmap.bin", 0x5e800);
// Focus indicator bitmaps
EMBED(focus_piece_bitmap, "../assets/generated/focus_piece_bitmap.bin", 0x5ec00);
EMBED(focus_icon_bitmap, "../assets/generated/focus_icon_bitmap.bin", 0x5f000);

// Type definitions needed for video system
typedef enum {
    VIDEO_THEME_DEFAULT = 0,
    VIDEO_THEME_HIGH_CONTRAST = 1,
    VIDEO_THEME_COLORBLIND = 2,
    VIDEO_THEME_COUNT
} video_theme_t;

typedef struct {
    uint8_t r, g, b;
} video_rgb_t;

typedef struct {
    video_rgb_t background;
    video_rgb_t board_light;
    video_rgb_t board_dark;
    video_rgb_t board_border;
    video_rgb_t ui_panel;
    video_rgb_t highlight_primary;
    video_rgb_t highlight_secondary;
    /* Highlight SPRITE colors (separate from board highlight) */
    video_rgb_t highlight_sprite_empty_primary;
    video_rgb_t highlight_sprite_empty_secondary;
    video_rgb_t highlight_sprite_occupied_primary;
    video_rgb_t highlight_sprite_occupied_secondary;
    video_rgb_t text_primary;
} video_palette_t;

typedef struct {
    uint8_t enable_double_buffer;
    uint8_t front_bitmap_page;
    uint8_t back_bitmap_page;
    video_theme_t theme;
} video_config_t;

typedef enum {
    VIDEO_PIECE_BITMAP_A_NORMAL = 0,
    VIDEO_PIECE_BITMAP_A_SWAPPED = 1,
    VIDEO_PIECE_BITMAP_B_NORMAL = 2,
    VIDEO_PIECE_BITMAP_B_SWAPPED = 3,
    VIDEO_PIECE_BITMAP_COUNT = 4
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
    IDEO_ICON_DIFFICULTY = 5,
    VIDEO_ICON_INFO = 6, 
    VIDEO_ICON_EXIT = 7,
    VIDEO_ICON_COUNT = 8
} video_icon_id_t;

// Function declarations 
const video_palette_t *video_get_theme_palette(video_theme_t theme);
uint8_t video_board_palette_index(uint8_t row, uint8_t col);
void video_reset_board_cell_color(uint8_t row, uint8_t col);
void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player);
void video_reset_all_board_cell_colors(void);

// Constants
#define VIDEO_PRIMARY_CLUT 0
#define VIDEO_BITMAP_PAGE 2

#define VIDEO_SCREEN_WIDTH 320u
#define VIDEO_SCREEN_HEIGHT 240u
#define VIDEO_BOARD_CELL_SIZE 26u
#define VIDEO_PIECE_SPRITE_SIZE 24u
#define VIDEO_ICON_SPRITE_SIZE 16u
#define VIDEO_BOARD_COLUMNS 4u
#define VIDEO_BOARD_ROWS 8u

// VRAM layout - bitmap and sprite addresses matching EMBED locations
#define VIDEO_VRAM_BITMAP_BASE 0x44000u
#define VIDEO_VRAM_PIECE_A_NORMAL 0x56c00u
#define VIDEO_VRAM_PIECE_A_SWAPPED 0x58000u
#define VIDEO_VRAM_PIECE_B_NORMAL 0x59400u
#define VIDEO_VRAM_PIECE_B_SWAPPED 0x5a800u

#define VIDEO_VRAM_ICON_RESET 0x5bc00u
#define VIDEO_VRAM_ICON_HINT 0x5c400u
#define VIDEO_VRAM_ICON_DIFFICULTY 0x5cc00u
#define VIDEO_VRAM_ICON_NEXT 0x5d400u
#define VIDEO_VRAM_ICON_PREVIOUS 0x5f300u
#define VIDEO_VRAM_ICON_SWAP_MODE 0x5dc00u
#define VIDEO_VRAM_ICON_GAME_MODE_AI 0x5f100u
#define VIDEO_VRAM_ICON_GAME_MODE_PUZZLE 0x5f200u
#define VIDEO_VRAM_ICON_EXIT 0x5e400u

// Order Icons 2 columns by 4 rows
static const uint32_t s_video_icon_vram_addrs[VIDEO_ICON_COUNT] = {
    VIDEO_VRAM_ICON_GAME_MODE_AI,  // DEFAULT ICON is AI MODE, BITMAP SWITCHED BASED ON MODE
    VIDEO_VRAM_ICON_RESET,
    VIDEO_VRAM_ICON_PREVIOUS,
    VIDEO_VRAM_ICON_NEXT,
    VIDEO_VRAM_ICON_SWAP_MODE,
    VIDEO_VRAM_ICON_DIFFICULTY,
    VIDEO_VRAM_ICON_HINT,
    VIDEO_VRAM_ICON_EXIT
};

// Highlight sprite VRAM addresses (empty and occupied variants)
#define VIDEO_VRAM_HIGHLIGHT_EMPTY 0x5e500u
#define VIDEO_VRAM_HIGHLIGHT_OCCUPIED 0x5e800u
// Focus VRAM addresses
#define VIDEO_VRAM_FOCUS_PIECE 0x5ec00u
#define VIDEO_VRAM_FOCUS_ICON 0x5f000u

// Sprite ID assignments  
#define VIDEO_SPRITE_PIECE_BASE 0u
#define VIDEO_SPRITE_ICON_BASE (VIDEO_SPRITE_PIECE_BASE + VIDEO_SPRITE_PIECE_COUNT)
#define VIDEO_SPRITE_OFFSET 32u  // Offset to avoid clipping at screen edges

// CLUT indices - per video_assets.md specification
#define VIDEO_CLUT_BOARD_BORDER 33
#define VIDEO_CLUT_UI_PANEL 34
#define VIDEO_CLUT_HIGHLIGHT_PRIMARY 35
#define VIDEO_CLUT_HIGHLIGHT_SECONDARY 36
#define VIDEO_CLUT_TEXT_PRIMARY 37
// Highlight sprite CLUT slots (distinct from board highlight CLUTs)
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_PRIMARY 85
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_SECONDARY 86
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY 87
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_SECONDARY 88

// Focus CLUT index (single slot used for focus outline)
#define VIDEO_CLUT_FOCUS 89

// Player A piece colors (indices 65-68, 73-74) - per video_assets.md
#define VIDEO_CLUT_PLAYER_A_EDGE_1 65
#define VIDEO_CLUT_PLAYER_A_EDGE_2 66
#define VIDEO_CLUT_PLAYER_A_FILL_1 67
#define VIDEO_CLUT_PLAYER_A_FILL_2 68
#define VIDEO_CLUT_PLAYER_A_SWAPPED_1 73
#define VIDEO_CLUT_PLAYER_A_SWAPPED_2 74

// Player B piece colors (indices 69-72, 75-76) - per video_assets.md
#define VIDEO_CLUT_PLAYER_B_EDGE_1 69
#define VIDEO_CLUT_PLAYER_B_EDGE_2 70
#define VIDEO_CLUT_PLAYER_B_FILL_1 71
#define VIDEO_CLUT_PLAYER_B_FILL_2 72
#define VIDEO_CLUT_PLAYER_B_SWAPPED_1 75
#define VIDEO_CLUT_PLAYER_B_SWAPPED_2 76

// Icon colors (indices 77-84) - per video_assets.md
#define VIDEO_CLUT_ICON_EDGE_1 77
#define VIDEO_CLUT_ICON_EDGE_2 78
#define VIDEO_CLUT_ICON_FILL_1 79
#define VIDEO_CLUT_ICON_FILL_2 80
#define VIDEO_CLUT_ICON_SYMBOL_1 81
#define VIDEO_CLUT_ICON_SYMBOL_2 82
#define VIDEO_CLUT_ICON_SYMBOL_3 83
#define VIDEO_CLUT_ICON_SYMBOL_4 84

#define VIDEO_PRIMARY_CLUT 0
#define VIDEO_BITMAP_PAGE 2

// VRAM layout - bitmap at fixed address
#define VIDEO_VRAM_BITMAP_BASE 0x44000u

// Color themes
static const video_palette_t kThemePalettes[VIDEO_THEME_COUNT] = {
    [VIDEO_THEME_DEFAULT] = {
        .background = { .r = 0x10, .g = 0x16, .b = 0x1C },
        .board_light = { .r = 0x90, .g = 0xA8, .b = 0xC0 },
        .board_dark = { .r = 0x38, .g = 0x44, .b = 0x5A },
        .board_border = { .r = 0xC0, .g = 0xD8, .b = 0xF0 },
        .ui_panel = { .r = 0x22, .g = 0x28, .b = 0x36 },
        .highlight_primary = { .r = 0xF4, .g = 0xC2, .b = 0x44 },
        .highlight_secondary = { .r = 0xF0, .g = 0x7C, .b = 0x40 },
        .highlight_sprite_empty_primary = { .r = 0x38, .g = 0xb0, .b = 0x58 },
        .highlight_sprite_empty_secondary = { .r = 0x63, .g = 0xf6, .b = 0x51 },
        .highlight_sprite_occupied_primary = { .r = 0x5c, .g = 0x46, .b = 0x0b },
        .highlight_sprite_occupied_secondary = { .r = 0xab, .g = 0x7f, .b = 0x0a },
        .text_primary = { .r = 0xF4, .g = 0xF4, .b = 0xFA },
    },
    [VIDEO_THEME_HIGH_CONTRAST] = {
        .background = { .r = 0x00, .g = 0x00, .b = 0x00 },
        .board_light = { .r = 0xFF, .g = 0xFF, .b = 0xFF },
        .board_dark = { .r = 0x00, .g = 0x00, .b = 0x00 },
        .board_border = { .r = 0xFF, .g = 0xD7, .b = 0x00 },
        .ui_panel = { .r = 0x20, .g = 0x20, .b = 0x20 },
        .highlight_primary = { .r = 0xFF, .g = 0x45, .b = 0x00 },
        .highlight_secondary = { .r = 0x00, .g = 0xBF, .b = 0xFF },
    .highlight_sprite_empty_primary = { .r = 0xFF, .g = 0x80, .b = 0x40 },
    .highlight_sprite_empty_secondary = { .r = 0xFF, .g = 0x60, .b = 0x20 },
    .highlight_sprite_occupied_primary = { .r = 0xFF, .g = 0x45, .b = 0x00 },
    .highlight_sprite_occupied_secondary = { .r = 0x00, .g = 0xBF, .b = 0xFF },
        .text_primary = { .r = 0xFF, .g = 0xFF, .b = 0xFF },
    },
    [VIDEO_THEME_COLORBLIND] = {
        .background = { .r = 0x12, .g = 0x1C, .b = 0x18 },
        .board_light = { .r = 0xB4, .g = 0xD6, .b = 0xB8 },
        .board_dark = { .r = 0x2E, .g = 0x54, .b = 0x3E },
        .board_border = { .r = 0xE0, .g = 0xF6, .b = 0xE4 },
        .ui_panel = { .r = 0x1A, .g = 0x26, .b = 0x22 },
        .highlight_primary = { .r = 0xFF, .g = 0xB0, .b = 0x4C },
        .highlight_secondary = { .r = 0x5A, .g = 0xC8, .b = 0xFF },
        .highlight_sprite_empty_primary = { .r = 0xFF, .g = 0xD9, .b = 0xB0 },
        .highlight_sprite_empty_secondary = { .r = 0xFF, .g = 0xC0, .b = 0x88 },
        .highlight_sprite_occupied_primary = { .r = 0xFF, .g = 0xB0, .b = 0x4C },
        .highlight_sprite_occupied_secondary = { .r = 0x5A, .g = 0xC8, .b = 0xFF },
        .text_primary = { .r = 0xF0, .g = 0xFF, .b = 0xF0 },
    },
};

// Helper functions
const video_palette_t *video_get_theme_palette(video_theme_t theme) {
    if (theme >= VIDEO_THEME_COUNT) {
        theme = VIDEO_THEME_DEFAULT;
    }
    return &kThemePalettes[theme];
}

uint8_t video_board_palette_index(uint8_t row, uint8_t col) {
    // Map board position to CLUT index (1-32 for board cells)
    return 1 + (row * VIDEO_BOARD_COLUMNS + col);
}

// Global state
static video_config_t s_video_config = {
    .enable_double_buffer = 0,
    .front_bitmap_page = 0,
    .back_bitmap_page = 0,
    .theme = VIDEO_THEME_DEFAULT,
};


static video_theme_t s_active_theme = VIDEO_THEME_DEFAULT;

// Function prototypes
static void video_setup_clut(const video_palette_t *palette);
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


void video_init(const video_config_t *config) {
    // Set up configuration
    if (config != NULL) {
        s_video_config = *config;
    }
    
    if (s_video_config.theme >= VIDEO_THEME_COUNT) {
        s_video_config.theme = VIDEO_THEME_DEFAULT;
    }
    
    s_active_theme = s_video_config.theme;

    const video_palette_t *palette = video_get_theme_palette(s_active_theme);
    
    clear_text_matrix();


    video_setup_clut(palette);
    // Initialize video hardware following EMBED example pattern


    POKE(MMU_IO_CTRL, 0);
    
    // Set master control exactly like the example
    // XXX GAMMA  SPRITE   TILE  | BITMAP  GRAPH  OVRLY  TEXT
    POKE(VKY_MSTR_CTRL_0, 0b00101111); // sprite, bitmap, graph enabled 
    // XXX XXX  FON_SET FON_OVLY | MON_SLP DBL_Y  DBL_X  CLK_70
    POKE(VKY_MSTR_CTRL_1, 0b00000000); // 320x240 at 60 Hz with font overlay


    bitmapSetAddress(VIDEO_BITMAP_PAGE, VIDEO_VRAM_BITMAP_BASE);

    graphicsSetLayerBitmap(VIDEO_BITMAP_PAGE, 2);
    bitmapSetActive(VIDEO_BITMAP_PAGE);

    bitmapSetCLUT(VIDEO_PRIMARY_CLUT);
    
    bitmapSetVisible(VIDEO_BITMAP_PAGE, true);
    bitmapSetVisible(0, false);
    bitmapSetVisible(1, false);

    // Force black graphics background
    POKE(0xD00D, 0x00);
    POKE(0xD00E, 0x00);
    POKE(0xD00F, 0x00);

    // Initialize PS/2 mouse hardware
    // Mouse coordinate system is always 640x480 regardless of video mode
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    enable_mouse();
    center_mouse();

    // Load and position sprites
    // Assets are embedded at fixed addresses using EMBED - no manifest needed
    video_position_sprites();

    // output graphics table data.


}

static void video_setup_clut(const video_palette_t *palette) {

    
    // Set up board cell colors (slots 1-32)
    for (uint8_t row = 0; row < VIDEO_BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < VIDEO_BOARD_COLUMNS; ++col) {
            uint8_t clut_index = video_board_palette_index(row, col);
            const video_rgb_t *color = ((row + col) & 1) ? &palette->board_dark : &palette->board_light;
            graphicsDefineColor(VIDEO_PRIMARY_CLUT, clut_index, color->r, color->g, color->b);
        }
    }
    
    // Set board border color (slot 33)
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_BOARD_BORDER, 
                       palette->board_border.r, palette->board_border.g, palette->board_border.b);
    
    // Add placeholder greyscale for gradients
    for (uint8_t i = 0; i < 31; ++i) {
        const uint8_t base = 60;
        graphicsDefineColor(VIDEO_PRIMARY_CLUT, 34 + i, base + i * 2, base + i * 2, base + i * 2);
    }

    // Set piece sprite colors (slots 65-76) - modern blue/purple theme
    // Player A colors - Blue theme
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_EDGE_1, 0x4A, 0x90, 0xE2);  // Light blue edge
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_EDGE_2, 0x21, 0x71, 0xB5);  // Dark blue edge
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_FILL_1, 0x6e, 0xc5, 0xff);  // Blue fill 1
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_FILL_2, 0x29, 0x80, 0xB9);  // Blue fill 2
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_SWAPPED_1, 0xFF, 0xFF, 0xFF);  // White star
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_A_SWAPPED_2, 0xF0, 0xF0, 0xF0);  // Light gray star
    
    // Player B colors - Purple theme
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_EDGE_1, 0xA4, 0x58, 0xC3);  // Light purple edge
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_EDGE_2, 0x8E, 0x44, 0xAD);  // Dark purple edge
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_FILL_1, 0x7D, 0x3C, 0x98);  // Purple fill 1
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_FILL_2, 0xc3, 0x97, 0xd4);  // Purple fill 2
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_SWAPPED_1, 0xFF, 0xFF, 0xFF);  // White diamond
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_PLAYER_B_SWAPPED_2, 0xF0, 0xF0, 0xF0);  // Light gray diamond
    
    // Icon sprite colors (slots 77-84) - Orange theme
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_EDGE_1, 0x00, 0x00, 0x00);  // Orange edge 1
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_EDGE_2, 0x13, 0x0b, 0xf9);  // Dark orange edge 2
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_FILL_1, 0x3b, 0x61, 0xe8);  // Orange fill 1
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_FILL_2, 0x19, 0x85, 0xee);  // Orange fill 2
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_SYMBOL_1, 0xe8, 0x33, 0x33);  // Red symbol 1
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_SYMBOL_2, 0xee, 0x3f, 0x3f);  // Red symbol 2
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_SYMBOL_3, 0xd7, 0xa0, 0x0d);  // Yellow symbol 3
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_ICON_SYMBOL_4, 0xff, 0xe3, 0x97);  // Yellow symbol 4

    // Highlight colors (slots 35-36) - from active palette
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_PRIMARY,
                       palette->highlight_primary.r, palette->highlight_primary.g, palette->highlight_primary.b);
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_SECONDARY,
                       palette->highlight_secondary.r, palette->highlight_secondary.g, palette->highlight_secondary.b);
    
    // Highlight SPRITE colors (slots 85-88) - separate from board highlight
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_PRIMARY,
                       palette->highlight_sprite_empty_primary.r, palette->highlight_sprite_empty_primary.g, palette->highlight_sprite_empty_primary.b);
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_SECONDARY,
                       palette->highlight_sprite_empty_secondary.r, palette->highlight_sprite_empty_secondary.g, palette->highlight_sprite_empty_secondary.b);
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY,
                       palette->highlight_sprite_occupied_primary.r, palette->highlight_sprite_occupied_primary.g, palette->highlight_sprite_occupied_primary.b);
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_SECONDARY,
                       palette->highlight_sprite_occupied_secondary.r, palette->highlight_sprite_occupied_secondary.g, palette->highlight_sprite_occupied_secondary.b);
    

}

static void video_position_sprites(void) {
    // Calculate board layout
    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11; // Extra for border
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15; // Extra for border
    const int16_t board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    const int16_t board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;
    
    // Calculate centering offset for pieces within cells
    // Cell is 28x28, piece is 24x24, so offset by (28-24)/2 = 2 pixels
    const uint16_t cell_offset = (VIDEO_BOARD_CELL_SIZE - VIDEO_PIECE_SPRITE_SIZE) / 2;
    
    // Account for 4-pixel border before first cell
    const int16_t first_cell_x = board_x + 4;
    const int16_t first_cell_y = board_y + 4;

    // Define piece sprites - all share bitmap data from EMBED locations
    // Player A sprites (8 total) use normal bitmap initially
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_PIECE_BASE + VIDEO_SPRITE_A_0 + i);
        
        spriteDefine(sprite_id, VIDEO_VRAM_PIECE_A_NORMAL, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetVisible(sprite_id, 0);  // Start hidden
    }
    
    // Player B sprites (8 total) use normal bitmap initially  
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_PIECE_BASE + VIDEO_SPRITE_B_0 + i);
        
        spriteDefine(sprite_id, VIDEO_VRAM_PIECE_B_NORMAL, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetVisible(sprite_id, 0);  // Start hidden
    }
    
    // Position Player A pieces (bottom 2 rows)
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t row = 6 + (i / 4);  // Rows 6-7
        uint8_t col = i % 4;
        
        // Calculate cell position, then center piece within cell
        uint16_t cell_x = (uint16_t)(first_cell_x + (col * (VIDEO_BOARD_CELL_SIZE + 1)));
        uint16_t cell_y = (uint16_t)(first_cell_y + (row * (VIDEO_BOARD_CELL_SIZE + 1)));
        uint16_t piece_x = cell_x + cell_offset;
        uint16_t piece_y = cell_y + cell_offset;
        
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_PIECE_BASE + VIDEO_SPRITE_A_0 + i);
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + piece_x, VIDEO_SPRITE_OFFSET + piece_y);
        spriteSetVisible(sprite_id, 1);
    }
    
    // Position Player B pieces (top 2 rows)
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t row = i / 4;  // Rows 0-1
        uint8_t col = i % 4;
        
        // Calculate cell position, then center piece within cell
        uint16_t cell_x = (uint16_t)(first_cell_x + (col * (VIDEO_BOARD_CELL_SIZE + 1)));
        uint16_t cell_y = (uint16_t)(first_cell_y + (row * (VIDEO_BOARD_CELL_SIZE + 1)));
        uint16_t piece_x = cell_x + cell_offset;
        uint16_t piece_y = cell_y + cell_offset;
        
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_PIECE_BASE + VIDEO_SPRITE_B_0 + i);
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + piece_x, VIDEO_SPRITE_OFFSET + piece_y);
        spriteSetVisible(sprite_id, 1);
    }
    
    // Define and position icon sprites (right panel) using EMBED addresses
    // Icons are arranged in 2 columns by 4 rows
    const uint16_t icon_start_y = (uint16_t)(board_y + 8);
    const uint16_t icon_spacing = VIDEO_ICON_SPRITE_SIZE + 8;
    
    for (uint16_t i = 0; i < VIDEO_ICON_COUNT; ++i) {
        const uint16_t icon_x = (uint16_t)(board_x + board_width + 16) + ((i % 2) ? icon_spacing : 0);
        uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_ICON_BASE + i);
        uint16_t y = (uint16_t)(icon_start_y + ((i / 2) * icon_spacing));
        
        spriteDefine(sprite_id, s_video_icon_vram_addrs[i], VIDEO_ICON_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + icon_x, VIDEO_SPRITE_OFFSET + y);
        spriteSetVisible(sprite_id, 1);
    }
}

void video_apply_palette(const video_palette_t *palette) {
    if (palette != NULL) {
        video_setup_clut(palette);
    }
}

void video_apply_theme(video_theme_t theme) {
    if (theme >= VIDEO_THEME_COUNT) {
        theme = VIDEO_THEME_DEFAULT;
    }
    
    s_active_theme = theme;
    const video_palette_t *palette = video_get_theme_palette(theme);
    video_apply_palette(palette);
}

void video_wait_vblank(void) {
    graphicsWaitVerticalBlank();
}

void video_set_piece_sprite_swapped(uint8_t sprite_id, uint8_t swapped) {
    if (sprite_id >= VIDEO_SPRITE_PIECE_COUNT) {
        return;
    }
    
    uint32_t bitmap_addr;
    if (sprite_id < 8) {
        // Player A sprites
        bitmap_addr = swapped ? VIDEO_VRAM_PIECE_A_SWAPPED : VIDEO_VRAM_PIECE_A_NORMAL;
    } else {
        // Player B sprites  
        bitmap_addr = swapped ? VIDEO_VRAM_PIECE_B_SWAPPED : VIDEO_VRAM_PIECE_B_NORMAL;
    }
    
    uint8_t actual_sprite_id = (uint8_t)(VIDEO_SPRITE_PIECE_BASE + sprite_id);
    spriteDefine(actual_sprite_id, bitmap_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
}

void video_set_game_mode_icon_bitmap(bool is_puzzle_mode) {
    uint32_t bitmap_addr = is_puzzle_mode ? VIDEO_VRAM_ICON_GAME_MODE_PUZZLE : VIDEO_VRAM_ICON_GAME_MODE_AI;
    uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_ICON_BASE + VIDEO_ICON_GAME_MODE);
    // Calculate board layout
    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11; // Extra for border
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15; // Extra for border
    const int16_t board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    const int16_t board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;    
    spriteDefine(sprite_id, bitmap_addr, VIDEO_ICON_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
    spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + board_x + board_width+ 16, VIDEO_SPRITE_OFFSET + board_y + 8);
    spriteSetVisible(sprite_id, 1);
}

// Reset board cell CLUT to original checkerboard color
void video_reset_board_cell_color(uint8_t row, uint8_t col) {
    uint8_t clut_index = video_board_palette_index(row, col);
    const video_palette_t *palette = video_get_theme_palette(s_active_theme);
    const video_rgb_t *color = ((row + col) & 1) ? &palette->board_dark : &palette->board_light;
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, clut_index, color->r, color->g, color->b);
}

// Set board cell CLUT to win highlight color for a player
void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player) {
    uint8_t clut_index = video_board_palette_index(row, col);
    const video_palette_t *palette = video_get_theme_palette(s_active_theme);
    
    // Use different highlight colors for different players
    const video_rgb_t *color;
    if (player == PLAYER_WHITE) {
        color = &palette->highlight_primary;  // Player A (white) uses primary highlight
    } else {
        color = &palette->highlight_secondary; // Player B (black) uses secondary highlight
    }
    
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, clut_index, color->r, color->g, color->b);
}

// Reset all board cell CLUTs to original checkerboard colors
void video_reset_all_board_cell_colors(void) {
    const video_palette_t *palette = video_get_theme_palette(s_active_theme);
    
    for (uint8_t row = 0; row < VIDEO_BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < VIDEO_BOARD_COLUMNS; ++col) {
            uint8_t clut_index = video_board_palette_index(row, col);
            const video_rgb_t *color = ((row + col) & 1) ? &palette->board_dark : &palette->board_light;
            graphicsDefineColor(VIDEO_PRIMARY_CLUT, clut_index, color->r, color->g, color->b);
        }
    }
}
