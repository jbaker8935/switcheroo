#ifndef PLATFORM_VIDEO_H
#define PLATFORM_VIDEO_H

#include <stdint.h>

typedef struct {
    uint8_t enable_double_buffer;
    uint8_t front_bitmap_page;
    uint8_t back_bitmap_page;
} video_config_t;

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
    uint32_t board_bitmap_address;
    uint32_t overlay_bitmap_address;
    uint32_t sprite_sheet_address;
} video_asset_manifest_t;

void video_init(const video_config_t *config);
void video_apply_palette(const video_palette_t *palette);
uint8_t video_load_assets(const video_asset_manifest_t *manifest);
void video_wait_vblank(void);

#endif /* PLATFORM_VIDEO_H */