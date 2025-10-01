#define F256LIB_IMPLEMENTATION
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/render.h"
#include "f256lib.h"

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_init(const void *config);

// Global game state
static game_state_t g_game_state;

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

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

    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT)
    {
        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);

        // Process input events - drain all pending events like sprites example
        uint32_t events_this_frame = 0;
        uint32_t loop_iterations = 0;
        do
        {
            loop_iterations++;

            // Get next kernel event (always call, like the example)
            kernelNextEvent();

            // Debug: print raw kernel event type, kernelError and pending count
            textGotoXY(0, 1);
            printf("Kernel event type: 0x%02X  kernelError=%d pending=%d Iter:%ld",
                kernelEventData.type, (int)kernelError,
                (int)kernelArgs->events.pending, loop_iterations);

            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event))
            {
                events_this_frame++;

                // Process event through input handler
                input_handler_process_event(&g_game_state, &event);
            }
        } while (kernelGetPending() > 0);

        // Update rendering
        render_update(&g_game_state);

        // Idle/wait for next frame
        platform_idle();
    }

    return 0;
}
