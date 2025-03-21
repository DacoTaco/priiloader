/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2019  DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef ___IOS_H_
#define ___IOS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include <gccore.h>
#include <ogc/machine/processor.h>
#include <vector>

//IOS Patching structs
typedef void (*Iospatcher)(u8* address);
typedef struct _IosPatch {
	std::vector<u8> Pattern;
	Iospatcher Patch;
} IosPatch;

extern const IosPatch AhbProtPatcher;
extern const IosPatch SetUidPatcher;
extern const IosPatch NandAccessPatcher;
extern const IosPatch FakeSignOldPatch;
extern const IosPatch FakeSignPatch;
extern const IosPatch EsIdentifyPatch;

bool DisableAHBProt(void);
s8 IsIOSstub(u8 ios_number);
s32 ReloadIOS(s32 iosToLoad, s8 keepAhbprot);
s8 PatchIOS(std::vector<IosPatch> patches);
s8 PatchIOSKernel(std::vector<IosPatch> patches);

//note : these are less "safe" then libogc's but libogc forces uncached MEM1 addresses, even for writes.
//this can cause some issues sometimes when patching ios in mem2
static inline u8 ReadRegister8(u32 address)
{
	DCFlushRange((void*)(0xC0000000 | address), 2);
	return read8(address);
}

static inline u16 ReadRegister16(u32 address)
{
	DCFlushRange((void*)(0xC0000000 | address), 4);
	return read16(address);
}

static inline u32 ReadRegister32(u32 address)
{
	DCFlushRange((void*)(0xC0000000 | address), 8);
	return read32(address);
}

static inline void WriteRegister8(u32 address, u8 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("stb %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

static inline void WriteRegister16(u32 address, u16 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("sth %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

static inline void WriteRegister32(u32 address, u32 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("stw %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

#endif