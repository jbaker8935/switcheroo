/**
 * @file input.c
 * @brief Input subsystem implementation for F256 Switcharoo
 */

#include "input.h"
#include "platform_f256.h"
#include "text_display.h"
#include "mouse_pointer.h"
#include <string.h>

static input_state_t s_input_state;

static void input_store_screen_mouse_from_hw(void) {
    int16_t hw_x;
    int16_t hw_y;

    mouse_get_hw_position(&hw_x, &hw_y);
    s_input_state.mouse_x = (uint16_t)(hw_x / 2);
    s_input_state.mouse_y = (uint16_t)(hw_y / 2);
}

// Keyboard scan code to key code mapping
// F256 keyboard scan codes from PS/2 keyboard
static key_code_t scan_to_key(uint8_t scan) {
    switch (scan) {
        case 0xB6: return KEY_UP;      // Up arrow
        case 0xB7: return KEY_DOWN;    // Down arrow
        case 0xB8: return KEY_LEFT;    // Left arrow
        case 0xB9: return KEY_RIGHT;   // Right arrow
        case 0x94: return KEY_ENTER;   // Enter
        case 0x92: return KEY_ESCAPE;  // Escape
        case 0x95: return KEY_ESCAPE;  // Escape (microkernel alternate)
        case 0x6D: return KEY_M;       // M
        case 0x72: return KEY_R;       // R
        case 0x70: return KEY_P;       // P
        case 0x6E: return KEY_N;       // N
        case 0x62: return KEY_B;       // B
        case 0x66: return KEY_F;       // F
        case 0x73: return KEY_S;       // S
        case 0x64: return KEY_D;       // D
        case 0x68: return KEY_H;       // H
        case 0x78: return KEY_X;       // X
        case 0x75: return KEY_U;       // U
        case 0x20: return KEY_SPACE;   // Spacebar
        case 0x81: return KEY_F1;      // F1
        case 0x61: return KEY_A;      // A
        default: return KEY_NONE;
    }
}

static key_code_t ascii_to_key(char c) {
    switch (c) {
        case ' ': return KEY_SPACE;
        case 'm': case 'M': return KEY_M;
        case 'r': case 'R': return KEY_R;
        case 'p': case 'P': return KEY_P;
        case 'n': case 'N': return KEY_N;
        case 'b': case 'B': return KEY_B;
        case 'f': case 'F': return KEY_F;
        case 's': case 'S': return KEY_S;
        case 'd': case 'D': return KEY_D;
        case 'h': case 'H': return KEY_H;
        case 'x': case 'X': return KEY_X;
        case 'u': case 'U': return KEY_U;
        case 'a': case 'A': return KEY_A;
        case '\r':
        case '\n': return KEY_ENTER;
        case 27: return KEY_ESCAPE;
        default: return KEY_NONE;
    }
}

static key_code_t key_from_kernel_event(void) {
    key_code_t key = scan_to_key(kernelEventData.u.key.raw);
    if (key != KEY_NONE) {
        return key;
    }

    char c = kernelEventData.u.key.ascii;
    if (c != 0 && !kernelEventData.u.key.flags) {
        return ascii_to_key(c);
    }

    return KEY_NONE;
}

void input_sync_mouse_from_hardware(void) {
    input_store_screen_mouse_from_hw();
}

void input_reset_mouse_button_edges(void) {
    s_input_state.mouse_buttons_prev = s_input_state.mouse_buttons;
}

void input_init(void) {
    memset(&s_input_state, 0, sizeof(s_input_state));
    s_input_state.mouse_x = 160;
    s_input_state.mouse_y = 120;
    s_input_state.focus_row = 6;  // Start at player's pieces
    s_input_state.focus_col = 0;
    s_input_state.keyboard_mode = false;
}

