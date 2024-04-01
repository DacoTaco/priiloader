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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include <gccore.h>
#include <ogc/machine/processor.h>

#include "IOS.h"
#include "gecko.h"

#ifndef ALIGN32
#define ALIGN32(x) (((x) + 31) & ~31)
#endif

#define SRAMADDR(x) (0x0d400000 | ((x) & 0x000FFFFF))

//IOS Patches

//Ahbprot : 68 1B -> 0x23 0xFF
const IosPatch AhbProtPatcher = {
	//Pattern - patch by tuedj
	{
		0x68, 0x5B,				// ldr r3,[r3,#4]  ; get TMD pointer
		0x22, 0xEC, 0x00, 0x52,	// movls r2, 0x1D8
		0x18, 0x9B,				// adds r3, r3, r2 ; add offset of access rights field in TMD
		0x68, 0x1B,				// ldr r3, [r3]    ; load access rights (haxxme!)
		0x46, 0x98,				// mov r8, r3      ; store it for the DVD video bitcheck later
		0x07, 0xDB				// lsls r3, r3, #31; check AHBPROT bit
	},

	//Apply Patch
	[](u8* address) {
		gprintf("Found ES_AHBPROT check @ 0x%X, patching...", address);
		WriteRegister8((u32)address + 8, 0x23); // li r3, 0xFF.aka, make it look like the TMD had max settings
		WriteRegister8((u32)address + 9, 0xFF);
	}
};

//setuid : 0xD1, 0x2A, 0x1C, 0x39 -> 0x46, 0xC0, 0x1C, 0x39
const IosPatch SetUidPatcher = {
	//Pattern
	{
		0xD1, 0x2A, 
		0x1C, 0x39
	},

	//Apply Patch
	[](u8* address) {
		gprintf("Found SetUID @ 0x%X, patching...", address);
		WriteRegister8((u32)address, 0x46);
		WriteRegister8((u32)address + 1, 0xC0);
	}
};

//nand permissions : 42 8b d0 01 25 66 -> 42 8b e0 01 25 66
const IosPatch NandAccessPatcher = {
	//Pattern
	{
		0x42, 0x8B,
		0xD0, 0x01,
		0x25, 0x66
	},

	//Apply Patch
	[](u8* address) {
		gprintf("Found NAND Permission check @ 0x%X, patching...", address);
		WriteRegister8((u32)address + 2, 0xE0);
		WriteRegister8((u32)address + 3, 0x01);
	}
};

//Fakesign : 0x20, 0x07, 0x23, 0xA2  -> 0x20, 0x00, 0x23, 0xA2 || 0x20, 0x07, 0x4B, 0x0B -> 0x20, 0x00, 0x4B, 0x0B
const IosPatch FakeSignOldPatch = {
	//Pattern
	{
		0x20, 0x07,
		0x23, 0xA2
	},

	//Apply Patch
	[](u8* address)
	{
		gprintf("Found (old)fakesign check @ 0x%X, patching...", address);
		WriteRegister8((u32)address + 1, 0x00);
	}
};

const IosPatch FakeSignPatch = {
	//Pattern
	{
		0x20, 0x07,
		0x4B, 0x0B
	},

	//Apply Patch
	[](u8* address)
	{
		gprintf("Found fakesign check @ 0x%X, patching...", address);
		WriteRegister8((u32)address + 1, 0x00);
	}
};

//Es identify : D1 23 -> 00 00
const IosPatch EsIdentifyPatch = {
	//Pattern
	{
		0x68, 0x68,
		0x28, 0x03,
		0xD1, 0x23
	},

	//Apply Patch
	[](u8* address) {
		gprintf("Found EsIdentify check @ 0x%X, patching...", address);
		WriteRegister16((u32)address+4, 0);
	}
};

