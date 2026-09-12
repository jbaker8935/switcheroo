#ifndef MOUSE_POINTER_H
#define MOUSE_POINTER_H

#include <stdint.h>

// PS/2 Mouse hardware registers (not in f256lib.h)
#define PS2_M_MODE_EN 0xD6E0
#define PS2_M_X_LO    0xD6E2
#define PS2_M_X_HI    0xD6E3
#define PS2_M_Y_LO    0xD6E4
#define PS2_M_Y_HI    0xD6E5

/* Pointer sensitivity: multiply kernel deltas before applying to hardware coords. */
#define MOUSE_SPEED_DEFAULT     2u
#define MOUSE_SPEED_FAST        4u
#define MOUSE_FAST_THRESHOLD    4u
#define MOUSE_SPEED_MULT_MIN    1u
#define MOUSE_SPEED_MULT_MAX    8u

typedef enum {
    MOUSE_CURSOR_NORMAL= 0,
    MOUSE_CURSOR_BUSY = 1
} mouse_cursor_t;


void set_mouse_cursor(mouse_cursor_t cursor_type);

void enable_mouse();
void disable_mouse();
void center_mouse();
void poll_and_refresh_mouse_postion();

/* Apply a kernel mouse.DELTA to the software-tracked hardware cursor and poke it. */
void mouse_apply_delta(int8_t dx, int8_t dy);
void mouse_get_hw_position(int16_t *x, int16_t *y);

void mouse_set_speed(uint8_t normal_mult, uint8_t fast_mult);
void mouse_get_speed(uint8_t *normal_mult, uint8_t *fast_mult);

#endif
