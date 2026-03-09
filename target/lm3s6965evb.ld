/* Linker script for lm3s6965evb (QEMU Cortex-M3)
 * FLASH: 512 KiB starting at 0x00000000
 * RAM:    64 KiB starting at 0x20000000
 *
 * Memory map matches QEMU's lm3s6965evb machine.
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Top of stack = end of RAM */
_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS {
    /* -----------------------------------------------------------------------
     * Vector table must be first so the Cortex-M3 reset loads it from 0x0.
     */
    .vectors : {
        KEEP(*(.vectors))
    } > FLASH

    /* -----------------------------------------------------------------------
     * Code and read-only data
     */
    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _text_end = .;
    } > FLASH

    /* -----------------------------------------------------------------------
     * ARM exception index (required by libgcc unwinder even with -fno-exceptions)
     */
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* -----------------------------------------------------------------------
     * Initialised data: stored in FLASH (LMA), copied to RAM (VMA) by startup
     */
    _data_load = LOADADDR(.data);
    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM AT > FLASH

    /* -----------------------------------------------------------------------
     * Uninitialised data: zeroed by startup
     */
    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
        end = .;        /* heap start for _sbrk (newlib/rdimon) */
    } > RAM

    /* -----------------------------------------------------------------------
     * Stack grows downward from _stack_top (defined above)
     */
}
