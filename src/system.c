#include "platform_f256.h"
#include <stdint.h>

// Simple video initialization without complex config
static void platform_init_video(void) {
    // Set I/O page to 0 for video register access
    POKE(MMU_IO_CTRL, 0);
    
    // XXX GAMMA  SPRITE   TILE  | BITMAP  GRAPH  OVRLY  TEXT
    POKE(VKY_MSTR_CTRL_0, 0b00101111); // Enable sprite, graph, overlay, text
    
    // XXX XXX  FON_SET FON_OVLY | MON_SLP DBL_Y  DBL_X  CLK_70
    POKE(VKY_MSTR_CTRL_1, 0b00000100); // Font overlay, 320x240 at 60 Hz
    
    // Layer control
    POKE(VKY_LAYER_CTRL_0, 0b00000001); // Bitmap 1 in layer 0, bitmap 0 in layer 1
    POKE(VKY_LAYER_CTRL_1, 0b00000010); // Bitmap 2 in layer 2
    
    // Force black graphics background
    POKE(0xD00D, 0x00);
    POKE(0xD00E, 0x00);
    POKE(0xD00F, 0x00);
    
    // Set up bitmap for board display
    bitmapSetActive(0);
    bitmapSetAddress(0, 0x44000); // Board bitmap address
    bitmapSetCLUT(0);
    bitmapSetVisible(0, true);
    bitmapSetVisible(1, false);
    bitmapSetVisible(2, false);
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
