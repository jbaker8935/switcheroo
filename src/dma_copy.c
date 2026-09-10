
#include "f256lib.h"
void dma2dCopy(uint32_t dest, uint32_t src, uint16_t width, uint16_t height, uint16_t source_stride, uint16_t dest_stride) {
	while (PEEKW(RAST_ROW_L) < 482); // Wait for VBL.

	__asm volatile { sei }
	POKE(DMA_CTRL, DMA_CTRL_2D | DMA_CTRL_ENABLE);
	POKEA(DMA_SRC_ADDR, src);
	POKEA(DMA_DST_ADDR, dest);
	POKEW(DMA_WIDTH, width);
	POKEW(DMA_HEIGHT, height);
	POKEW(DMA_STRIDE_DST, dest_stride);
	POKEW(DMA_STRIDE_SRC, source_stride);
	POKE(DMA_CTRL, PEEK(DMA_CTRL) | DMA_CTRL_START);
	__asm volatile { nop }
	__asm volatile { nop }
	__asm volatile { nop }
	__asm volatile { nop }
	__asm volatile { nop }
	__asm volatile { nop }
	__asm volatile { cli }


}
