#this is our stack!
#we abusing the .fini section so it gets placed after the text section by the linker
.section	".fini"
.globl __stack
__stack:
	.space 0x100
