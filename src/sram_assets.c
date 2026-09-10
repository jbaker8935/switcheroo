// Asset embedding for oscar64 — each EMBED becomes a section+region+#embed block
// placing binary data at exact far memory addresses for the PGZ loader.

#include "sram_assets.h"

// --- Bitmaps (76800 bytes each) ---

#pragma section(sa00, 0)
#pragma region(sa00, 0x20000, 0x32C00, , , {sa00})
#pragma data(sa00)
__export const char sa00_d[] = {
	#embed 76800 "../assets/ui/ui_board.bin"
};
#pragma data(data)

#pragma section(sa01, 0)
#pragma region(sa01, 0x32C00, 0x45800, , , {sa01})
#pragma data(sa01)
__export const char sa01_d[] = {
	#embed 76800 "../assets/ui/ui_splash.bin"
};
#pragma data(data)

#pragma section(sa02, 0)
#pragma region(sa02, 0x45800, 0x58400, , , {sa02})
#pragma data(sa02)
__export const char sa02_d[] = {
	#embed 76800 "../assets/ui/ui_achievements.bin"
};
#pragma data(data)

// --- Palettes (1024 bytes each) ---

#pragma section(sa03, 0)
#pragma region(sa03, 0x58400, 0x58800, , , {sa03})
#pragma data(sa03)
__export const char sa03_d[] = {
	#embed "../assets/ui/ui_board_palette.bin"
};
#pragma data(data)

#pragma section(sa04, 0)
#pragma region(sa04, 0x58800, 0x58C00, , , {sa04})
#pragma data(sa04)
__export const char sa04_d[] = {
	#embed "../assets/ui/ui_pieces_palette.bin"
};
#pragma data(data)

#pragma section(sa05, 0)
#pragma region(sa05, 0x58C00, 0x59000, , , {sa05})
#pragma data(sa05)
__export const char sa05_d[] = {
	#embed "../assets/ui/ui_menu_palette.bin"
};
#pragma data(data)

#pragma section(sa06, 0)
#pragma region(sa06, 0x59000, 0x59400, , , {sa06})
#pragma data(sa06)
__export const char sa06_d[] = {
	#embed "../assets/ui/ui_splash_palette.bin"
};
#pragma data(data)

#pragma section(sa07, 0)
#pragma region(sa07, 0x59400, 0x59800, , , {sa07})
#pragma data(sa07)
__export const char sa07_d[] = {
	#embed "../assets/ui/ui_achieve_base_palette.bin"
};
#pragma data(data)

#pragma section(sa08, 0)
#pragma region(sa08, 0x59800, 0x59C00, , , {sa08})
#pragma data(sa08)
__export const char sa08_d[] = {
	#embed "../assets/ui/ui_achieve_color_palette.bin"
};
#pragma data(data)

#pragma section(sa09, 0)
#pragma region(sa09, 0x59C00, 0x5A000, , , {sa09})
#pragma data(sa09)
__export const char sa09_d[] = {
	#embed "../assets/ui/ui_achieve_grey_palette.bin"
};
#pragma data(data)

// --- Menu Icons (1024 bytes each) ---

#pragma section(sa10, 0)
#pragma region(sa10, 0x5A000, 0x5A400, , , {sa10})
#pragma data(sa10)
__export const char sa10_d[] = {
	#embed "../assets/ui/ui_menu_robot_32x32.bin"
};
#pragma data(data)

#pragma section(sa11, 0)
#pragma region(sa11, 0x5A400, 0x5A800, , , {sa11})
#pragma data(sa11)
__export const char sa11_d[] = {
	#embed "../assets/ui/ui_menu_puzzle_32x32.bin"
};
#pragma data(data)

#pragma section(sa12, 0)
#pragma region(sa12, 0x5A800, 0x5AC00, , , {sa12})
#pragma data(sa12)
__export const char sa12_d[] = {
	#embed "../assets/ui/ui_menu_retry_32x32.bin"
};
#pragma data(data)

