#define F256LIB_IMPLEMENTATION
#include "f256lib.h"
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/render.h"

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_init(const void *config);

// Global game state
static game_state_t g_game_state;

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Initialize f256lib (includes kernelReset and all subsystems)
    f256Init();

    // Initialize subsystems
    video_init(NULL);  // Use default config
    input_init();
    input_handler_init();
    
    // Initialize game state
    game_state_init(&g_game_state);
    game_state_start_new_game(&g_game_state);
    
    // Initialize rendering
    render_init();
    
    // Main game loop
    uint32_t mouse_event_count = 0;
    uint32_t key_event_count = 0;
    uint32_t frame_count = 0;
    uint32_t total_events = 0;
    
    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT) {
        frame_count++;
        
        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);
        
        // Process input events - drain all pending events like sprites example
        uint32_t events_this_frame = 0;
        do {
            // Get next kernel event
            kernelCall(NextEvent);
            
            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event)) {
                events_this_frame++;
                total_events++;
                
                // Process event through input handler
                input_handler_process_event(&g_game_state, &event);
                
                // Debug: count events
                if (event.type == INPUT_EVENT_MOUSE_MOVE || 
                    event.type == INPUT_EVENT_MOUSE_DOWN || 
                    event.type == INPUT_EVENT_MOUSE_UP) {
                    mouse_event_count++;
                }
                if (event.type == INPUT_EVENT_KEY_DOWN || 
                    event.type == INPUT_EVENT_KEY_UP) {
                    key_event_count++;
                }
            }
        } while (kernelGetPending() > 0);
        
        // Debug output (top of screen) - update every 10 frames
        if (frame_count % 10 == 0) {
            textGotoXY(0, 0);
            printf("Frame:%ld Mouse:%ld Key:%ld Total:%ld     ", 
                   frame_count, mouse_event_count, key_event_count, total_events);
        }
        
        // Update rendering
        render_update(&g_game_state);
        
        // Idle/wait for next frame
        platform_idle();
    }
    
    return 0;
}

