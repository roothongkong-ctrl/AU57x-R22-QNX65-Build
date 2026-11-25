ENTRY(mu_started)
SECTIONS {
  . = 0x510100;
  .graft : { KEEP(*(.text.entry)) *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) *(.comment*) *(.note*) *(.ARM.attributes*) }
}
