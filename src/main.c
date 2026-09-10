#include "overlay_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "ai_agent.h"
#include "game_state.h"
#include "input.h"
#include "input_handler.h"
#include "platform_f256.h"
#include "puzzle_data.h"
#include "render.h"
#include "screen.h"
#include "text_display.h"
#include "timer.h"
#include "video.h"
#include "mouse_pointer.h"
#include "achievements.h"
#include "file_io.h"
#include "sound.h"
#include "achievements_screen.h"
#include "playsid.h"
#include "system.h"
#include "f256lib.h"

screen_state_t g_screen_state = SCREEN_SPLASH;

extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_reset(void);
extern void display_test(void);
extern void video_set_game_mode_icon_bitmap(bool is_puzzle_mode);

game_state_t g_game_state;

static void ui_refresh_move_history(void)
{
    const bool is_free_play = !g_game_state.is_puzzle_mode;
    const bool has_start_entry = is_free_play && freeplay_history_has_start_entry(&g_game_state.history_state);
    uint16_t board_move_count = g_game_state.board.move_count;
    uint16_t live_move_count = board_move_count;

    if (is_free_play && g_game_state.history_state.count > 0u)
    {
        live_move_count = g_game_state.history_state.entries[0].board.move_count;
    }

    print_move_history(g_game_state.context.history,
                       g_game_state.context.history_count,
                       board_move_count,
                       live_move_count,
                       is_free_play,
                       has_start_entry);
}

void init_main_screen(void) {
    video_init();
    render_init();
    text_display_init();
}

void restore_main_screen(void) {
    init_main_screen();

    render_update_score(&g_game_state.stats);
    print_ai_difficulty(g_game_state.ai_config.difficulty);
    print_game_mode(g_game_state.is_puzzle_mode);
    video_set_game_mode_icon_bitmap(g_game_state.is_puzzle_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);
    game_state_print_current_player(&g_game_state);
    ui_refresh_move_history();
    refresh_win_path(&g_game_state.win_path);
    if (g_game_state.is_puzzle_mode) {
        const puzzle_collection_t *collection = get_puzzle_collection();
        const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
        if (puzzle) {
            print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count,
                puzzle->difficulty, puzzle->is_solved);
        }
    }
}

static uint8_t s_main_mouse_grace = 0;

void display_main_screen(void) {
    video_hide_splash();
    init_main_screen();
    render_update_score(&g_game_state.stats);
    game_state_start_new_game(&g_game_state);
    print_ai_difficulty(g_game_state.ai_config.difficulty);
    print_game_mode(g_game_state.is_puzzle_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);
    game_state_print_current_player(&g_game_state);
    if (g_game_state.is_puzzle_mode) {
        const puzzle_collection_t *collection = get_puzzle_collection();
        const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
        if (puzzle) {
            print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count,
                            puzzle->difficulty, puzzle->is_solved);
        }
    }
}

static void enter_main_from_splash(void) {
    g_screen_state = SCREEN_MAIN;
    clearAlarm(TIMER_ALARM_SPLASH);
    play_sound(SOUND_ID_RESET_BOARD);
    display_main_screen();
    input_sync_mouse_from_hardware();
    input_reset_mouse_button_edges();
    s_main_mouse_grace = 3;
}

