ENTRY(_start)
SECTIONS {
    . = 1M;

    .text ALIGN(4K) : {
        boot/boot.o(.text)
        *(.text) *(.text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata) *(.rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data) *(.data.*)
    }

    .bss ALIGN(4K) : {
        __bss_start = .;
        *(COMMON)
        *(.bss) *(.bss.*)
        __bss_end = .;
    }

    kernel_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
