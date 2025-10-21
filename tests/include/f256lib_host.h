#ifndef F256LIB_HOST_H
#define F256LIB_HOST_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal host stub of f256lib interfaces needed for unit tests */

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

static inline uint8_t PEEK(uint16_t address) {
    (void)address;
    return 0u;
}

static inline void POKE(uint16_t address, uint8_t value) {
    (void)address;
    (void)value;
}

/* Host harness provides these implementations in stubs.c */
void textGotoXY(uint8_t x, uint8_t y);
void textPrint(char *text);

/* Math coprocessor host fallbacks */
static inline int32_t mathSignedMultiply(int16_t a, int16_t b) {
    return (int32_t)a * (int32_t)b;
}

static inline uint32_t mathUnsignedMultiply(uint16_t a, uint16_t b) {
    return (uint32_t)a * (uint32_t)b;
}

static inline uint32_t mathUnsignedAddition(uint32_t a, uint32_t b) {
    return a + b;
}

static inline uint16_t mathUnsignedDivision(uint16_t a, uint16_t b) {
    return (uint16_t)(b == 0u ? 0u : a / b);
}

static inline uint16_t mathUnsignedDivisionRemainder(uint16_t a, uint16_t b, uint16_t *remainder) {
    if (b == 0u) {
        if (remainder) {
            *remainder = 0u;
        }
        return 0u;
    }
    if (remainder) {
        *remainder = (uint16_t)(a % b);
    }
    return (uint16_t)(a / b);
}

static inline int16_t mathSignedDivision(int16_t a, int16_t b) {
    return (int16_t)(b == 0 ? 0 : a / b);
}

static inline int16_t mathSignedDivisionRemainder(int16_t a, int16_t b, int16_t *remainder) {
    if (b == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }
    if (remainder) {
        *remainder = (int16_t)(a % b);
    }
    return (int16_t)(a / b);
}

#endif /* F256LIB_HOST_H */
