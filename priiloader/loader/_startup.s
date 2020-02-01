#	this --MUST-- be the first code that is in the binary output
#	we insure this is the case by defining the code to go into the .text.startup section, placing it in front of all the rest.
#	when run, we will jump to addr 0 of the compiled code. if this at address 0x00, this will be the code run
#	Also, the arguments are stored in argument registers r3,r4,r5 & r6. 
#	we stay off them, so by the time we enter the main code, _boot, the arguments should still be valid

.section	".text.startup"
.globl _start
_start:
#set stack address
#we do this by retrieving the address we jumped to from ctr and adding the stack start address to it, followed by its size (minus the bytes needed as buffer)
#i am using r18 as buffer, since it looked to be unused at the time.
	mr		18,1
	mfctr	1
	addi	1,1,__stack@l
	addi	1,1,0xF4
#save current stack data
	stw		0,0(1)
	stw		18,4(1)
	mflr	18
	stw		18,8(1)
	isync
#jump to our main function
	bl _boot
#restore stack data and return
	lwz 0,0(1)
	lwz 18,8(1)
	mtlr 18
	lwz 18,4(1)
	mr	1,18
	blr
	