bool input_translate_event(input_event_t *event) {
    // Translate kernelEventData (already populated by caller's kernelCall) to our format
    // This function does NOT call kernelCall - caller must do that first!
    
    // Debug: log the kernel event type we're processing
    static uint8_t last_type = 0;
    if (kernelEventData.type != last_type) {

        last_type = kernelEventData.type;
    }


    if (kernelEventData.type == 0) {
        // No event
        if (event) {
            event->type = INPUT_EVENT_NONE;
        }
        return false;
    }
    
    // Handle keyboard events
    if (kernelEventData.type == kernelEvent(key.PRESSED)) {
        key_code_t key = key_from_kernel_event();

        if (key != KEY_NONE && event) {
            event->type = INPUT_EVENT_KEY_DOWN;
            event->data.key.code = key;
            event->data.key.ascii = kernelEventData.u.key.ascii;
            event->data.key.is_repeat = false;

            return true;
        }
    }
    
    // if (kernelEventData.type == kernelEvent(key.RELEASED)) {
    //     key_code_t key = scan_to_key(kernelEventData.u.key.raw);
        
    //     if (key != KEY_NONE && event) {
    //         event->type = INPUT_EVENT_KEY_UP;
    //         event->data.key.code = key;
    //         event->data.key.ascii = kernelEventData.u.key.ascii;
    //         event->data.key.is_repeat = false;

    //         return true;
    //     }
    // }
    
    // Handle mouse events
    if (kernelEventData.type == kernelEvent(mouse.DELTA)) {
        mouse_apply_delta((int8_t)kernelEventData.u.mouse.delta.x,
                          (int8_t)kernelEventData.u.mouse.delta.y);
        input_store_screen_mouse_from_hw();
        // print_mouse_position(s_input_state.mouse_x, s_input_state.mouse_y);
        // Update button state
        uint8_t new_buttons = kernelEventData.u.mouse.delta.buttons;
        bool button_changed = (new_buttons != s_input_state.mouse_buttons);
        s_input_state.mouse_buttons = new_buttons;
        
        if (event) {
            if (button_changed) {
                // Button state changed - prioritize that
                bool pressed = (new_buttons & 1) && !(s_input_state.mouse_buttons_prev & 1);
                if (pressed) {
                    event->type = INPUT_EVENT_MOUSE_DOWN;
                    event->data.mouse.x = s_input_state.mouse_x;
                    event->data.mouse.y = s_input_state.mouse_y;
                    event->data.mouse.button = MOUSE_BUTTON_LEFT;
                } else {
                    // Mouse up
                    event->type = INPUT_EVENT_MOUSE_UP;
                    event->data.mouse.x = s_input_state.mouse_x;
                    event->data.mouse.y = s_input_state.mouse_y;
                    event->data.mouse.button = MOUSE_BUTTON_LEFT;
                }
            } else {
                // Just movement
                event->type = INPUT_EVENT_MOUSE_MOVE;
                event->data.mouse.x = s_input_state.mouse_x;
                event->data.mouse.y = s_input_state.mouse_y;
                event->data.mouse.button = MOUSE_BUTTON_NONE;
            }
            s_input_state.keyboard_mode = false;
        }
        
        s_input_state.mouse_buttons_prev = new_buttons;

        
        return true;
    }
    
    if (kernelEventData.type == kernelEvent(mouse.CLICKS)) {
        // Handle mouse clicks - but for our simple selection logic, we only care about
        // the initial button press from DELTA events. CLICKS events are redundant
        // and can cause double-processing. Consume them without generating input events.
        return false;
    }
    
    // No events we recognize
    if (event) {
        event->type = INPUT_EVENT_NONE;
    }

    
    return false;
}

void input_get_mouse_position(uint16_t *x, uint16_t *y) {
    if (x) *x = s_input_state.mouse_x;
    if (y) *y = s_input_state.mouse_y;
}

bool input_is_mouse_button_down(mouse_button_t button) {
    if (button == MOUSE_BUTTON_LEFT) {
        return (s_input_state.mouse_buttons & 1) != 0;
    }
    if (button == MOUSE_BUTTON_RIGHT) {
        return (s_input_state.mouse_buttons & 2) != 0;
    }
    return false;
}

bool input_is_key_down(key_code_t key) {
    // TODO: Check keyboard state array
    (void)key;
    return false;
}

void input_set_focus(uint8_t row, uint8_t col) {

    disable_mouse();

    s_input_state.focus_row = row;
    s_input_state.focus_col = col;
    s_input_state.keyboard_mode = true;
}

void input_get_focus(uint8_t *row, uint8_t *col) {
    if (row) *row = s_input_state.focus_row;
    if (col) *col = s_input_state.focus_col;
}

bool input_is_keyboard_mode(void) {
    return s_input_state.keyboard_mode;
}
