#include "platform/video.h"

#include <stddef.h>
#include <string.h>

#include "assets/generated_assets.h"
#include "f_bitmap.h"
#include "f_graphics.h"
#include "f_sprite.h"

#define VIDEO_PRIMARY_CLUT 0

#define VIDEO_SLOT_BACKGROUND         0
#define VIDEO_SLOT_BOARD_LIGHT        1
#define VIDEO_SLOT_BOARD_DARK         2
#define VIDEO_SLOT_BOARD_BORDER       3
#define VIDEO_SLOT_UI_PANEL           4
#define VIDEO_SLOT_HIGHLIGHT_PRIMARY  5
#define VIDEO_SLOT_HIGHLIGHT_SECONDARY 6
#define VIDEO_SLOT_TEXT_PRIMARY       7

#define VIDEO_SCREEN_WIDTH 320u
#define VIDEO_SCREEN_HEIGHT 240u
#define VIDEO_BOARD_COLUMNS 4u
#define VIDEO_BOARD_ROWS 8u
#define VIDEO_BOARD_CELL_SIZE 28u
#define VIDEO_BOARD_BORDER 1u
#define VIDEO_BOARD_MARGIN 4u
#define VIDEO_UI_PANEL_GAP 16u

#define VIDEO_PIECE_SPRITE_SIZE 24u
#define VIDEO_ICON_SPRITE_SIZE 16u
#define VIDEO_HIGHLIGHT_SPRITE_SIZE 32u
#define VIDEO_HIGHLIGHT_SOURCE_SIZE 28u

#define VIDEO_VRAM_PIECE_BASE 0x040000u
#define VIDEO_VRAM_PIECE_STRIDE 0x000400u
#define VIDEO_VRAM_HIGHLIGHT_BASE (VIDEO_VRAM_PIECE_BASE + (VIDEO_PIECE_COUNT * VIDEO_VRAM_PIECE_STRIDE))
#define VIDEO_VRAM_HIGHLIGHT_STRIDE 0x000400u
#define VIDEO_VRAM_ICON_BASE (VIDEO_VRAM_HIGHLIGHT_BASE + VIDEO_VRAM_HIGHLIGHT_STRIDE)
#define VIDEO_VRAM_ICON_STRIDE 0x000200u

#define VIDEO_SPRITE_WHITE_BASE 0u
#define VIDEO_SPRITE_WHITE_COUNT 8u
#define VIDEO_SPRITE_BLACK_BASE 8u
#define VIDEO_SPRITE_BLACK_COUNT 8u
#define VIDEO_SPRITE_HIGHLIGHT 24u
#define VIDEO_SPRITE_ICON_BASE 32u

static video_config_t s_video_config = {
    .enable_double_buffer = 1,
    .front_bitmap_page = 0,
    .back_bitmap_page = 1,
    .theme = VIDEO_THEME_DEFAULT,
};

typedef struct {
    int board_x;
    int board_y;
    int board_width;
    int board_height;
    int ui_panel_x;
} video_board_layout_t;

typedef struct {
    uint32_t bitmap_page[3];
    uint32_t piece[VIDEO_PIECE_COUNT];
    uint32_t icon[VIDEO_ICON_COUNT];
    uint32_t highlight;
} video_vram_layout_t;

static const video_palette_t kThemePalettes[VIDEO_THEME_COUNT] = {
    [VIDEO_THEME_DEFAULT] = {
        .background = { .r = 0x10, .g = 0x16, .b = 0x1C },
        .board_light = { .r = 0x90, .g = 0xA8, .b = 0xC0 },
        .board_dark = { .r = 0x38, .g = 0x44, .b = 0x5A },
        .board_border = { .r = 0xC0, .g = 0xD8, .b = 0xF0 },
        .ui_panel = { .r = 0x22, .g = 0x28, .b = 0x36 },
        .highlight_primary = { .r = 0xF4, .g = 0xC2, .b = 0x44 },
        .highlight_secondary = { .r = 0xF0, .g = 0x7C, .b = 0x40 },
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
        .text_primary = { .r = 0xF0, .g = 0xFF, .b = 0xF0 },
    },
};

static video_theme_t s_active_theme = VIDEO_THEME_DEFAULT;

