// Minimal Cortex-M4 vector table + reset handler
// Target: STM32WL55JCxx (CM4 application core)
//
// Copies .data from flash to RAM, zeros .bss, then calls main().
// Using soft-float ABI (-mfloat-abi=soft) — no FPU initialisation needed.

    .syntax unified
    .cpu cortex-m4
    .thumb

// ---------------------------------------------------------------------------
// Vector table — placed at 0x08000000 (first thing in FLASH)
// ---------------------------------------------------------------------------
    .section .vectors, "a", %progbits
    .type _vectors, %object
_vectors:
    .word _stack_top        // 0: Initial stack pointer
    .word reset_handler     // 1: Reset
    .word default_handler   // 2: NMI
    .word default_handler   // 3: HardFault
    .word default_handler   // 4: MemManage
    .word default_handler   // 5: BusFault
    .word default_handler   // 6: UsageFault
    .word 0                 // 7-10: Reserved
    .word 0
    .word 0
    .word 0
    .word default_handler   // 11: SVCall
    .word default_handler   // 12: DebugMonitor
    .word 0                 // 13: Reserved
    .word default_handler   // 14: PendSV
    .word default_handler   // 15: SysTick

// ---------------------------------------------------------------------------
// Reset handler
// ---------------------------------------------------------------------------
    .section .text.reset_handler, "ax", %progbits
    .type reset_handler, %function
reset_handler:
    // Copy .data from flash LMA to RAM VMA
    ldr  r0, =_data_start
    ldr  r1, =_data_end
    ldr  r2, =_data_load
    b    .copy_check
.copy_loop:
    ldr  r3, [r2], #4
    str  r3, [r0], #4
.copy_check:
    cmp  r0, r1
    blt  .copy_loop

    // Zero .bss
    ldr  r0, =_bss_start
    ldr  r1, =_bss_end
    mov  r2, #0
    b    .zero_check
.zero_loop:
    str  r2, [r0], #4
.zero_check:
    cmp  r0, r1
    blt  .zero_loop

    bl   main

.loop_forever:
    b    .loop_forever

    .size reset_handler, . - reset_handler

// ---------------------------------------------------------------------------
// Default handler — infinite loop for any unhandled interrupt
// ---------------------------------------------------------------------------
    .section .text.default_handler, "ax", %progbits
    .type default_handler, %function
default_handler:
    b    default_handler
    .size default_handler, . - default_handler

    .end
