#include "platform/system.h"

#include "platform/video.h"

static void platform_init_video(void) {
    const video_config_t config = {
        .enable_double_buffer = 1,
        .front_bitmap_page = 0,
        .back_bitmap_page = 1,
        .theme = VIDEO_THEME_DEFAULT,
    };

    video_init(&config);
}

static void platform_init_input(void) {
    // TODO: Initialize keyboard and mouse subsystems.
}

static void platform_init_audio(void) {
    // TODO: Initialize PSG/OPL audio hardware.
}

void platform_bootstrap(void) {
    platform_init_video();
    platform_init_input();
    platform_init_audio();
}

void platform_idle(void) {
    // Placeholder that will later process events and maintain timing.
    for (;;) {
        // Break immediately until the real loop is implemented.
        break;
    }
}

void platform_shutdown(void) {
    // TODO: Perform any necessary hardware shutdown or memory cleanup.
}