#pragma section(sa13, 0)
#pragma region(sa13, 0x5AC00, 0x5B000, , , {sa13})
#pragma data(sa13)
__export const char sa13_d[] = {
	#embed "../assets/ui/ui_menu_left_32x32.bin"
};
#pragma data(data)

#pragma section(sa14, 0)
#pragma region(sa14, 0x5B000, 0x5B400, , , {sa14})
#pragma data(sa14)
__export const char sa14_d[] = {
	#embed "../assets/ui/ui_menu_right_32x32.bin"
};
#pragma data(data)

#pragma section(sa15, 0)
#pragma region(sa15, 0x5B400, 0x5B800, , , {sa15})
#pragma data(sa15)
__export const char sa15_d[] = {
	#embed "../assets/ui/ui_menu_swap_mode_32x32.bin"
};
#pragma data(data)

#pragma section(sa16, 0)
#pragma region(sa16, 0x5B800, 0x5BC00, , , {sa16})
#pragma data(sa16)
__export const char sa16_d[] = {
	#embed "../assets/ui/ui_menu_difficulty_32x32.bin"
};
#pragma data(data)

#pragma section(sa17, 0)
#pragma region(sa17, 0x5BC00, 0x5C000, , , {sa17})
#pragma data(sa17)
__export const char sa17_d[] = {
	#embed "../assets/ui/ui_menu_hint_32x32.bin"
};
#pragma data(data)

#pragma section(sa18, 0)
#pragma region(sa18, 0x5C000, 0x5C400, , , {sa18})
#pragma data(sa18)
__export const char sa18_d[] = {
	#embed "../assets/ui/ui_menu_exit_32x32.bin"
};
#pragma data(data)

// --- Achievement Sprites (576 bytes each) ---

#pragma section(sa19, 0)
#pragma region(sa19, 0x5C400, 0x5C640, , , {sa19})
#pragma data(sa19)
__export const char sa19_d[] = {
	#embed "../assets/ui/award.bin"
};
#pragma data(data)

#pragma section(sa20, 0)
#pragma region(sa20, 0x5C640, 0x5C880, , , {sa20})
#pragma data(sa20)
__export const char sa20_d[] = {
	#embed "../assets/ui/achieve_100.bin"
};
#pragma data(data)

#pragma section(sa21, 0)
#pragma region(sa21, 0x5C880, 0x5CAC0, , , {sa21})
#pragma data(sa21)
__export const char sa21_d[] = {
	#embed "../assets/ui/runner.bin"
};
#pragma data(data)

#pragma section(sa22, 0)
#pragma region(sa22, 0x5CAC0, 0x5CD00, , , {sa22})
#pragma data(sa22)
__export const char sa22_d[] = {
	#embed "../assets/ui/brain.bin"
};
#pragma data(data)

#pragma section(sa23, 0)
#pragma region(sa23, 0x5CD00, 0x5CF40, , , {sa23})
#pragma data(sa23)
__export const char sa23_d[] = {
	#embed "../assets/ui/achieve_puzzle.bin"
};
#pragma data(data)

#pragma section(sa24, 0)
#pragma region(sa24, 0x5CF40, 0x5D180, , , {sa24})
#pragma data(sa24)
__export const char sa24_d[] = {
	#embed "../assets/ui/thinker.bin"
};
#pragma data(data)

#pragma section(sa25, 0)
#pragma region(sa25, 0x5D180, 0x5D3C0, , , {sa25})
#pragma data(sa25)
__export const char sa25_d[] = {
	#embed "../assets/ui/lightning.bin"
};
#pragma data(data)

#pragma section(sa26, 0)
#pragma region(sa26, 0x5D3C0, 0x5D600, , , {sa26})
#pragma data(sa26)
__export const char sa26_d[] = {
	#embed "../assets/ui/bullseye.bin"
};
#pragma data(data)

#pragma section(sa27, 0)
#pragma region(sa27, 0x5D600, 0x5D840, , , {sa27})
#pragma data(sa27)
__export const char sa27_d[] = {
	#embed "../assets/ui/sword.bin"
};
#pragma data(data)

