#ifndef PLATFORM_F256_HOST_H
#define PLATFORM_F256_HOST_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal host stub of platform_f256.h interfaces needed for unit tests */

/* Timer and MMIO constants (no-op in host tests) */
#define T0_CTR 0u
#define T0_PEND 0u
#define T0_CMP_CTR 0u
#define T0_CMP_CTR_RECLEAR 0u
#define T0_CMP_L 0u
#define T0_CMP_M 0u
#define T0_CMP_H 0u
#define T0_VAL_H 0u
#define T0_VAL_M 0u
#define T0_VAL_L 0u

#define CTR_CLEAR 0u
#define CTR_UPDOWN 0u
#define CTR_ENABLE 0u

/* Keyboard/mouse event types */
typedef struct {
    uint8_t dummy;
} events_t;

/* Host harness provides these implementations */
void textGotoXY(uint8_t x, uint8_t y);
void textPrint(char *text);
void render_invalidate_cache(void);
void video_reset_all_board_cell_colors(void);

#endif /* PLATFORM_F256_HOST_H */
