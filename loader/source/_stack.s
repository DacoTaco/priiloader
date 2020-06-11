#this is our stack!
#we are using the .sdata2 section for this. this is behind the text section where it belongs
.section	.sdata2
.globl __stack
__stack:
	.space 0x100