#pragma code(ovl15_code)
void FAR_main_loop(void) {

    uint16_t old_move_count = 0;

    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT) {
        timer_service();

        if (g_screen_state == SCREEN_MAIN && s_main_mouse_grace > 0u) {
            s_main_mouse_grace--;
        }

        game_state_update(&g_game_state, 1.0 / 60.0);

        if (g_screen_state == SCREEN_MAIN && g_game_state.phase == GAME_PHASE_GAME_OVER) {
            if (g_game_state.is_puzzle_mode &&
                g_game_state.win_path.winner == PLAYER_WHITE &&
                game_state_has_exceeded_puzzle_moves(&g_game_state)) {
                print_game_over();
            } else {
                print_game_winner(g_game_state.win_path.winner);
            }
            ui_refresh_move_history();
            if (g_game_state.is_puzzle_mode) {
                if (g_game_state.win_path.winner == PLAYER_WHITE) {
                    const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                    if (puzzle) {
                        const bool was_solved = puzzle->is_solved;
                        const uint8_t white_moves = (uint8_t)((g_game_state.board.move_count + 1u) / 2u);
                        const bool qualifies_for_mark = (white_moves <= puzzle->difficulty);

                        achievements_on_puzzle_attempt_completed(&g_game_state.achievements,
                                                                 puzzle,
                                                                 qualifies_for_mark);

                        if (qualifies_for_mark && !was_solved) {
                            const puzzle_collection_t *collection = get_puzzle_collection();
                            mark_puzzle_solved(g_game_state.prefs.current_puzzle_index);
                            const puzzle_t *puzzle_updated = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                            if (collection && puzzle_updated) {
                                print_puzzle_info(g_game_state.prefs.current_puzzle_index,
                                                  collection->count,
                                                  puzzle_updated->difficulty,
                                                  puzzle_updated->is_solved);
                            }
                        }
                    }
                }
            }
        }

        if (g_screen_state == SCREEN_MAIN && g_game_state.board.move_count != old_move_count && g_game_state.phase != GAME_PHASE_GAME_OVER) {
            ui_refresh_move_history();
            game_state_print_current_player(&g_game_state);
            old_move_count = g_game_state.board.move_count;
        }

        do {
            bool splash_alarm_elapsed = checkAlarm(TIMER_ALARM_SPLASH);
            if (splash_alarm_elapsed && g_screen_state == SCREEN_SPLASH) {
                enter_main_from_splash();
            }

            bool puzzle_alarm_elapsed = checkAlarm(TIMER_ALARM_PUZZLE);
            if (g_screen_state == SCREEN_MAIN && g_game_state.is_puzzle_mode &&
                g_game_state.phase != GAME_PHASE_GAME_OVER) {
                print_puzzle_clock(getAlarmTicks(TIMER_ALARM_PUZZLE));
                achievements_update_timer(&g_game_state.achievements, puzzle_alarm_elapsed);
            }

            kernelNextEvent();

            input_event_t event;
            if (input_translate_event(&event)) {
                switch (g_screen_state) {
                    case SCREEN_SPLASH:
                        disable_mouse();
                        if (event.type == INPUT_EVENT_KEY_DOWN ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN &&
                             event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            enter_main_from_splash();
                        }
                        break;
                    case SCREEN_MAIN:
                        if (s_main_mouse_grace > 0u &&
                            event.type == INPUT_EVENT_MOUSE_DOWN) {
                            break;
                        }
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_F1) {
                            display_show_help_screen();
                            g_screen_state = SCREEN_HELP;
                        } else if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_A) {
                            display_achievements_screen(&g_game_state.achievements, 0);
                            g_screen_state = SCREEN_ACHIEVEMENTS1;
                        } else {
                            input_handler_process_event(&g_game_state, &event);
                        }
                        break;
                    case SCREEN_HELP:
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            display_hide_help_screen();
                            restore_main_screen();
                        }
                        break;
                    case SCREEN_ACHIEVEMENTS1:
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_A) {
                            g_screen_state = SCREEN_ACHIEVEMENTS2;
                            display_achievements_screen(&g_game_state.achievements, 1);
                        } else if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                                   (event.type == INPUT_EVENT_MOUSE_DOWN &&
                                    event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            hide_achievements_screen();
                            restore_main_screen();
                        }
                        break;
                    case SCREEN_ACHIEVEMENTS2:
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_A) {
                            g_screen_state = SCREEN_ACHIEVEMENTS1;
                            display_achievements_screen(&g_game_state.achievements, 0);
                        } else if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                                   (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            hide_achievements_screen();
                            restore_main_screen();
                        }
                        break;
                }
                if (g_game_state.board.move_count != old_move_count &&
                    g_game_state.phase != GAME_PHASE_GAME_OVER && g_screen_state == SCREEN_MAIN) {
                    ui_refresh_move_history();
                    game_state_print_current_player(&g_game_state);
                    old_move_count = g_game_state.board.move_count;
                }
            }
        } while (kernelGetPending() > 0);

        if(g_screen_state == SCREEN_MAIN) {
            render_update(&g_game_state);
        }

        platform_idle();
    }

}
#pragma code(code)

OVERLAY_TRAMPOLINE void main_loop(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_15);
    FAR_main_loop();
    POKE(OVERLAY_MMU_REG, saved);
}

char g_base_dir[256]={0};

int main(int argc, char *argv[]) {

    char *last_slash = strrchr(argv[0], '/');
    if (last_slash != NULL) {
        uint8_t dir_len = last_slash - argv[0] + 1;
        strncpy(g_base_dir, argv[0], dir_len);
        g_base_dir[dir_len] = '\0';
    }

    (void)argc;

    f256Init();

    input_init();
    input_handler_init();

    gameSetTimer0();
    setMonoSID();

    g_screen_state = SCREEN_SPLASH;
    video_show_splash();
    gameSetTimer0();
    bool intro_bypassed = playback(SRAM_SOUND_INTRO_SID, SOUND_INTRO_SID_FRAMES);

    game_state_init(&g_game_state);
    file_io_init();
    init_sounds();

    if (intro_bypassed) {
        enter_main_from_splash();
    } else {
        setAlarm(TIMER_ALARM_SPLASH, 2u * T0_TICK_FREQ);
        video_splash_continue();
        play_sound(SOUND_ID_START);
    }

    main_loop();

    video_show_exit();

    clearSIDRegisters();
    gameSetTimer0();
    bool outro_bypassed = playback(SRAM_SOUND_OUTRO_SID, SOUND_OUTRO_SID_FRAMES);

    if (!outro_bypassed) {
        setAlarm(TIMER_ALARM_GENERAL0, (uint16_t)(2u * T0_TICK_FREQ));
        bool exit_wait = false;
        while (!exit_wait && !checkAlarm(TIMER_ALARM_GENERAL0)) {
            timer_service();
            kernelNextEvent();
            input_event_t exit_event;
            if (input_translate_event(&exit_event) && exit_event.type == INPUT_EVENT_KEY_DOWN) {
                exit_wait = true;
            }
            if (!exit_wait) {
                platform_idle();
            }
        }
        clearAlarm(TIMER_ALARM_GENERAL0);
    }

    file_io_save();

    POKE(0xD6A2, 0xDE);
    POKE(0xD6A3, 0xAD);
    POKE(0xD6A0, 0x80);
    POKE(0xD6A0, 0x00);

    return 0;
}