const IosPatch DebugRedirectionPatch = {
	//Pattern
	{
		0x46, 0x72,
		0x1C, 0x01,
		0x20, 0x05
	},

	//Apply Patch
	[](u8* address)
	{
		gprintf("patching DebugRedirectionPatch 0x%08X", (0xFFFF0000) | (u32)address);
		if ((((u32)address) & 0x90000000) != 0)
			return;

		u8 patch[] = {
			0xE9, 0xAD, 0x40, 0x1F, 0xE1, 0x5E, 0x30, 0xB2, 0xE2, 0x03, 0x30, 0xFF, 0xE3, 0x53, 0x00, 0xAB,
			0x1A, 0x00, 0x00, 0x07, 0xE3, 0x50, 0x00, 0x04, 0x1A, 0x00, 0x00, 0x05, 0xE5, 0x9F, 0x30, 0x58,
			0xE5, 0xD1, 0x20, 0x00, 0xEB, 0x00, 0x00, 0x04, 0xE2, 0x81, 0x10, 0x01, 0xE3, 0x52, 0x00, 0x00,
			0x1A, 0xFF, 0xFF, 0xFA, 0xE8, 0x3D, 0x40, 0x1F, 0xE1, 0xB0, 0xF0, 0x0E, 0xE3, 0xA0, 0x00, 0xD0,
			0xE5, 0x83, 0x00, 0x00, 0xE3, 0xA0, 0x02, 0x0B, 0xE1, 0x80, 0x0A, 0x02, 0xE5, 0x83, 0x00, 0x10,
			0xE3, 0xA0, 0x00, 0x19, 0xE5, 0x83, 0x00, 0x0C, 0xE5, 0x93, 0x00, 0x0C, 0xE3, 0x10, 0x00, 0x01,
			0x1A, 0xFF, 0xFF, 0xFC, 0xE5, 0x93, 0x00, 0x10, 0xE3, 0x10, 0x03, 0x01, 0xE3, 0xA0, 0x00, 0x00,
			0xE5, 0x83, 0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xF0, 0xE1, 0xA0, 0xF0, 0x0E, 0x0D, 0x80, 0x68, 0x14
		};

		for (u32 i = 0; i < sizeof(patch); i += 4)
			WriteRegister32(((u32)address) + i, *(u32*)&patch[i]);

		//redirect svc handler
		WriteRegister32(SRAMADDR(0xFFFF0028), (0xFFFF0000) | (u32)address);
		DCFlushRange((void*)SRAMADDR(0xFFFF0028), 16);
		ICInvalidateRange((void*)SRAMADDR(0xFFFF0028), 16);
	}
};

s8 IsIOSstub(u8 ios_number)
{
	u32 tmd_size = 0;
	ES_GetTMDViewSize(0x0000000100000000ULL | ios_number, &tmd_size);
	if (!tmd_size)
	{
		//getting size failed. invalid or fake tmd for sure!
		gdprintf("isIOSstub : ES_GetTMDViewSize fail,ios %d", ios_number);
		return 1;
	}

	STACK_ALIGN(tmd_view, ios_tmd, ALIGN32(tmd_size), 32);
	if (!ios_tmd)
	{
		gdprintf("isIOSstub : TMD alloc failure");
		return 1;
	}
	memset(ios_tmd, 0, tmd_size);
	ES_GetTMDView(0x0000000100000000ULL | ios_number, (u8*)ios_tmd, tmd_size);
	gdprintf("isIOSstub : IOS %d is rev %d(0x%x) with tmd size of %u and %u contents", ios_number, ios_tmd->title_version, ios_tmd->title_version, tmd_size, ios_tmd->num_contents);
	/*Stubs have a few things in common:
	- title version : it is mostly 65280 , or even better : in hex the last 2 digits are 0.
		example : IOS 60 rev 6400 = 0x1900 = 00 = stub
	- exception for IOS21 which is active, the tmd size is 592 bytes (or 140 with the views)
	- the stub ios' have 1 app of their own (type 0x1) and 2 shared apps (type 0x8001).
	eventho the 00 check seems to work fine , we'll only use other knowledge as well cause some
	people/applications install an ios with a stub rev >_> ...*/
	u8 Version = ios_tmd->title_version;
	//version now contains the last 2 bytes. as said above, if this is 00, its a stub
	if (Version == 0 && ios_tmd->num_contents == 3 && (ios_tmd->contents[0].type == 1 && ios_tmd->contents[1].type == 0x8001 && ios_tmd->contents[2].type == 0x8001))
	{
		gdprintf("isIOSstub : %d == stub", ios_number);
		return 1;
	}

	gdprintf("isIOSstub : %d != stub", ios_number);
	return 0;
}

