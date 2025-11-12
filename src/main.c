#define F256LIB_IMPLEMENTATION
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/ai_agent.h"
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/platform_f256.h"
#include "../src/puzzle_data.h"
#include "../src/render.h"
#include "../src/screen.h"
#include "../src/text_display.h"
#include "../src/timer.h"
#include "../src/video.h"
#include "../src/mouse_pointer.h"
#include "../src/achievements.h"
#include "../src/file_io.h"
#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif
#include "stddef.h"

screen_state_t g_screen_state = SCREEN_SPLASH;

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_reset(void);
extern void display_test(void);
extern void video_set_game_mode_icon_bitmap(bool is_puzzle_mode);

// Global game state
game_state_t g_game_state;

void init_main_screen(void) {
    // Initialize subsystems
    video_init();  // Use default config
    // Initialize rendering
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
    print_current_player(g_game_state.context.current_player);
    print_move_history(g_game_state.context.history, g_game_state.context.history_count);
    refresh_win_path(&g_game_state.win_path);
    const puzzle_collection_t *collection = get_puzzle_collection();
    const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
    if (puzzle) {
        print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count,
            puzzle->difficulty, puzzle->is_solved);
        }
    }
    
void display_main_screen(void) {
    video_hide_splash();
    init_main_screen();
    render_update_score(&g_game_state.stats);
    game_state_start_new_game(&g_game_state);
    print_ai_difficulty(g_game_state.ai_config.difficulty);
    print_game_mode(g_game_state.is_puzzle_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);
    print_current_player(g_game_state.context.current_player);
    if (g_game_state.is_puzzle_mode) {
        const puzzle_collection_t *collection = get_puzzle_collection();
        const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
        if (puzzle) {
            print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count,
                            puzzle->difficulty, puzzle->is_solved);
        }
    }
}

void FAR11_main_loop(void);

#pragma clang optimize off
__attribute__((noinline))

void main_loop(void) {
    volatile unsigned char ___mmu = (unsigned char)*(volatile unsigned char *)0x000d;
    *(volatile unsigned char *)0x000d = 11;
    FAR11_main_loop();
    *(volatile unsigned char *)0x000d = ___mmu;
}
#pragma clang optimize on

__attribute__((noinline, section(".block11"))) 
void FAR11_main_loop(void) {

    uint16_t old_move_count = 0;
    
    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT) {
        // print_game_mode(g_game_state.is_puzzle_mode);
        // print_swap_rule(g_game_state.ai_config.swap_rule);
        // print_current_player(g_game_state.context.current_player);
        // print_move_history(g_game_state.context.history, g_game_state.context.history_count);
        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);

        if (g_screen_state == SCREEN_MAIN && g_game_state.phase == GAME_PHASE_GAME_OVER) {
            print_game_winner(g_game_state.win_path.winner);
            print_move_history(g_game_state.context.history, g_game_state.context.history_count);
            if (g_game_state.is_puzzle_mode) {
                // In puzzle mode, mark puzzle as solved if player won
                // in N Player A moves or less
                // retrieve current puzzle and its difficulty
                if (g_game_state.win_path.winner == PLAYER_WHITE) {
                    const puzzle_collection_t *collection = get_puzzle_collection();
                    const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                    bool qualifies_for_mark = false;
                    if (puzzle && !puzzle->is_solved) {
                        uint8_t white_moves = (uint8_t)((g_game_state.board.move_count + 1u) / 2u);
                        qualifies_for_mark = (white_moves <= puzzle->difficulty);
                        achievements_on_puzzle_attempt_completed(&g_game_state.achievements,
                                                                 puzzle,
                                                                 qualifies_for_mark);
                        // Mark puzzle as solved in persistent storage
                        if (qualifies_for_mark) {
                            mark_puzzle_solved(g_game_state.prefs.current_puzzle_index);
                            // confirm write.
                            const puzzle_t *puzzle_updated = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                            print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count,
                                            puzzle_updated->difficulty, puzzle_updated->is_solved);
                        }
                    } 
                }
            }
        }

        if (g_screen_state == SCREEN_MAIN && g_game_state.board.move_count != old_move_count && g_game_state.phase != GAME_PHASE_GAME_OVER) {
            print_move_history(g_game_state.context.history, g_game_state.context.history_count);
            print_current_player(g_game_state.context.current_player);
            old_move_count = g_game_state.board.move_count;
        }

        // Process input events - drain all pending events like sprites example
        do {

            bool alarm_elapsed = checkAlarm();
            if(alarm_elapsed && g_screen_state == SCREEN_SPLASH) {
                // Time to exit splash screen
                g_screen_state = SCREEN_MAIN;
                display_main_screen();

            }   
            if(g_screen_state == SCREEN_MAIN && g_game_state.is_puzzle_mode &&
                g_game_state.phase != GAME_PHASE_GAME_OVER) {
                print_puzzle_clock(getAlarmTicks());
                achievements_update_timer(&g_game_state.achievements, alarm_elapsed);
            }

            // Get next kernel event (always call, like the example)
            kernelNextEvent();

            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event)) {
                // Screen transition logic (extensible pattern)
                switch (g_screen_state) {
                    case SCREEN_SPLASH:
                        disable_mouse();
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            display_main_screen();
                        }
                        break;
                    case SCREEN_MAIN:
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_F1) {
                            display_show_help_screen();
                            g_screen_state = SCREEN_HELP;
                        } else if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_A) {
                            display_achievements_screen(&g_game_state.achievements, 0);
                            g_screen_state = SCREEN_ACHIEVEMENTS1;
                        } else {
                            // Process other main screen events through input handler
                            input_handler_process_event(&g_game_state, &event);
                        }
                        // Add other main screen transitions here
                        break;
                    case SCREEN_HELP:
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            // Hide help screen and restore main display
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
                    g_game_state.phase != GAME_PHASE_GAME_OVER && g_screen_state == SCREEN_MAIN  ) {
                    print_move_history(g_game_state.context.history, g_game_state.context.history_count);
                    print_current_player(g_game_state.context.current_player);
                    old_move_count = g_game_state.board.move_count;
                }
            }
        } while (kernelGetPending() > 0);

        // Diagnostic text output disabled in release builds to conserve ROM/RAM.

        // Update rendering
        if(g_screen_state == SCREEN_MAIN) {

            render_update(&g_game_state);
        }

        // Idle/wait for next frame
        platform_idle();
    }

}

int main(int argc, char *argv[]) {
        (void)argc;
        (void)argv;

        
        // Initialize f256lib (includes kernelReset and all subsystems)
    f256Init();

    input_init();
    input_handler_init();

    setTimer0();  // Initialize timer for UI updates and other periodic tasks

    // Initialize screen state - start in splash
    g_screen_state = SCREEN_SPLASH;
    video_show_splash();
    setAlarm(60); // Set alarm for 60 ticks 
    // Initialize game state
    game_state_init(&g_game_state);

    achievements_init(&g_game_state.achievements);

    file_io_init();

    main_loop();
    
    print_game_exit();

    file_io_save();
    // textClear();
    // getchar();


    // soft reset
    POKE(0xD6A2, 0xDE);
    POKE(0xD6A3, 0xAD);
    POKE(0xD6A0, 0x80);  // arm reset
    POKE(0xD6A0, 0x00);  // trigger reset

    return 0;
}
