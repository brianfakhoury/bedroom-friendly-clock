ENTRY(_start)
MEMORY { ram (rwx) : ORIGIN = 0x50000000, LENGTH = 16K
 hp (rwx) : ORIGIN = 0x40800000, LENGTH = 512K }
SECTIONS {
 .init : { *(.init.*) } > hp

 .text : { KEEP(*(.wake.entry)) KEEP(*(.text.entry)) *(.text*) } > ram
 .rodata : { *(.rodata*) *(.srodata*) . = ALIGN(4); _crc_end = .; } > ram
 .data : { *(.data*) *(.sdata*) } > ram
 . = ALIGN(16);
 __global_pointer$ = . + 0x800;
 .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(16); _bss_end = .; } > ram
 .noinit (NOLOAD) : { KEEP(*(.noinit*)) . = ALIGN(16); _retained_end = .; } > ram
 _stack_top = ORIGIN(ram) + LENGTH(ram);
 ASSERT(_retained_end < _stack_top - 4096, "RAM overflow")
 /DISCARD/ : { *(.eh_frame*) *(.comment*) }
}