static video_palette_t s_palette_cache;
static video_asset_manifest_t s_manifest_cache;
static uint8_t s_board_bitmap[VIDEO_SCREEN_WIDTH * VIDEO_SCREEN_HEIGHT];
static video_board_layout_t s_board_layout;
static video_vram_layout_t s_vram_layout;
static const video_asset_manifest_t *s_assets = NULL;

static void video_generate_board_bitmap(void);
static void video_refresh_board_surface(void);
static video_board_layout_t video_compute_layout(void);
static void video_cache_bitmap_addresses(void);
static void video_initialize_vram_layout(void);
static void video_upload_board_bitmap(void);
static void video_upload_board_page(uint8_t page);
static void video_upload_buffer_to_vram(uint32_t destination, const uint8_t *source, uint32_t length);
static uint32_t video_read_bitmap_register(uint16_t address_low);
static void video_upload_sprite_assets(const video_asset_manifest_t *manifest);
static void video_expand_highlight_sprite(const uint8_t *source, uint8_t *dest);
static void video_stage_initial_scene(void);
static void video_position_initial_pieces(void);
static void video_position_menu_icons(void);

static void video_define_color(uint8_t slot, const video_rgb_t *color) {
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, slot, color->r, color->g, color->b);
}

void video_init(const video_config_t *config) {
    if (config != NULL) {
        s_video_config = *config;
    }

    if (s_video_config.theme >= VIDEO_THEME_COUNT) {
        s_video_config.theme = VIDEO_THEME_DEFAULT;
    }

    if (s_video_config.front_bitmap_page > 2) {
        s_video_config.front_bitmap_page = 0;
    }
    if (s_video_config.back_bitmap_page > 2) {
        s_video_config.back_bitmap_page = 1;
    }
    if (s_video_config.enable_double_buffer &&
        s_video_config.back_bitmap_page == s_video_config.front_bitmap_page) {
        s_video_config.back_bitmap_page = (s_video_config.front_bitmap_page + 1) % 3;
    }

    video_initialize_vram_layout();

    graphicsReset();
    bitmapReset();
    spriteReset();

    video_cache_bitmap_addresses();

    s_active_theme = s_video_config.theme;
    video_apply_theme(s_active_theme);

    bitmapSetActive(s_video_config.front_bitmap_page);
    bitmapSetCLUT(VIDEO_PRIMARY_CLUT);
    bitmapSetColor(VIDEO_SLOT_BACKGROUND);
    bitmapClear();
    bitmapSetVisible(s_video_config.front_bitmap_page, 1);

    if (s_video_config.enable_double_buffer) {
        bitmapSetActive(s_video_config.back_bitmap_page);
        bitmapSetCLUT(VIDEO_PRIMARY_CLUT);
        bitmapSetColor(VIDEO_SLOT_BACKGROUND);
        bitmapClear();
        bitmapSetVisible(s_video_config.back_bitmap_page, 0);
        bitmapSetActive(s_video_config.front_bitmap_page);
    }

    bitmapSetVisible(2, 0);

    if (s_assets == NULL) {
        video_load_assets(&g_video_assets);
    }
}

void video_apply_palette(const video_palette_t *palette) {
    const video_palette_t *fallback = video_get_theme_palette(s_active_theme);
    const video_palette_t *source = palette != NULL ? palette : fallback;

    memcpy(&s_palette_cache, source, sizeof(video_palette_t));

    video_define_color(VIDEO_SLOT_BACKGROUND, &source->background);
    video_define_color(VIDEO_SLOT_BOARD_LIGHT, &source->board_light);
    video_define_color(VIDEO_SLOT_BOARD_DARK, &source->board_dark);
    video_define_color(VIDEO_SLOT_BOARD_BORDER, &source->board_border);
    video_define_color(VIDEO_SLOT_UI_PANEL, &source->ui_panel);
    video_define_color(VIDEO_SLOT_HIGHLIGHT_PRIMARY, &source->highlight_primary);
    video_define_color(VIDEO_SLOT_HIGHLIGHT_SECONDARY, &source->highlight_secondary);
    video_define_color(VIDEO_SLOT_TEXT_PRIMARY, &source->text_primary);
}

uint8_t video_load_assets(const video_asset_manifest_t *manifest) {
    if (manifest == NULL) {
        return 0;
    }

    s_manifest_cache = *manifest;
    s_manifest_cache.board_bitmap = s_board_bitmap;
    s_manifest_cache.board_bitmap_size = sizeof(s_board_bitmap);

    s_assets = &s_manifest_cache;

    video_refresh_board_surface();
    video_upload_sprite_assets(s_assets);
    video_stage_initial_scene();

    return 1;
}

