#ifndef PLATFORM_F256_CONFIG_H
#define PLATFORM_F256_CONFIG_H

#include "f256lib.h"
#include <stdint.h>
#include <stdbool.h>

static inline uint8_t platform_far_read_byte(uint32_t address) {
	return FAR_PEEK(address);
}

static inline uint16_t platform_far_read_word(uint32_t address) {
	return FAR_PEEKW(address);
}

static inline void platform_far_read_bytes(uint32_t address,
                                           uint8_t *dest,
                                           uint16_t length) {
	while (length != 0u) {
		*dest++ = FAR_PEEK(address++);
		--length;
	}
}

#endif /* PLATFORM_F256_CONFIG_H */
