/* Linker script for STM32WL55JCxx (Cortex-M4 application core)
 * FLASH: 256 KB at 0x08000000
 * SRAM1:  64 KB at 0x20000000
 *
 * EEPROM emulation pages — default: last 4 KB of 256 KB flash (pages 126-127).
 * To place EEPROM anywhere else:
 *   1. Change EEPROM ORIGIN to the first page's base address.
 *   2. Change FLASH LENGTH to exclude those pages (split into two regions if
 *      EEPROM is in the middle — see comment below).
 *   3. Pass matching page indices to the compiler:
 *        -DEEPROM_PAGE0_IDX=30 -DEEPROM_PAGE1_IDX=31   (JAxx,  64 KB, last 4 KB)
 *        -DEEPROM_PAGE0_IDX=62 -DEEPROM_PAGE1_IDX=63   (JBxx, 128 KB, last 4 KB)
 *
 * Mid-flash example (pages 60-61 of a 128-page device):
 *   FLASH_1 (rx) : ORIGIN = 0x08000000, LENGTH = 120K   -- 60 pages x 2 KB
 *   EEPROM  (r)  : ORIGIN = 0x0801E000, LENGTH = 4K     -- pages 60-61
 *   FLASH_2 (rx) : ORIGIN = 0x08020000, LENGTH = 136K   -- remaining pages
 *   (distribute .vectors/.text/.rodata between FLASH_1 and FLASH_2 as needed)
 */

MEMORY {
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 252K   /* 256K - 4K EEPROM */
    EEPROM (r)   : ORIGIN = 0x080FC000, LENGTH = 4K     /* pages 126-127    */
    RAM    (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS {
    .vectors : {
        KEEP(*(.vectors))
    } > FLASH

    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _text_end = .;
    } > FLASH

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    _data_load = LOADADDR(.data);
    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM AT > FLASH

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
        end = .;
    } > RAM

    /* EEPROM emulation pages — reserved, never linked with code.
     * The linker errors if any section would overlap this region. */
    .eeprom_reserved (NOLOAD) : {
        . += LENGTH(EEPROM);
    } > EEPROM
}
