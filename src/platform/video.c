#include "platform/video.h"

#include <stddef.h>

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

static video_config_t s_video_config = {
    .enable_double_buffer = 1,
    .front_bitmap_page = 0,
    .back_bitmap_page = 1,
};

static const video_palette_t kDefaultPalette = {
    .background = { .r = 0x10, .g = 0x16, .b = 0x1C },
    .board_light = { .r = 0x90, .g = 0xA8, .b = 0xC0 },
    .board_dark = { .r = 0x38, .g = 0x44, .b = 0x5A },
    .board_border = { .r = 0xC0, .g = 0xD8, .b = 0xF0 },
    .ui_panel = { .r = 0x22, .g = 0x28, .b = 0x36 },
    .highlight_primary = { .r = 0xF4, .g = 0xC2, .b = 0x44 },
    .highlight_secondary = { .r = 0xF0, .g = 0x7C, .b = 0x40 },
    .text_primary = { .r = 0xF4, .g = 0xF4, .b = 0xFA },
};

static video_palette_t s_palette_cache;

static void video_define_color(uint8_t slot, const video_rgb_t *color) {
    graphicsDefineColor(VIDEO_PRIMARY_CLUT, slot, color->r, color->g, color->b);
}

void video_init(const video_config_t *config) {
    if (config != NULL) {
        s_video_config = *config;
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

    graphicsReset();
    bitmapReset();
    spriteReset();

    video_apply_palette(NULL);

    bitmapSetActive(s_video_config.front_bitmap_page);
    bitmapSetCLUT(VIDEO_PRIMARY_CLUT);
    bitmapSetColor(VIDEO_SLOT_BACKGROUND);
    bitmapClear();
    bitmapSetVisible(s_video_config.front_bitmap_page, true);

    if (s_video_config.enable_double_buffer) {
        bitmapSetActive(s_video_config.back_bitmap_page);
        bitmapSetCLUT(VIDEO_PRIMARY_CLUT);
        bitmapSetColor(VIDEO_SLOT_BACKGROUND);
        bitmapClear();
        bitmapSetVisible(s_video_config.back_bitmap_page, false);
        bitmapSetActive(s_video_config.front_bitmap_page);
    }

    bitmapSetVisible(2, false);
}

void video_apply_palette(const video_palette_t *palette) {
    const video_palette_t *source = palette != NULL ? palette : &kDefaultPalette;

    s_palette_cache = *source;

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
    return manifest != NULL;
}

void video_wait_vblank(void) {
    graphicsWaitVerticalBlank();
}