void video_wait_vblank(void) {
    graphicsWaitVerticalBlank();
}

void video_apply_theme(video_theme_t theme) {
    if (theme >= VIDEO_THEME_COUNT) {
        theme = VIDEO_THEME_DEFAULT;
    }

    s_active_theme = theme;
    const video_palette_t *palette = video_get_theme_palette(theme);
    video_apply_palette(palette);

    video_refresh_board_surface();
}

const video_palette_t *video_get_theme_palette(video_theme_t theme) {
    if (theme >= VIDEO_THEME_COUNT) {
        theme = VIDEO_THEME_DEFAULT;
    }

    return &kThemePalettes[theme];
}

const video_asset_manifest_t *video_get_assets(void) {
    return s_assets;
}

static void video_refresh_board_surface(void) {
    video_generate_board_bitmap();
    video_upload_board_bitmap();
    video_position_menu_icons();
}

static video_board_layout_t video_compute_layout(void) {
    video_board_layout_t layout;
    layout.board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE;
    layout.board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE;
    layout.board_x = (VIDEO_SCREEN_WIDTH - layout.board_width) / 2;
    layout.board_y = (VIDEO_SCREEN_HEIGHT - layout.board_height) / 2;
    layout.ui_panel_x = layout.board_x + layout.board_width + VIDEO_UI_PANEL_GAP;
    return layout;
}

static void video_cache_bitmap_addresses(void) {
    s_vram_layout.bitmap_page[0] = video_read_bitmap_register(VKY_BM0_ADDR_L);
    s_vram_layout.bitmap_page[1] = video_read_bitmap_register(VKY_BM1_ADDR_L);
    s_vram_layout.bitmap_page[2] = video_read_bitmap_register(VKY_BM2_ADDR_L);
}

static void video_initialize_vram_layout(void) {
    for (uint32_t index = 0; index < VIDEO_PIECE_COUNT; ++index) {
        s_vram_layout.piece[index] = VIDEO_VRAM_PIECE_BASE + (index * VIDEO_VRAM_PIECE_STRIDE);
    }
    s_vram_layout.highlight = VIDEO_VRAM_HIGHLIGHT_BASE;
    for (uint32_t index = 0; index < VIDEO_ICON_COUNT; ++index) {
        s_vram_layout.icon[index] = VIDEO_VRAM_ICON_BASE + (index * VIDEO_VRAM_ICON_STRIDE);
    }
}

static uint32_t video_read_bitmap_register(uint16_t address_low) {
    const uint32_t low = (uint32_t)PEEK(address_low);
    const uint32_t mid = (uint32_t)PEEK(address_low + 1u);
    const uint32_t high = (uint32_t)(PEEK(address_low + 2u) & 0x03u);
    return low | (mid << 8) | (high << 16);
}

static void video_upload_board_bitmap(void) {
    video_upload_board_page(s_video_config.front_bitmap_page);
    if (s_video_config.enable_double_buffer) {
        video_upload_board_page(s_video_config.back_bitmap_page);
    }
}

static void video_upload_board_page(uint8_t page) {
    if (page >= 3u) {
        return;
    }

    const uint32_t destination = s_vram_layout.bitmap_page[page];
    if (destination == 0u) {
        return;
    }

    video_upload_buffer_to_vram(destination, s_board_bitmap, sizeof(s_board_bitmap));
}

static void video_upload_buffer_to_vram(uint32_t destination, const uint8_t *source, uint32_t length) {
    if (source == NULL || length == 0u) {
        return;
    }

    SWAP_IO_SETUP();

    while (length > 0u) {
        const uint8_t bank = (uint8_t)(destination / EIGHTK);
        const uint16_t offset = (uint16_t)(destination & (EIGHTK - 1u));
        uint16_t chunk = (uint16_t)(EIGHTK - offset);
        if (chunk > length) {
            chunk = (uint16_t)length;
        }

        POKE(SWAP_SLOT, bank);
        volatile uint8_t *const window = (volatile uint8_t *)SWAP_ADDR;
        const uint8_t *const chunk_source = source;
        for (uint16_t index = 0u; index < chunk; ++index) {
            window[offset + index] = chunk_source[index];
        }

        destination += chunk;
        source += chunk;
        length -= chunk;
    }

    SWAP_RESTORE_SLOT();
    SWAP_IO_SHUTDOWN();
}

