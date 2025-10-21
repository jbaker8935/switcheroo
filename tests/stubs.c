#include <stdint.h>

void render_invalidate_cache(void) {}

void textGotoXY(uint8_t x, uint8_t y) {
    (void)x;
    (void)y;
}

void textPrint(char *text) {
    (void)text;
}

void print_formatted_text(uint8_t x, uint8_t y, const char *text) {
    (void)x;
    (void)y;
    (void)text;
}
