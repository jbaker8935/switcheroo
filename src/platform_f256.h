#ifndef PLATFORM_F256_CONFIG_H
#define PLATFORM_F256_CONFIG_H



#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "f256lib.h"
#endif

#include <stdint.h>

#include <stdbool.h>


static inline uint8_t platform_far_read_byte(uint32_t address) {
#if defined(__llvm_mos__)
	return FAR_PEEK(address);
#else
	(void)address;
	return 0u;
#endif
}

static inline uint16_t platform_far_read_word(uint32_t address) {
#if defined(__llvm_mos__)
	return FAR_PEEKW(address);
#else
	(void)address;
	return 0u;
#endif
}

static inline void platform_far_read_bytes(uint32_t address,
								   uint8_t *dest,
								   uint16_t length) {
#if defined(__llvm_mos__)
	while (length != 0u) {
		*dest++ = FAR_PEEK(address++);
		--length;
	}
#else
	(void)address;
	(void)dest;
	(void)length;
#endif
}

#endif /* PLATFORM_F256_CONFIG_H */
