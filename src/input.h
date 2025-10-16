/**
 * @file input.h
 * @brief Input subsystem for F256 Switcharoo
 * 
 * Handles mouse and keyboard input with event translation per requirements.md.
 */

#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/platform_f256_host.h"
#else
#include "platform_f256.h"
#endif
#include <stdint.h>

// Input event types
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_DOWN,
    INPUT_EVENT_MOUSE_UP,
    INPUT_EVENT_KEY_DOWN,
    INPUT_EVENT_KEY_UP
} input_event_type_t;

// Key codes
typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_R,      // Reset
    KEY_I,      // Info
    KEY_D,      // Difficulty
    KEY_S,      // Starting board
    KEY_H,      // History
    KEY_X,      // Exit
    KEY_M,      // Mute
    KEY_PLUS,   // Volume up
    KEY_MINUS,  // Volume down
    KEY_U       // Undo
} key_code_t;

// Mouse buttons
typedef enum {
    MOUSE_BUTTON_NONE = 0,
    MOUSE_BUTTON_LEFT = 1,
    MOUSE_BUTTON_RIGHT = 2,
    MOUSE_BUTTON_MIDDLE = 3
} mouse_button_t;

// Input event structure
typedef struct {
    input_event_type_t type;
    union {
        struct {
            uint16_t x;
            uint16_t y;
            mouse_button_t button;
        } mouse;
        struct {
            key_code_t code;
            uint8_t modifiers;
            char ascii;        // ASCII character if available
            bool is_repeat;    // True if key repeat event
        } key;
    } data;
} input_event_t;

// Input state
typedef struct {
    uint16_t mouse_x;
    uint16_t mouse_y;
    uint8_t mouse_buttons;
    uint8_t mouse_buttons_prev;
    uint8_t keyboard_state[16];  // Bit array for key states
    uint8_t focus_row;
    uint8_t focus_col;
    bool keyboard_mode;          // True if using keyboard navigation
} input_state_t;

// Initialize input subsystem
void input_init(void);

// Translate kernelEventData to our event format (caller must call kernelCall(NextEvent) first!)
bool input_translate_event(input_event_t *event);

// Query current state
void input_get_mouse_position(uint16_t *x, uint16_t *y);
bool input_is_mouse_button_down(mouse_button_t button);
bool input_is_key_down(key_code_t key);

// Focus management for keyboard navigation
void input_set_focus(uint8_t row, uint8_t col);
void input_get_focus(uint8_t *row, uint8_t *col);
bool input_is_keyboard_mode(void);

#endif // PLATFORM_INPUT_H
