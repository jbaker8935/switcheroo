/**
 * @file input_handler.h
 * @brief Input event handler that converts inputs to game actions
 */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "platform_f256.h"
#include "../src/input.h"
#include "../src/game_state.h"

// Hit test results
typedef enum {
    HIT_NONE = 0,
    HIT_BOARD_CELL,
    HIT_MENU_ICON
} hit_type_t;

typedef struct {
    hit_type_t type;
    union {
        struct {
            uint8_t row;
            uint8_t col;
        } cell;
        menu_icon_t icon;
    } data;
} hit_result_t;

// Initialize input handler
void input_handler_init(void);

// Process an input event and update game state
void input_handler_process_event(game_state_t *state, const input_event_t *event);

// Hit testing - convert screen coordinates to game elements
hit_result_t input_handler_hit_test(uint16_t screen_x, uint16_t screen_y);

// Keyboard focus management
void input_handler_move_focus(game_state_t *state, key_code_t direction);
void input_handler_activate_focused(game_state_t *state);

#endif // INPUT_HANDLER_H
