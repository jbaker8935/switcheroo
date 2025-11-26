#include "f256lib.h"
#include "../src/sram_assets.h"

EMBED(board_bitmap, "../assets/ui/ui_board.bin", 0x20000);
EMBED(splash_bitmap, "../assets/ui/ui_splash.bin", 0x32C00);
EMBED(achievement_bitmap, "../assets/ui/ui_achievements.bin", 0x45800);
EMBED(board_palette, "../assets/ui/ui_board_palette.bin", 0x58400);
EMBED(pieces_palette, "../assets/ui/ui_pieces_palette.bin", 0x58800);
EMBED(menu_icon_palette, "../assets/ui/ui_menu_palette.bin", 0x58C00);
EMBED(splash_palette, "../assets/ui/ui_splash_palette.bin", 0x59000);
EMBED(achieve_base_palette, "../assets/ui/ui_achieve_base_palette.bin", 0x59400);
EMBED(achieve_color_palette, "../assets/ui/ui_achieve_color_palette.bin", 0x59800);
EMBED(achieve_grey_palette, "../assets/ui/ui_achieve_grey_palette.bin", 0x59C00);
EMBED(icon_game_mode_ai, "../assets/ui/ui_menu_robot_32x32.bin", 0x5A000);
EMBED(icon_game_mode_puzzle, "../assets/ui/ui_menu_puzzle_32x32.bin", 0x5A400);
EMBED(icon_reset, "../assets/ui/ui_menu_retry_32x32.bin", 0x5A800);
EMBED(icon_previous, "../assets/ui/ui_menu_left_32x32.bin", 0x5AC00);
EMBED(icon_next, "../assets/ui/ui_menu_right_32x32.bin", 0x5B000);
EMBED(icon_swap_mode, "../assets/ui/ui_menu_swap_mode_32x32.bin", 0x5B400);
EMBED(icon_difficulty, "../assets/ui/ui_menu_difficulty_32x32.bin", 0x5B800);
EMBED(icon_hint, "../assets/ui/ui_menu_hint_32x32.bin", 0x5BC00);
EMBED(icon_exit, "../assets/ui/ui_menu_exit_32x32.bin", 0x5C000);
EMBED(achieve_award, "../assets/ui/award.bin", 0x5C400);
EMBED(achieve_100, "../assets/ui/achieve_100.bin", 0x5C640);
EMBED(achieve_runner, "../assets/ui/runner.bin", 0x5C880);
EMBED(achieve_brain, "../assets/ui/brain.bin", 0x5CAC0);
EMBED(achieve_puzzle, "../assets/ui/achieve_puzzle.bin", 0x5CD00);
EMBED(achieve_thinker, "../assets/ui/thinker.bin", 0x5CF40);
EMBED(achieve_lightning, "../assets/ui/lightning.bin", 0x5D180);
EMBED(achieve_bullseye, "../assets/ui/bullseye.bin", 0x5D3C0);
EMBED(achieve_sword, "../assets/ui/sword.bin", 0x5D600);
EMBED(achieve_arm_flex, "../assets/ui/arm_flex.bin", 0x5D840);
EMBED(achieve_dice, "../assets/ui/dice.bin", 0x5DA80);
EMBED(achieve_flame, "../assets/ui/flame.bin", 0x5DCC0);
EMBED(achieve_medal, "../assets/ui/medal.bin", 0x5DF00);
EMBED(achieve_crown, "../assets/ui/crown.bin", 0x5E140);
EMBED(piece_a_normal_light, "../assets/ui/playerA_normal_light.bin", 0x5E380);
EMBED(piece_a_swapped_light, "../assets/ui/playerA_swapped_light.bin", 0x5E5C0);
EMBED(piece_b_normal_light, "../assets/ui/playerB_normal_light.bin", 0x5E800);
EMBED(piece_b_swapped_light, "../assets/ui/playerB_swapped_light.bin", 0x5EA40);
EMBED(piece_a_normal_dark, "../assets/ui/playerA_normal_dark.bin", 0x5EC80);
EMBED(piece_a_swapped_dark, "../assets/ui/playerA_swapped_dark.bin", 0x5EEC0);
EMBED(piece_b_normal_dark, "../assets/ui/playerB_normal_dark.bin", 0x5F100);
EMBED(piece_b_swapped_dark, "../assets/ui/playerB_swapped_dark.bin", 0x5F340);
EMBED(highlight_empty, "../assets/ui/highlight_empty.bin", 0x5F580);
EMBED(highlight_occupied, "../assets/ui/highlight_occupied.bin", 0x5F7C0);
EMBED(focus_piece, "../assets/ui/cell_focus.bin", 0x5FA00);

EMBED(sound_loss,"../assets/sounds/loss.mp3",0x5FE00u);
EMBED(sound_move,"../assets/sounds/move.mp3",0x61630u);
EMBED(sound_reset_board,"../assets/sounds/reset_board.mp3",0x62E60u);
EMBED(sound_win,"../assets/sounds/win.mp3",0x64690u);
EMBED(sound_game_start,"../assets/sounds/game_start_sound.mp3",0x65EC0u);


EMBED(sid_sound_intro,"../assets/sounds/intro.bin",0x66890u);
EMBED(sid_sound_game_start,"../assets/sounds/loaded.bin",0x685E0u);
EMBED(sid_sound_loss,"../assets/sounds/loss.bin",0x688A0u);
EMBED(sid_sound_move,"../assets/sounds/move.bin",0x68E80u);
EMBED(sid_sound_win,"../assets/sounds/win.bin",0x69000u);
EMBED(sid_outro,"../assets/sounds/outro.bin",0x695E0u);



EMBED(not_a_thing, "../assets/ui/not_a_thing.bin", 0x6BB00u);
EMBED(ui_splash_continue, "../assets/ui/ui_splash_continue.bin", 0x6E3A0u);
EMBED(puzzle_catalog, "../assets/generated/puzzle_data.bin", 0x6E970u);



