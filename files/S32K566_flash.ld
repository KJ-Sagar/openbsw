/* =============================================================================
 * FILE    : bsp/s32k566/linker/S32K566_flash.ld
 * PURPOSE : GNU ld linker script for NXP S32K566.
 *
 * Memory map (S32K566 — code MRAM variant):
 *   Code  MRAM : 0x00400000 – 0x007FFFFF  (4 MB)
 *   Data  MRAM : 0x10000000 – 0x1007FFFF  (512 KB)
 *   SRAM  (L)  : 0x20000000 – 0x2007FFFF  (512 KB — SRAM_L)
 *   SRAM  (U)  : 0x20080000 – 0x200FFFFF  (512 KB — SRAM_U)
 *   I-TCM      : 0x00000000 – 0x0000FFFF  (64 KB — fast zero-wait)
 *   D-TCM      : 0x20000000 – 0x2000FFFF  (available as D-TCM on M7 core 0)
 *
 * For this application we place:
 *   .isr_vector + .text  → Code MRAM
 *   .data (load)         → Code MRAM  (copied to SRAM at startup)
 *   .data (run)          → SRAM_L
 *   .bss                 → SRAM_L
 *   Stack                → SRAM_U (grows downward)
 * ============================================================================= */

ENTRY(Reset_Handler)

MEMORY {
    MRAM  (rx)  : ORIGIN = 0x00400000, LENGTH = 4M
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 1M
}

/* Stack size — increase for complex applications */
STACK_SIZE = 0x2000;   /* 8 KB */
HEAP_SIZE  = 0x1000;   /* 4 KB */

SECTIONS {

    /* -------------------------------------------------------------------------
     * Vector table — must be at base of MRAM (boot address)
     * ---------------------------------------------------------------------- */
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > MRAM

    /* -------------------------------------------------------------------------
     * Code and read-only data
     * ---------------------------------------------------------------------- */
    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > MRAM

    /* ARM exception unwind tables */
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > MRAM

    /* C++ constructors / destructors called before/after main() */
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > MRAM

    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > MRAM

    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > MRAM

    /* -------------------------------------------------------------------------
     * Initialised data — load from MRAM, run in SRAM
     * ---------------------------------------------------------------------- */
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > MRAM

    __data_load_start = LOADADDR(.data);

    /* -------------------------------------------------------------------------
     * Uninitialised data — zeroed by startup code
     * ---------------------------------------------------------------------- */
    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    /* -------------------------------------------------------------------------
     * Heap
     * ---------------------------------------------------------------------- */
    .heap : {
        . = ALIGN(8);
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
        . = ALIGN(8);
    } > SRAM

    /* -------------------------------------------------------------------------
     * Stack — placed at top of SRAM; grows downward
     * ---------------------------------------------------------------------- */
    .stack : {
        . = ALIGN(8);
        __stack_bottom = .;
        . += STACK_SIZE;
        __stack_top = .;
        . = ALIGN(8);
    } > SRAM

    /* -------------------------------------------------------------------------
     * Debug sections (not loaded)
     * ---------------------------------------------------------------------- */
    .debug_info     0 : { *(.debug_info)     }
    .debug_abbrev   0 : { *(.debug_abbrev)   }
    .debug_aranges  0 : { *(.debug_aranges)  }
    .debug_ranges   0 : { *(.debug_ranges)   }
    .debug_line     0 : { *(.debug_line)     }
    .debug_str      0 : { *(.debug_str)      }
    .debug_frame    0 : { *(.debug_frame)    }
    .debug_loc      0 : { *(.debug_loc)      }
}

/* Provide symbols expected by newlib stubs (optional) */
PROVIDE(end = __bss_end);