#pragma section(sa28, 0)
#pragma region(sa28, 0x5D840, 0x5DA80, , , {sa28})
#pragma data(sa28)
__export const char sa28_d[] = {
	#embed "../assets/ui/arm_flex.bin"
};
#pragma data(data)

#pragma section(sa29, 0)
#pragma region(sa29, 0x5DA80, 0x5DCC0, , , {sa29})
#pragma data(sa29)
__export const char sa29_d[] = {
	#embed "../assets/ui/dice.bin"
};
#pragma data(data)

#pragma section(sa30, 0)
#pragma region(sa30, 0x5DCC0, 0x5DF00, , , {sa30})
#pragma data(sa30)
__export const char sa30_d[] = {
	#embed "../assets/ui/flame.bin"
};
#pragma data(data)

#pragma section(sa31, 0)
#pragma region(sa31, 0x5DF00, 0x5E140, , , {sa31})
#pragma data(sa31)
__export const char sa31_d[] = {
	#embed "../assets/ui/medal.bin"
};
#pragma data(data)

#pragma section(sa32, 0)
#pragma region(sa32, 0x5E140, 0x5E380, , , {sa32})
#pragma data(sa32)
__export const char sa32_d[] = {
	#embed "../assets/ui/crown.bin"
};
#pragma data(data)

// --- Piece Sprites (576 bytes each) ---

#pragma section(sa33, 0)
#pragma region(sa33, 0x5E380, 0x5E5C0, , , {sa33})
#pragma data(sa33)
__export const char sa33_d[] = {
	#embed "../assets/ui/playerA_normal_light.bin"
};
#pragma data(data)

#pragma section(sa34, 0)
#pragma region(sa34, 0x5E5C0, 0x5E800, , , {sa34})
#pragma data(sa34)
__export const char sa34_d[] = {
	#embed "../assets/ui/playerA_swapped_light.bin"
};
#pragma data(data)

#pragma section(sa35, 0)
#pragma region(sa35, 0x5E800, 0x5EA40, , , {sa35})
#pragma data(sa35)
__export const char sa35_d[] = {
	#embed "../assets/ui/playerB_normal_light.bin"
};
#pragma data(data)

#pragma section(sa36, 0)
#pragma region(sa36, 0x5EA40, 0x5EC80, , , {sa36})
#pragma data(sa36)
__export const char sa36_d[] = {
	#embed "../assets/ui/playerB_swapped_light.bin"
};
#pragma data(data)

#pragma section(sa37, 0)
#pragma region(sa37, 0x5EC80, 0x5EEC0, , , {sa37})
#pragma data(sa37)
__export const char sa37_d[] = {
	#embed "../assets/ui/playerA_normal_dark.bin"
};
#pragma data(data)

#pragma section(sa38, 0)
#pragma region(sa38, 0x5EEC0, 0x5F100, , , {sa38})
#pragma data(sa38)
__export const char sa38_d[] = {
	#embed "../assets/ui/playerA_swapped_dark.bin"
};
#pragma data(data)

#pragma section(sa39, 0)
#pragma region(sa39, 0x5F100, 0x5F340, , , {sa39})
#pragma data(sa39)
__export const char sa39_d[] = {
	#embed "../assets/ui/playerB_normal_dark.bin"
};
#pragma data(data)

#pragma section(sa40, 0)
#pragma region(sa40, 0x5F340, 0x5F580, , , {sa40})
#pragma data(sa40)
__export const char sa40_d[] = {
	#embed "../assets/ui/playerB_swapped_dark.bin"
};
#pragma data(data)

// --- Highlights and Focus (576 or 1024 bytes) ---

#pragma section(sa41, 0)
#pragma region(sa41, 0x5F580, 0x5F7C0, , , {sa41})
#pragma data(sa41)
__export const char sa41_d[] = {
	#embed "../assets/ui/highlight_empty.bin"
};
#pragma data(data)

#pragma section(sa42, 0)
#pragma region(sa42, 0x5F7C0, 0x5FA00, , , {sa42})
#pragma data(sa42)
__export const char sa42_d[] = {
	#embed "../assets/ui/highlight_occupied.bin"
};
#pragma data(data)

