#ifndef PLATFORM_F256_CONFIG_H
#define PLATFORM_F256_CONFIG_H

// Configure the far-memory swap slot to use bank 5 (0xA000 window).
// Bank 7 is reserved by the Foenix microkernel and cannot be repurposed
// for far memory streaming, so we remap the f256lib helpers here.
// #ifndef SWAP_SLOT
// #define SWAP_SLOT 0x000D  // MMU_MEM_BANK_5
// #endif

// // Ensure the swap slot is restored after each FAR_PEEK/FAR_POKE to keep
// // the memory map stable for subsequent operations.
// #ifndef SWAP_RESTORE
// #define SWAP_RESTORE
// #endif

#include "f256lib.h"

#include <stdint.h>

#include <stdbool.h>

// #define PLATFORM_F256_BANK_REGISTER MMU_MEM_BANK_5
// #define PLATFORM_F256_BANK_WINDOW   0xA000u
// #define PLATFORM_F256_BLOCK_SIZE    0x2000u

static inline void platform_far_write_byte(uint32_t address, uint8_t value) {
#if defined(__llvm_mos__)
	FAR_POKE(address, value);
#else
	(void)address;
	(void)value;
#endif
}

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
