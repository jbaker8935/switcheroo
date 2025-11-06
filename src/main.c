#define F256LIB_IMPLEMENTATION
#include "f256lib.h"
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/render.h"
#include "../src/platform_f256.h"
#include "../src/text_display.h"
#include "../src/puzzle_data.h"
#include "../src/ai_agent.h"
#include "../src/video.h"
#include "../src/timer.h"
#include "../src/screen.h"
#include "stddef.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

screen_state_t g_screen_state = SCREEN_SPLASH;

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_reset(void);
extern void display_test(void);

// Global game state
static game_state_t g_game_state;

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint16_t old_move_count = 0;

    // Initialize f256lib (includes kernelReset and all subsystems)
    f256Init();
    // Initialize subsystems
    video_init(); // Use default config
    // Initialize rendering
    render_init();
    text_display_init();
    input_init();
    input_handler_init();

    setTimer0(); // Initialize timer for UI updates and other periodic tasks

    // Initialize game state
    game_state_init(&g_game_state);
    render_update_score(&g_game_state.stats);
    game_state_start_new_game(&g_game_state);

    // Initialize screen state - start in main (splash will come later)
    g_screen_state = SCREEN_MAIN;


    // Main game loop


    print_ai_difficulty(g_game_state.ai_config.difficulty);
    print_game_mode(g_game_state.is_puzzle_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);
    print_current_player(g_game_state.context.current_player);
    print_move_history(g_game_state.context.history, g_game_state.context.history_count);    
    
    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT)
    {
        // print_game_mode(g_game_state.is_puzzle_mode);
        // print_swap_rule(g_game_state.ai_config.swap_rule);
        // print_current_player(g_game_state.context.current_player);
        // print_move_history(g_game_state.context.history, g_game_state.context.history_count);
        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);

        if (g_game_state.phase == GAME_PHASE_GAME_OVER)
        {
            print_game_winner(g_game_state.win_path.winner);
            if (g_game_state.is_puzzle_mode)
            {
                // In puzzle mode, mark puzzle as solved if player won
                // in N Player A moves or less 
                // retrieve current puzzle and its difficulty
                if (g_game_state.win_path.winner == PLAYER_WHITE)
                {
                    const puzzle_collection_t *collection = get_puzzle_collection();
                    const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                    if (puzzle && !puzzle->is_solved && ((g_game_state.board.move_count+1)/2) <= puzzle->difficulty)
                    {
                        // Mark puzzle as solved in persistent storage
                        mark_puzzle_solved(g_game_state.prefs.current_puzzle_index);
                        // confirm write.
                        const puzzle_t * puzzle_updated = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                        print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count, puzzle_updated->difficulty, puzzle_updated->is_solved);
                    }
                }
            }
        }

        if (g_game_state.board.move_count != old_move_count)
        {
            print_move_history(g_game_state.context.history, g_game_state.context.history_count);
            print_current_player(g_game_state.context.current_player);
            old_move_count = g_game_state.board.move_count;
        }

        // Process input events - drain all pending events like sprites example
        do {
            // Get next kernel event (always call, like the example)
            kernelNextEvent();

            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event))
            {
                // Process event through input handler
                input_handler_process_event(&g_game_state, &event);

                // Screen transition logic (extensible pattern)
                switch (g_screen_state) {
                    case SCREEN_SPLASH:
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            // TODO: Call main screen draw/init if needed
                        }
                        break;
                    case SCREEN_MAIN:
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_F1) {
                            display_show_help_screen();
                            g_screen_state = SCREEN_HELP;
                        }
                        // Add other main screen transitions here
                        break;
                    case SCREEN_HELP:
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            // Hide help screen and restore main display
                            display_hide_help_screen();
                            render_update_score(&g_game_state.stats);
                            print_ai_difficulty(g_game_state.ai_config.difficulty);
                            print_game_mode(g_game_state.is_puzzle_mode);
                            print_swap_rule(g_game_state.ai_config.swap_rule);
                            print_current_player(g_game_state.context.current_player);
                            print_move_history(g_game_state.context.history, g_game_state.context.history_count);
                            if(g_game_state.is_puzzle_mode) {
                                clear_swap_unavailable();
                                game_state_apply_current_puzzle(&g_game_state, true);
                            }
                        }
                        break;
                    case SCREEN_ACHIEVEMENTS1:
                        if (event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_A) {
                            g_screen_state = SCREEN_ACHIEVEMENTS2;
                            // TODO: Show second achievements screen
                        } else if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                                   (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            // TODO: Restore main screen
                        }
                        break;
                    case SCREEN_ACHIEVEMENTS2:
                        if ((event.type == INPUT_EVENT_KEY_DOWN && event.data.key.code == KEY_SPACE) ||
                            (event.type == INPUT_EVENT_MOUSE_DOWN && event.data.mouse.button == MOUSE_BUTTON_LEFT)) {
                            g_screen_state = SCREEN_MAIN;
                            // TODO: Restore main screen
                        }
                        break;
                }
                if (g_game_state.board.move_count != old_move_count)
                {
                    print_move_history(g_game_state.context.history, g_game_state.context.history_count);
                    print_current_player(g_game_state.context.current_player);
                    old_move_count = g_game_state.board.move_count;
                }
            }
        } while (kernelGetPending() > 0);

        // Diagnostic text output disabled in release builds to conserve ROM/RAM.

        // Update rendering
        render_update(&g_game_state);

        // Idle/wait for next frame
        platform_idle();
    }

    textClear();
    // getchar();

    // soft reset
    POKE(0xD6A2, 0xDE);
    POKE(0xD6A3, 0xAD);
    POKE(0xD6A0, 0x80); // arm reset
    POKE(0xD6A0, 0x00); // trigger reset

    return 0;
}