#pragma section(sa43, 0)
#pragma region(sa43, 0x5FA00, 0x5FE00, , , {sa43})
#pragma data(sa43)
__export const char sa43_d[] = {
	#embed "../assets/ui/cell_focus.bin"
};
#pragma data(data)

// --- MP3 Sounds ---

#pragma section(sa44, 0)
#pragma region(sa44, 0x5FE00, 0x61630, , , {sa44})
#pragma data(sa44)
__export const char sa44_d[] = {
	#embed "../assets/sounds/loss.mp3"
};
#pragma data(data)

#pragma section(sa45, 0)
#pragma region(sa45, 0x61630, 0x62E60, , , {sa45})
#pragma data(sa45)
__export const char sa45_d[] = {
	#embed "../assets/sounds/move.mp3"
};
#pragma data(data)

#pragma section(sa46, 0)
#pragma region(sa46, 0x62E60, 0x64690, , , {sa46})
#pragma data(sa46)
__export const char sa46_d[] = {
	#embed "../assets/sounds/reset_board.mp3"
};
#pragma data(data)

#pragma section(sa47, 0)
#pragma region(sa47, 0x64690, 0x65EC0, , , {sa47})
#pragma data(sa47)
__export const char sa47_d[] = {
	#embed "../assets/sounds/win.mp3"
};
#pragma data(data)

#pragma section(sa48, 0)
#pragma region(sa48, 0x65EC0, 0x66890, , , {sa48})
#pragma data(sa48)
__export const char sa48_d[] = {
	#embed "../assets/sounds/game_start_sound.mp3"
};
#pragma data(data)

// --- SID Sounds ---

#pragma section(sa49, 0)
#pragma region(sa49, 0x66890, 0x685E0, , , {sa49})
#pragma data(sa49)
__export const char sa49_d[] = {
	#embed "../assets/sounds/intro.bin"
};
#pragma data(data)

#pragma section(sa50, 0)
#pragma region(sa50, 0x685E0, 0x688A0, , , {sa50})
#pragma data(sa50)
__export const char sa50_d[] = {
	#embed "../assets/sounds/loaded.bin"
};
#pragma data(data)

#pragma section(sa51, 0)
#pragma region(sa51, 0x688A0, 0x68E80, , , {sa51})
#pragma data(sa51)
__export const char sa51_d[] = {
	#embed "../assets/sounds/loss.bin"
};
#pragma data(data)

#pragma section(sa52, 0)
#pragma region(sa52, 0x68E80, 0x69000, , , {sa52})
#pragma data(sa52)
__export const char sa52_d[] = {
	#embed "../assets/sounds/move.bin"
};
#pragma data(data)

#pragma section(sa53, 0)
#pragma region(sa53, 0x69000, 0x695E0, , , {sa53})
#pragma data(sa53)
__export const char sa53_d[] = {
	#embed "../assets/sounds/win.bin"
};
#pragma data(data)

#pragma section(sa54, 0)
#pragma region(sa54, 0x695E0, 0x6BB00, , , {sa54})
#pragma data(sa54)
__export const char sa54_d[] = {
	#embed "../assets/sounds/outro.bin"
};
#pragma data(data)

// --- Miscellaneous ---

#pragma section(sa55, 0)
#pragma region(sa55, 0x6BB00, 0x6E3A0, , , {sa55})
#pragma data(sa55)
__export const char sa55_d[] = {
	#embed "../assets/ui/not_a_thing.bin"
};
#pragma data(data)

#pragma section(sa56, 0)
#pragma region(sa56, 0x6E3A0, 0x6E970, , , {sa56})
#pragma data(sa56)
__export const char sa56_d[] = {
	#embed "../assets/ui/ui_splash_continue.bin"
};
#pragma data(data)

#pragma section(sa57, 0)
#pragma region(sa57, 0x6E970, 0x71300, , , {sa57})
#pragma data(sa57)
__export const char sa57_d[] = {
	#embed "../assets/generated/puzzle_data.bin"
};
#pragma data(data)