static void video_expand_highlight_sprite(const uint8_t *source, uint8_t *dest) {
    const uint32_t total_size = VIDEO_HIGHLIGHT_SPRITE_SIZE * VIDEO_HIGHLIGHT_SPRITE_SIZE;
    memset(dest, VIDEO_SLOT_BACKGROUND, total_size);

    if (source == NULL) {
        return;
    }

    for (uint16_t row = 0u; row < VIDEO_HIGHLIGHT_SOURCE_SIZE; ++row) {
        const uint32_t dest_index = ((uint32_t)(row + 2u) * VIDEO_HIGHLIGHT_SPRITE_SIZE) + 2u;
        memcpy(&dest[dest_index], &source[row * VIDEO_HIGHLIGHT_SOURCE_SIZE], VIDEO_HIGHLIGHT_SOURCE_SIZE);
    }
}

static void video_upload_sprite_assets(const video_asset_manifest_t *manifest) {
    if (manifest == NULL) {
        return;
    }

    if (manifest->piece_sprite_size > 0u) {
        for (uint32_t index = 0; index < VIDEO_PIECE_COUNT; ++index) {
            const uint8_t *payload = manifest->piece_sprites[index];
            if (payload != NULL) {
                video_upload_buffer_to_vram(s_vram_layout.piece[index], payload, manifest->piece_sprite_size);
            }
        }
    }

    if (manifest->highlight_frame != NULL && manifest->highlight_frame_size > 0u) {
        uint8_t expanded[VIDEO_HIGHLIGHT_SPRITE_SIZE * VIDEO_HIGHLIGHT_SPRITE_SIZE];
        video_expand_highlight_sprite(manifest->highlight_frame, expanded);
        video_upload_buffer_to_vram(s_vram_layout.highlight, expanded, sizeof(expanded));
    }

    if (manifest->menu_icon_size > 0u) {
        for (uint32_t index = 0; index < VIDEO_ICON_COUNT; ++index) {
            const uint8_t *payload = manifest->menu_icons[index];
            if (payload != NULL) {
                video_upload_buffer_to_vram(s_vram_layout.icon[index], payload, manifest->menu_icon_size);
            }
        }
    }
}

static void video_stage_initial_scene(void) {
    if (s_assets == NULL) {
        return;
    }

    const uint32_t white_normal_addr = s_vram_layout.piece[VIDEO_PIECE_WHITE_NORMAL];
    const uint32_t black_normal_addr = s_vram_layout.piece[VIDEO_PIECE_BLACK_NORMAL];

    for (uint8_t index = 0u; index < VIDEO_SPRITE_WHITE_COUNT; ++index) {
        spriteDefine(VIDEO_SPRITE_WHITE_BASE + index, white_normal_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 0);
        spriteSetVisible(VIDEO_SPRITE_WHITE_BASE + index, 0);
    }

    for (uint8_t index = 0u; index < VIDEO_SPRITE_BLACK_COUNT; ++index) {
        spriteDefine(VIDEO_SPRITE_BLACK_BASE + index, black_normal_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetVisible(VIDEO_SPRITE_BLACK_BASE + index, 0);
    }

    for (uint8_t index = 0u; index < VIDEO_ICON_COUNT; ++index) {
        const uint8_t sprite_id = (uint8_t)(VIDEO_SPRITE_ICON_BASE + index);
        spriteDefine(sprite_id, s_vram_layout.icon[index], VIDEO_ICON_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 3);
        spriteSetVisible(sprite_id, 1);
    }

    spriteDefine(VIDEO_SPRITE_HIGHLIGHT, s_vram_layout.highlight, VIDEO_HIGHLIGHT_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 2);
    spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT, 0);
    spriteSetPosition(VIDEO_SPRITE_HIGHLIGHT, (uint16_t)(s_board_layout.board_x - 2), (uint16_t)(s_board_layout.board_y - 2));

    video_position_initial_pieces();
    video_position_menu_icons();
}

