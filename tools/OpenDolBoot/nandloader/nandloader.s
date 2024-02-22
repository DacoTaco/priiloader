#include <ogc/machine/asm.h>

#special machine state registers definition
#define	sr0		0
#define	sr1		1
#define	sr2		2
#define	sr3		3
#define	sr4		4
#define	sr5		5
#define	sr6		6
#define	sr7		7
#define	sr8		8
#define	sr9		9
#define	sr10	10
#define	sr11	11
#define	sr12	12
#define	sr13	13
#define	sr14	14
#define	sr15	15

.section	.text.startup
.globl _start
.globl _main
.align 2
_start:
	b 		_main
	
parameters:
version:
	.long	0x00000001
entrypoint:
	.long	0x81000000

_main:
	// HID0 = 00110c64:
	// bus checkstops off, sleep modes off,
	// caches off, caches invalidate,
	// store gathering off, enable data cache
	// flush assist, enable branch target cache,
	// enable branch history table
	lis		r3, 0x11
	addi	r3, r3, 0xC64
	mtspr	HID0, r3
	isync
	// MSR = 00002000 (FP on)
	li		r4, 0x2000
	mtmsr	r4, 0
	// HID0 |= 0000c000 (caches on)
	ori     r3, r3, 0xC000
	mtspr   HID0, r3
	isync

	// clear IBAT0..5 & DBAT0...5
	li		r4, 0x0
	mtspr	DBAT0U, r4
	mtspr	DBAT1U, r4
	mtspr	DBAT2U, r4
	mtspr	DBAT3U, r4
	mtspr	DBAT4U, r4
	mtspr	DBAT5U, r4
	mtspr	IBAT0U, r4
	mtspr	IBAT1U, r4
	mtspr	IBAT2U, r4
	mtspr	IBAT3U, r4
	mtspr	IBAT4U, r4
	mtspr	IBAT5U, r4
	isync

	// clear all SRs
	lis		r4, -0x8000
	mtsr	sr0, r4
	mtsr	sr1, r4
	mtsr	sr2, r4
	mtsr	sr3, r4
	mtsr	sr4, r4
	mtsr	sr5, r4
	mtsr	sr6, r4
	mtsr	sr7, r4
	mtsr	sr8, r4
	mtsr	sr9, r4
	mtsr	sr10, r4
	mtsr	sr11, r4
	mtsr	sr12, r4
	mtsr	sr13, r4
	mtsr	sr14, r4
	mtsr	sr15, r4
	isync

	// set [DI]BAT0 for 256MB@80000000,
	// real 00000000, WIMG=0000, R/W
	li		r3, 0x02
	lis		r4, -0x8000
	ori		r4, r4, 0x1FFF
	mtspr	DBAT0L, r3
	mtspr	DBAT0U, r4
	mtspr	IBAT0L, r3
	mtspr	IBAT0U, r4
	isync

	//setup more BAT stuff
	li		r4, 0x2A
	lis		r3, -0x4000
	addi	r3, r3, 0x1FFF
	mtspr	DBAT1L, r4
	mtspr	DBAT1U, r3
	isync

	//setup Mem2 BAT's
	lis		r4, 0x1000
	addi	r4, r4, 0x2
	lis		r3, -0x7000
	addi	r3, r3, 0x1fff
	mtspr	DBAT4L, r4
	mtspr	DBAT4U, r3
	mtspr	IBAT4L, r4
	mtspr	IBAT4U, r3
	isync

	//more BAT/cache setup?
	lis		r4, 0x1000
	addi	r4, r4, 0x2A
	lis		r3, -0x3000
	addi	r3, r3, 0x1fff
	mtspr	DBAT5L, r4
	mtspr	DBAT5U, r3
	isync
	li		r4, 0x01
	li		r3, 0x1F
	mtspr	DBAT3L, r4
	mtspr	DBAT3U, r3
	mtspr	IBAT3L, r4
	mtspr	IBAT3U, r3
	isync

	// idk tbh
	mfmsr	r4
	ori		r4, r4, 0x30
	mtmsr	r4, 0
	isync

	//write read bi2.bin location (null) & jump to the entrypoint
	lis		r3, -0x8000
	li		r4, 0x0
	stw		r4, 0xf4(r3)
	li		r4, entrypoint@l
	lwz		r4, 0x3400(r4)
	mtlr	r4
	blr