typedef enum {
    MOUSE_CURSOR_NORMAL= 0,
    MOUSE_CURSOR_BUSY = 1
} mouse_cursor_t;


void set_mouse_cursor(mouse_cursor_t cursor_type);