#define F256LIB_IMPLEMENTATION
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/render.h"
#include "f256lib.h"
#include "../src/text_display.h"

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_init(const void *config);

// Global game state
static game_state_t g_game_state;


#define SEGMENT_MAIN

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint16_t old_move_count = 0;

    // Initialize f256lib (includes kernelReset and all subsystems)
    f256Init();
    // Initialize subsystems
    video_init(NULL); // Use default config
    input_init();
    input_handler_init();

    // Initialize game state
    game_state_init(&g_game_state);
    game_state_start_new_game(&g_game_state);

    // Initialize rendering
    render_init();

    // Main game loop

    
    print_ai_difficulty(g_game_state.ai_config.difficulty);
    // print_game_mode(g_game_state.game_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);

    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT)
    {

        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);
        if (g_game_state.board.move_count != old_move_count)
        {
            print_move_history(g_game_state.board.history, g_game_state.board.history_count);
            print_current_player(g_game_state.board.current_player);
            old_move_count = g_game_state.board.move_count;
        }

        // Process input events - drain all pending events like sprites example
        do
        {
            // Get next kernel event (always call, like the example)
            kernelNextEvent();

            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event))
            {
                // Process event through input handler
                input_handler_process_event(&g_game_state, &event);
                if (g_game_state.board.move_count != old_move_count)
                {
                    print_move_history(g_game_state.board.history, g_game_state.board.history_count);
                    print_current_player(g_game_state.board.current_player);
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

    return 0;
}