static void video_position_initial_pieces(void) {
    const uint16_t cell_offset = (uint16_t)((VIDEO_BOARD_CELL_SIZE - VIDEO_PIECE_SPRITE_SIZE) / 2u);
    const uint16_t board_x = (uint16_t)s_board_layout.board_x;
    const uint16_t board_y = (uint16_t)s_board_layout.board_y;

    uint8_t sprite_id = VIDEO_SPRITE_WHITE_BASE;
    for (uint8_t row_index = 0u; row_index < 2u; ++row_index) {
        const uint8_t row = (uint8_t)((VIDEO_BOARD_ROWS - 2u) + row_index);
        for (uint8_t col = 0u; col < VIDEO_BOARD_COLUMNS; ++col) {
            const uint16_t x = (uint16_t)(board_x + (col * VIDEO_BOARD_CELL_SIZE) + cell_offset);
            const uint16_t y = (uint16_t)(board_y + (row * VIDEO_BOARD_CELL_SIZE) + cell_offset);
            spriteSetPosition(sprite_id, x, y);
            spriteSetVisible(sprite_id, 1);
            sprite_id++;
        }
    }

    sprite_id = VIDEO_SPRITE_BLACK_BASE;
    for (uint8_t row = 0u; row < 2u; ++row) {
        for (uint8_t col = 0u; col < VIDEO_BOARD_COLUMNS; ++col) {
            const uint16_t x = (uint16_t)(board_x + (col * VIDEO_BOARD_CELL_SIZE) + cell_offset);
            const uint16_t y = (uint16_t)(board_y + (row * VIDEO_BOARD_CELL_SIZE) + cell_offset);
            spriteSetPosition(sprite_id, x, y);
            spriteSetVisible(sprite_id, 1);
            sprite_id++;
        }
    }
}

static void video_position_menu_icons(void) {
    if (s_assets == NULL) {
        return;
    }

    const uint16_t icon_x = (uint16_t)(s_board_layout.ui_panel_x + 8);
    const uint16_t start_y = (uint16_t)(s_board_layout.board_y + 8);
    const uint16_t spacing = VIDEO_ICON_SPRITE_SIZE + 8u;

    for (uint8_t index = 0u; index < VIDEO_ICON_COUNT; ++index) {
        const uint16_t y = (uint16_t)(start_y + (index * spacing));
        spriteSetPosition((uint8_t)(VIDEO_SPRITE_ICON_BASE + index), icon_x, y);
    }
}

static void video_generate_board_bitmap(void) {
    s_board_layout = video_compute_layout();

    const int width = VIDEO_SCREEN_WIDTH;
    const int height = VIDEO_SCREEN_HEIGHT;
    const int board_x = s_board_layout.board_x;
    const int board_y = s_board_layout.board_y;
    const int board_width = s_board_layout.board_width;
    const int board_height = s_board_layout.board_height;
    const int ui_panel_x = s_board_layout.ui_panel_x;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t color = VIDEO_SLOT_BACKGROUND;

            if (x >= ui_panel_x) {
                color = VIDEO_SLOT_UI_PANEL;
            } else if (y < (board_y - VIDEO_BOARD_MARGIN) ||
                       y >= (board_y + board_height + VIDEO_BOARD_MARGIN) ||
                       x < (board_x - VIDEO_BOARD_MARGIN) ||
                       x >= (board_x + board_width + VIDEO_BOARD_MARGIN)) {
                color = VIDEO_SLOT_BACKGROUND;
            } else if ((x >= (board_x - VIDEO_BOARD_BORDER) &&
                        x < (board_x + board_width + VIDEO_BOARD_BORDER) &&
                        ((y == (board_y - VIDEO_BOARD_BORDER)) || (y == (board_y + board_height)))) ||
                       (y >= (board_y - VIDEO_BOARD_BORDER) &&
                        y < (board_y + board_height + VIDEO_BOARD_BORDER) &&
                        ((x == (board_x - VIDEO_BOARD_BORDER)) || (x == (board_x + board_width))))) {
                color = VIDEO_SLOT_BOARD_BORDER;
            } else if (x >= board_x && x < (board_x + board_width) &&
                       y >= board_y && y < (board_y + board_height)) {
                const int cell_x = (x - board_x) / VIDEO_BOARD_CELL_SIZE;
                const int cell_y = (y - board_y) / VIDEO_BOARD_CELL_SIZE;
                const int parity = (cell_x + cell_y) & 1;
                color = parity == 0 ? VIDEO_SLOT_BOARD_LIGHT : VIDEO_SLOT_BOARD_DARK;
            }

            s_board_bitmap[y * width + x] = color;
        }
    }
}