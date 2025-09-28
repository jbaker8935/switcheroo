/*
 * Local Foenix F256 linker script for F256 Switcharoo.
 *
 * Derived from the Foenix SDK linker script distributed under the MIT license
 * by Kangaroo Punch, bundled with the llvm-mos toolchain. This copy is kept in
 * the repository to make the build independent of the external toolchain
 * directory layout while preserving the original memory map semantics.
 */

/* Fake C stack placed at the top of banked memory. */
PROVIDE(__stack = 0xA000);
/* Entry point to the program; 0x200-0x2FF are reserved for arguments. */
PROVIDE(__f256_start = 0x300);
/* Size (in bytes) of a Foenix memory block. */
PROVIDE(__BLOCK_SIZE = 0x2000);
/* Physical address of the swappable block. */
PROVIDE(__SLOT_ADDR = 0xA000);

/* Zero-page register layout sourced from the Foenix hardware map. */
__rc0 = 0x10;
/* GENERATED FILE -- DO NOT MANUALLY EDIT. */
/* Note: The odd regs must immediately follow the even regs for even/odd
 * pairs to work as pointers, but the even regs can be assigned arbitrarily in
 * linker scripts. */
__rc1 = __rc0 + 1;
PROVIDE(__rc2 = __rc1 + 1);
__rc3 = __rc2 + 1;
PROVIDE(__rc4 = __rc3 + 1);
__rc5 = __rc4 + 1;
PROVIDE(__rc6 = __rc5 + 1);
__rc7 = __rc6 + 1;
PROVIDE(__rc8 = __rc7 + 1);
__rc9 = __rc8 + 1;
PROVIDE(__rc10 = __rc9 + 1);
__rc11 = __rc10 + 1;
PROVIDE(__rc12 = __rc11 + 1);
__rc13 = __rc12 + 1;
PROVIDE(__rc14 = __rc13 + 1);
__rc15 = __rc14 + 1;
PROVIDE(__rc16 = __rc15 + 1);
__rc17 = __rc16 + 1;
PROVIDE(__rc18 = __rc17 + 1);
__rc19 = __rc18 + 1;
PROVIDE(__rc20 = __rc19 + 1);
__rc21 = __rc20 + 1;
PROVIDE(__rc22 = __rc21 + 1);
__rc23 = __rc22 + 1;
PROVIDE(__rc24 = __rc23 + 1);
__rc25 = __rc24 + 1;
PROVIDE(__rc26 = __rc25 + 1);
__rc27 = __rc26 + 1;
PROVIDE(__rc28 = __rc27 + 1);
__rc29 = __rc28 + 1;
PROVIDE(__rc30 = __rc29 + 1);
__rc31 = __rc30 + 1;

ASSERT(__rc0 == 0x10, "Inconsistent zero page map.");
ASSERT(__rc31 == 0x2f, "Inconsistent zero page map.");

MEMORY {
    /* Kernel uses 0xF0-0xFF for parameter passing. */
    zp : ORIGIN = __rc31 + 1, LENGTH = 0xF0 - (__rc31 + 1)
    ram (rw) : ORIGIN = __f256_start, LENGTH = __stack - __f256_start
}

/* Load memory addresses for overlay banks. */
__block8_lma    = ( 8 << 24) | __SLOT_ADDR;
__block9_lma    = ( 9 << 24) | __SLOT_ADDR;
__block10_lma   = (10 << 24) | __SLOT_ADDR;
__block11_lma   = (11 << 24) | __SLOT_ADDR;
__block12_lma   = (12 << 24) | __SLOT_ADDR;
__block13_lma   = (13 << 24) | __SLOT_ADDR;
__block14_lma   = (14 << 24) | __SLOT_ADDR;
__block15_lma   = (15 << 24) | __SLOT_ADDR;
__block16_lma   = (16 << 24) | __SLOT_ADDR;
__block17_lma   = (17 << 24) | __SLOT_ADDR;
__block18_lma   = (18 << 24) | __SLOT_ADDR;
__block19_lma   = (19 << 24) | __SLOT_ADDR;
__block20_lma   = (20 << 24) | __SLOT_ADDR;
__block21_lma   = (21 << 24) | __SLOT_ADDR;
__block22_lma   = (22 << 24) | __SLOT_ADDR;
__block23_lma   = (23 << 24) | __SLOT_ADDR;

