OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS
{
	text PT_LOAD FILEHDR PHDRS FLAGS(5);
	data PT_LOAD FLAGS(6);
	stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
	PROVIDE(__executable_start = SEGMENT_START("text-segment", 0x10000));
	. = SEGMENT_START("text-segment", 0x10000) + SIZEOF_HEADERS;

	.note.gnu.build-id : { *(.note.gnu.build-id) } :text

	.init :
	{
		KEEP(*(SORT_NONE(.init)))
	} :text

	.text :
	{
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.sorted.*))
		*(.text .stub .text.* .gnu.linkonce.t.*)
		*(.gnu.warning)
	} :text

	.fini :
	{
		KEEP(*(SORT_NONE(.fini)))
	} :text

	PROVIDE(__etext = .);
	PROVIDE(_etext = .);
	PROVIDE(etext = .);

	.rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) } :text
	.rodata1 : { *(.rodata1) } :text
	.sdata2 : { *(.sdata2 .sdata2.* .gnu.linkonce.s2.*) } :text
	.sbss2 : { *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*) } :text
	.eh_frame_hdr : { *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) } :text
	.eh_frame : ONLY_IF_RO { KEEP(*(.eh_frame)) *(.eh_frame.*) } :text
	.sframe : ONLY_IF_RO { *(.sframe) *(.sframe.*) } :text
	.gcc_except_table : ONLY_IF_RO { *(.gcc_except_table .gcc_except_table.*) } :text
	.gnu_extab : ONLY_IF_RO { *(.gnu_extab*) } :text
	.exception_ranges : ONLY_IF_RO { *(.exception_ranges*) } :text

	. = DATA_SEGMENT_ALIGN(CONSTANT(MAXPAGESIZE), CONSTANT(COMMONPAGESIZE));

	.eh_frame : ONLY_IF_RW { KEEP(*(.eh_frame)) *(.eh_frame.*) } :data
	.sframe : ONLY_IF_RW { *(.sframe) *(.sframe.*) } :data
	.gnu_extab : ONLY_IF_RW { *(.gnu_extab) } :data
	.gcc_except_table : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) } :data
	.exception_ranges : ONLY_IF_RW { *(.exception_ranges*) } :data

	.tdata :
	{
		PROVIDE_HIDDEN(__tdata_start = .);
		*(.tdata .tdata.* .gnu.linkonce.td.*)
	} :data

	.tbss : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) } :data

	.preinit_array :
	{
		PROVIDE_HIDDEN(__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN(__preinit_array_end = .);
	} :data

	.init_array :
	{
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
		KEEP(*(SORT(.ctors.*)))
		KEEP(*(.ctors))
		PROVIDE_HIDDEN(__init_array_end = .);
	} :data

	.fini_array :
	{
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		KEEP(*(.fini_array))
		KEEP(*(SORT(.dtors.*)))
		KEEP(*(.dtors))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} :data

	.data.rel.ro :
	{
		*(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
		*(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
	} :data

	.data :
	{
		__DATA_BEGIN__ = .;
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
	} :data

	.data1 : { *(.data1) } :data

	.sdata :
	{
		__SDATA_BEGIN__ = .;
		*(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
		*(.srodata .srodata.*)
		*(.sdata .sdata.* .gnu.linkonce.s.*)
	} :data

	_edata = .;
	PROVIDE(edata = .);

	. = ALIGN(32 / 8);
	__bss_start = .;

	.sbss :
	{
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
	} :data

	.bss :
	{
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(. != 0 ? 32 / 8 : 1);
	} :data

	. = ALIGN(32 / 8);
	. = SEGMENT_START("ldata-segment", .);
	. = ALIGN(32 / 8);

	__BSS_END__ = .;
	__global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800,
		MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));

	_end = .;
	PROVIDE(end = .);
	. = DATA_SEGMENT_END(.);

	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.gnu_object_only) }
}
