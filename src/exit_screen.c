#include "f256lib.h"
#include "sram_assets.h"
#include "video.h"
#include "text_display.h"
#include "mouse_pointer.h"
#include "dma_copy.h"
#include "playsid.h"
#include "overlay_config.h"

#include <stdint.h>

#pragma code(ovl13_code)
void FAR_video_show_exit(void) {
    
    for(uint32_t i = 0; i < 76800u; ++i) {
        FAR_POKE(SRAM_SPLASH_BASE + i, 11);
    }

    
    // Copy in Logo Screen Start x 113 y 24 w 100 h 104
    // uint16_t byte_offset = 0;
    // for (uint16_t row= 0; row < 104; row++) {
    //     for (uint16_t col = 0; col < 100; col++) {
    //         uint8_t byte = FAR_PEEK(SRAM_NOT_A_THING + byte_offset);
    //         uint16_t screen_x = 113 + col;
    //         uint16_t screen_y = 24 + row;
    //         uint32_t addr_offset = screen_y * 320 + screen_x;
    //         FAR_POKE(SRAM_SPLASH_BASE + addr_offset, byte);
    //         byte_offset++;
    //     }
    // } 

    dma2dCopy(SRAM_SPLASH_BASE +  (320 * 24) + 113, SRAM_NOT_A_THING, 100, 104, 100, 320);
    
    // Set bitmap address to splash data

    spriteReset();

    clear_text_matrix();

    // exit occurs when User is on Main screen and Menu is in Palette CLUT 2
    // setup clut 0xDC00

    // XXX GAMMA  SPRITE   TILE  | BITMAP  GRAPH  OVRLY  TEXT
    POKE(VKY_MSTR_CTRL_0, 0b00001111); // bitmap, graph, overlay, text enabled 
    // XXX XXX  FON_SET FON_OVLY | MON_SLP DBL_Y  DBL_X  CLK_70
    POKE(VKY_MSTR_CTRL_1, 0b00000000); // 320x240 at 60 Hz with font overlay
    
    graphicsSetLayerBitmap(VIDEO_SPLASH_PAGE, 0);

    bitmapSetActive(VIDEO_SPLASH_PAGE);
    
    bitmapSetCLUT(VIDEO_MENU_CLUT);
    
    bitmapSetVisible(1, false);
    bitmapSetVisible(2, false);
    
    bitmapSetAddress(VIDEO_SPLASH_PAGE, SRAM_SPLASH_BASE);
    bitmapSetVisible(VIDEO_SPLASH_PAGE, true);
    
    print_formatted_text(24, 38, "^4Switch^5eroo^1 a ^6Not a Thing^1 game");
    print_formatted_text(22, 40, "^1Various rights reserved and so forth^1");
    print_formatted_text(36, 42, "^6Credits^1"); 
    print_formatted_text(28, 44, "^2Addy's 'Switcheroo' Game^1");
    print_formatted_text(28, 46, "^2Developed by jbaker8935^1");
    print_formatted_text(16, 48, "^2Special Thanks to the Wildbits Discord Community^1");
    print_formatted_text(26, 50, "^2Powered by LLVM-MOS F256 SDK^1");
    print_formatted_text(31, 54, "^4Thanks ^1for ^5Playing^1");
    
    disable_mouse();
}
#pragma code(code)

void video_show_exit(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_video_show_exit();
    POKE(OVERLAY_MMU_REG, saved);
}
