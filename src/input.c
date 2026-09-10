/**
 * @file input.c
 * @brief Input subsystem implementation for F256 Switcharoo
 */

#include "input.h"
#include "platform_f256.h"
#include "text_display.h"
#include "mouse_pointer.h"
#include <string.h>

// PS/2 Mouse hardware registers
#define PS2_M_MODE_EN 0xD6E0
#define PS2_M_X_LO    0xD6E2
#define PS2_M_Y_LO    0xD6E4

static input_state_t s_input_state;

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
        key_code_t key = scan_to_key(kernelEventData.u.key.raw);
   
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
        // Apply boost for fast movement like in the example
        int8_t boost_x = 1;
        int8_t boost_y = 1;
        int8_t delta_x = (int8_t)kernelEventData.u.mouse.delta.x;
        int8_t delta_y = (int8_t)kernelEventData.u.mouse.delta.y;
        
        if (delta_x > 4 || delta_x < -4) boost_x = 2;
        if (delta_y > 4 || delta_y < -4) boost_y = 2;
        
        // Read current position from hardware
        int16_t hw_x = PEEKW(PS2_M_X_LO);
        int16_t hw_y = PEEKW(PS2_M_Y_LO);
        
        // Apply delta with boost
        int16_t new_x = hw_x + boost_x * delta_x;
        int16_t new_y = hw_y + boost_y * delta_y;
        
        // Clamp to mouse hardware bounds (640x480 regardless of video mode)
        // The mouse coordinate system is always 640x480 even in 320x240 mode
        if (new_x < 0) new_x = 0;
        if (new_x >= 640) new_x = 639;
        if (new_y < 0) new_y = 0;
        if (new_y >= 480) new_y = 479;
        
        // Write back to hardware registers
        POKEW(PS2_M_X_LO, new_x);
        POKEW(PS2_M_Y_LO, new_y);
        
        // Update our internal state (scale to screen coordinates if needed)
        // In 320x240 mode, mouse coords are 2x screen coords
        s_input_state.mouse_x = new_x / 2;
        s_input_state.mouse_y = new_y / 2;
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