s32 ReloadIOS(s32 iosToLoad, s8 keepAhbprot)
{
	s32 ret = keepAhbprot
		? PatchIOS({ AhbProtPatcher })
		: 1;

	if (ret <= 0)
		return ret;

	IOS_ReloadIOS(iosToLoad);

	if (ReadRegister32(0x0d800064) > 0 && IsUsbGeckoDetected())
		PatchIOSKernel({ DebugRedirectionPatch });

	return (iosToLoad != IOS_GetVersion())
		? -100
		: iosToLoad;
}
s8 PatchIOS(std::vector<IosPatch> patches)
{
	if (patches.size() == 0)
		return 0;

	//patching.
	/*TODO : according to sven writex is better then vu pointers (which failed before obviously) and even better would be to load ios(into memory) from nand,patch it, and let starlet load it
	(2:32:37 PM) sven_p:
		i've tried this a few days ago and the only stable thing seems to be to reload to a patched ios kernel
	(2:32:49 PM) sven_p:
		now i just kill ios, get a kernel from nand, patch it, and make starlet jump to it :)
	(2:33:47 PM) sven_p:
		in my experience: reload ios > write* >>> vu pointers
	(4:29:09 PM) sven_p:
		a) kill ios (disable irqs or something)
	(4:29:26 PM) sven_p:
		b) overwrite all thread contexts with a jump to 0x11000000 (or some other ptr in mem2)
	(4:29:32 PM) sven_p:
		c) load ios kernel from nand and apply patches*/

	if (ReadRegister32(0x0d800064) != 0xFFFFFFFF)
		return -1;

	if (ReadRegister16(0x0d8b420a))
		WriteRegister16(0x0d8b420a, 0); //there is more you can do to make more available but meh, not needed

	if (ReadRegister16(0x0d8b420a))
		return -2;

	//look in MEM2
	gprintf("Patching IOS in MEM2...");
	u8* mem_block = (u8*)ReadRegister32(0x80003130);
	u32 patchesFound = 0;
	while ((u32)mem_block < 0x93FFFFFF)
	{
		auto iterator = std::find_if(patches.begin(), patches.end(), [&patchesFound, mem_block](const IosPatch& iosPatch)
		{
			s32 patchSize = iosPatch.Pattern.size();
			if (memcmp(mem_block, &iosPatch.Pattern[0], patchSize) != 0)
				return false;

			//Apply patch
			iosPatch.Patch(mem_block);

			//flush cache
			u8* address = (u8*)(((u32)mem_block) >> 5 << 5);
			DCFlushRange(address, (patchSize >> 5 << 5) + 64);
			ICInvalidateRange(address, (patchSize >> 5 << 5) + 64);
			patchesFound++;
			return true;
		});

		if (iterator != patches.end() && patchesFound == patches.size())
			break;

		mem_block++;
	}

	WriteRegister16(0x0d8b420a, 1);
	return patchesFound;
}
s8 PatchIOSKernel(std::vector<IosPatch> patches)
{
	if (patches.size() == 0)
		return 0;

	if (ReadRegister32(0x0d800064) != 0xFFFFFFFF)
		return -1;

	if (ReadRegister16(0x0d8b420a))
		WriteRegister16(0x0d8b420a, 0); //there is more you can do to make more available but meh, not needed

	if (ReadRegister16(0x0d8b420a))
		return -2;

	gprintf("Patching IOS kernel...");
	u32 patchesFound = 0;
	u8* mem_block = (u8*)SRAMADDR(0xFFFF0000);
	while ((u32)mem_block < SRAMADDR(0xFFFFFFFF))
	{
		auto iterator = std::find_if(patches.begin(), patches.end(), [&patchesFound, mem_block](const IosPatch& iosPatch)
		{
			u32 patchSize = iosPatch.Pattern.size();
			u32 matches = 0;
			for (matches = 0; matches < patchSize; matches++)
			{
				if (ReadRegister8(((u32)mem_block) + matches) != iosPatch.Pattern[matches])
					break;
			}

			if (matches != patchSize)
				return false;

			//Apply patch
			iosPatch.Patch(mem_block);

			//flush cache
			u8* address = (u8*)(((u32)mem_block) >> 5 << 5);
			DCFlushRange(address, (matches >> 5 << 5) + 64);
			ICInvalidateRange(address, (matches >> 5 << 5) + 64);
			return true;
		});

		if (iterator != patches.end() && patchesFound == patches.size())
			break;

		mem_block++;
	}

	WriteRegister16(0x0d8b420a, 1);
	return patchesFound;
}