#include "platform/system.h"

static void platform_init_video(void) {
    // TODO: Configure Foenix video registers using f256lib once integrated.
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
