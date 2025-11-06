#ifndef SCREEN_H
#define SCREEN_H

typedef enum {
    SCREEN_SPLASH,
    SCREEN_MAIN,
    SCREEN_HELP,
    SCREEN_ACHIEVEMENTS1,
    SCREEN_ACHIEVEMENTS2
} screen_state_t;

extern screen_state_t g_screen_state;

#endif // SCREEN_H
