SECTIONS
{
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
        . = ALIGN(16);
        _end = .;
    }

    /DISCARD/ : {
        *(.dynamic);
    }
}
