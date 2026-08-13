SECTIONS
{
    . = 0x401ff000;
    .text : {
        *(.entry);
        *(.text)
        *(.text.*)
        *(.data)
        *(.data.*)
        *(.bss)
        *(.bss.*)
        *(.rodata)
        *(.rodata.*);
        . = ALIGN(4096) - 8;
        _end = .;
    }

    /DISCARD/ : {
        *(.dynamic);
    }
}
