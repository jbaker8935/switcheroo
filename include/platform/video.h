#ifndef PLATFORM_VIDEO_H
#define PLATFORM_VIDEO_H

#include <stdint.h>

typedef enum {
    VIDEO_THEME_DEFAULT = 0,
    VIDEO_THEME_HIGH_CONTRAST = 1,
    VIDEO_THEME_COLORBLIND = 2,
    VIDEO_THEME_COUNT
} video_theme_t;

typedef enum {
    VIDEO_PIECE_WHITE_NORMAL = 0,
    VIDEO_PIECE_WHITE_SWAPPED = 1,
    VIDEO_PIECE_BLACK_NORMAL = 2,
    VIDEO_PIECE_BLACK_SWAPPED = 3,
    VIDEO_PIECE_COUNT
} video_piece_id_t;

typedef enum {
    VIDEO_ICON_RESET = 0,
    VIDEO_ICON_INFO = 1,
    VIDEO_ICON_DIFFICULTY = 2,
    VIDEO_ICON_STARTING_BOARD = 3,
    VIDEO_ICON_HISTORY = 4,
    VIDEO_ICON_EXIT = 5,
    VIDEO_ICON_COUNT
} video_icon_id_t;

typedef struct {
    uint8_t enable_double_buffer;
    uint8_t front_bitmap_page;
    uint8_t back_bitmap_page;
    video_theme_t theme;
} video_config_t;

#define VIDEO_BOARD_COLUMNS 4u
#define VIDEO_BOARD_ROWS 8u
#define VIDEO_BOARD_CELL_COUNT (VIDEO_BOARD_COLUMNS * VIDEO_BOARD_ROWS)

#define VIDEO_CLUT_TRANSPARENT 0u
#define VIDEO_CLUT_BACKGROUND 1u
#define VIDEO_CLUT_BOARD_BORDER 2u
#define VIDEO_CLUT_UI_PANEL 3u
#define VIDEO_CLUT_TEXT_PRIMARY 4u
#define VIDEO_CLUT_BOARD_BASE 5u
#define VIDEO_CLUT_BOARD_COUNT VIDEO_BOARD_CELL_COUNT
#define VIDEO_CLUT_HIGHLIGHT_PRIMARY (VIDEO_CLUT_BOARD_BASE + VIDEO_CLUT_BOARD_COUNT)
#define VIDEO_CLUT_HIGHLIGHT_SECONDARY (VIDEO_CLUT_HIGHLIGHT_PRIMARY + 1u)
#define VIDEO_CLUT_HIGHLIGHT_DISABLED (VIDEO_CLUT_HIGHLIGHT_PRIMARY + 2u)

static inline uint8_t video_board_palette_index(uint8_t row, uint8_t column) {
    return (uint8_t)(VIDEO_CLUT_BOARD_BASE + (row * VIDEO_BOARD_COLUMNS) + column);
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} video_rgb_t;

typedef struct {
    video_rgb_t background;
    video_rgb_t board_light;
    video_rgb_t board_dark;
    video_rgb_t board_border;
    video_rgb_t ui_panel;
    video_rgb_t highlight_primary;
    video_rgb_t highlight_secondary;
    video_rgb_t text_primary;
} video_palette_t;

typedef struct {
    const uint8_t *highlight_frame;
    uint32_t highlight_frame_size;
    const uint8_t *move_indicator;
    uint32_t move_indicator_size;
    const uint8_t *piece_sprites[VIDEO_PIECE_COUNT];
    uint32_t piece_sprite_size;
    const uint8_t *menu_icons[VIDEO_ICON_COUNT];
    uint32_t menu_icon_size;
} video_asset_manifest_t;

void video_init(const video_config_t *config);
void video_apply_palette(const video_palette_t *palette);
void video_apply_theme(video_theme_t theme);
const video_palette_t *video_get_theme_palette(video_theme_t theme);
uint8_t video_load_assets(const video_asset_manifest_t *manifest);
const video_asset_manifest_t *video_get_assets(void);
void video_wait_vblank(void);

#endif /* PLATFORM_VIDEO_H */