/* Placeholder for binary data overlays (none defined yet). */

MEMORY {
    board_bitmap : ORIGIN = __block8_lma, LENGTH = __BLOCK_SIZE * 10
    block18      : ORIGIN = __block18_lma, LENGTH = __BLOCK_SIZE
    block19      : ORIGIN = __block19_lma, LENGTH = __BLOCK_SIZE
    block20      : ORIGIN = __block20_lma, LENGTH = __BLOCK_SIZE
    block21      : ORIGIN = __block21_lma, LENGTH = __BLOCK_SIZE
    block22      : ORIGIN = __block22_lma, LENGTH = __BLOCK_SIZE
    block23      : ORIGIN = __block23_lma, LENGTH = __BLOCK_SIZE
}

REGION_ALIAS("c_writeable", ram)
REGION_ALIAS("c_readonly", ram)

SECTIONS {
    /* Zero-page data that requires initialisation. */
    .zp.data : {
        __zp_data_start = .;
        *(.zp.data .zp.data.* .zp.rodata .zp.rodata.*)
    } >zp AT>c_readonly
    __zp_data_load_start = LOADADDR(.zp.data);
    __zp_data_size = SIZEOF(.zp.data);

    /* Zero-page variables that default to zero. */
    .zp.bss (NOLOAD) : {
        __zp_bss_start = .;
        *(.zp.bss .zp.bss.*)
    } >zp
    __zp_bss_size = SIZEOF(.zp.bss);

    /* Uninitialised zero-page slots reserved for direct-page usage. */
    .zp (NOLOAD) : {
        *(.zp .zp.* .zeropage .zeropage.* .directpage .directpage.* ZEROPAGE)
    } >zp

    /* Programme code and startup trampolines. */
    .text : {
        /* Dynamic _init construction. */
        _init = .;
        _start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init.* .init)))
        KEEP(*(.call_main))
        KEEP(*(.after_main))

        /* Dynamic _fini construction. */
        _fini = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini.* .fini)))
        KEEP(*(.fini_rts))

        *(.text .text.* CODE LOWCODE ONCE STARTUP)

        /* Constructor / destructor tables. */
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.* .init_array)))
        __init_array_end = .;

        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.* .fini_array)))
        __fini_array_end = .;
    } >c_readonly

    /* Read-only data. */
    .rodata : {
        *(.rodata .rodata.* RODATA)
    } >c_readonly

    /* Read/write data initialised by the loader. */
    .data : {
        __data_start = .;
        *(.data .data.* DATA)
        __data_end = .;
    } >c_writeable AT>c_readonly
    __data_load_start = LOADADDR(.data);
    __data_size = SIZEOF(.data);

    /* Zero-initialised data. */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* BSS COMMON)
        __bss_end = .;
    } >c_writeable
    __bss_size = SIZEOF(.bss);

    /* Persisted buffers that must survive resets. */
    .noinit (NOLOAD) : {
        *(.noinit .noinit.* NULL INIT ZPSAVE)
        /* Ensure chunks have a free low bit for allocator metadata. */
        __heap_start = ALIGN(., 2);
    } >c_writeable

    /* Overlay hooks: no sections defined yet, but keep layout compatible. */
    .video_board_bitmap : {
        *(.video_board_bitmap .video_board_bitmap.*)
    } >board_bitmap
    end_video_board_bitmap = .;
}

OUTPUT_FORMAT {
    BYTE(0x5A)    /* pgZ */

    /* RAM Segment */
    SHORT(ORIGIN(ram))              /* where to load it, 24 bits */
    BYTE(0x00)
    SHORT(__bss_start - ORIGIN(ram))  /* size to load */
    BYTE(0x00)
    TRIM(ram)

    /* Embedded assets */
    SHORT(ORIGIN(board_bitmap))
    BYTE(ORIGIN(board_bitmap) >> 16)
    SHORT(SIZEOF(.video_board_bitmap))
    BYTE(SIZEOF(.video_board_bitmap) >> 16)
    TRIM(board_bitmap)

    /* Launch the program at _start. */
    SHORT(_start)
    LONG(0)
}