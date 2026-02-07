ENTRY(_start)

SECTIONS
{
  /* Keep .bss within short PC-relative reach for bring-up toolchains. */
  . = 0x00008000;
  .bss : {
    *(.bss*)
    *(COMMON)
  }

  . = 0x00010000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
}

