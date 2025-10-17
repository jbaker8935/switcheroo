// PS/2 Mouse hardware registers (not in f256lib.h)
#define PS2_M_MODE_EN 0xD6E0
#define PS2_M_X_LO    0xD6E2
#define PS2_M_Y_LO    0xD6E4

typedef enum {
    MOUSE_CURSOR_NORMAL= 0,
    MOUSE_CURSOR_BUSY = 1
} mouse_cursor_t;


void set_mouse_cursor(mouse_cursor_t cursor_type);

void enable_mouse();
void disable_mouse();
void center_mouse();