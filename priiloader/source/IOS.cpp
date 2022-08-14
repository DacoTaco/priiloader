//compiler includes
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <ogc/machine/processor.h>

//Project includes
#include "mem2_manager.h"
#include "gecko.h"
#include "error.h"
#include "state.h"

//IOS Patching structs
typedef void (*Iospatcher)(u8* address);
typedef struct _IosPatch {
	std::vector<u8> Pattern;
	Iospatcher Patch;
} IosPatch;

//IOS Patches
static const IosPatch AhbProtPatcher = {
	//Pattern
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
		*(u8*)(address + 8) = 0x23;
		*(u8*)(address + 9) = 0xFF;
	}
};

//nand permissions : 42 8b d0 01 25 66 -> 42 8b e0 01 25 66
static const IosPatch NandAccessPatcher = {
	//Pattern
	{
			  0x42, 0x8B,
			  0xD0, 0x01,
			  0x25, 0x66 
	},
	//Apply Patch
	[](u8* address) {
		*(u8*)(address + 2) = 0xE0;
		*(u8*)(address + 3) = 0x01;
	}
};

bool isIOSstub(u8 ios_number)
{
	u32 tmd_size = 0;
	tmd_view *ios_tmd;

	ES_GetTMDViewSize(0x0000000100000000ULL | ios_number, &tmd_size);
	if (!tmd_size)
	{
		//getting size failed. invalid or fake tmd for sure!
		gdprintf("isIOSstub : ES_GetTMDViewSize fail,ios %d",ios_number);
		return true;
	}
	ios_tmd = (tmd_view *)mem_align( 32, ALIGN32(tmd_size) );
	if(!ios_tmd)
	{
		gdprintf("isIOSstub : TMD alloc failure");
		return true;
	}
	memset(ios_tmd , 0, tmd_size);
	ES_GetTMDView(0x0000000100000000ULL | ios_number, (u8*)ios_tmd , tmd_size);
	gdprintf("isIOSstub : IOS %d is rev %d(0x%x) with tmd size of %u and %u contents",ios_number,ios_tmd->title_version,ios_tmd->title_version,tmd_size,ios_tmd->num_contents);
	/*Stubs have a few things in common:
	- title version : it is mostly 65280 , or even better : in hex the last 2 digits are 0. 
		example : IOS 60 rev 6400 = 0x1900 = 00 = stub
	- exception for IOS21 which is active, the tmd size is 592 bytes (or 140 with the views)
	- the stub ios' have 1 app of their own (type 0x1) and 2 shared apps (type 0x8001).
	eventho the 00 check seems to work fine , we'll only use other knowledge as well cause some
	people/applications install an ios with a stub rev >_> ...*/
	u8 Version = ios_tmd->title_version;
	//version now contains the last 2 bytes. as said above, if this is 00, its a stub
	if ( Version == 0 )
	{
		if ( ( ios_tmd->num_contents == 3) && (ios_tmd->contents[0].type == 1 && ios_tmd->contents[1].type == 0x8001 && ios_tmd->contents[2].type == 0x8001) )
		{
			gdprintf("isIOSstub : %d == stub",ios_number);
			mem_free(ios_tmd);
			return true;
		}
		else
		{
			gdprintf("isIOSstub : %d != stub",ios_number);
			mem_free(ios_tmd);
			return false;
		}
	}
	gdprintf("isIOSstub : %d != stub",ios_number);
	mem_free(ios_tmd);
	return false;
}

s8 PatchIOS(std::vector<IosPatch> patches)
{
	if (read32(0x0d800064) != 0xFFFFFFFF)
		return -1;

	if (read16(0x0d8b420a))
		write16(0x0d8b420a, 0); //there is more you can do to make more available but meh, not needed

	if (read16(0x0d8b420a))
		return -2;

	if (patches.size() == 0)
		return 0;

	u8* mem_block = (u8*)read32(0x80003130);
	u32 patchesFound = 0;
	while ((u32)mem_block < 0x93FFFFFF)
	{
		std::for_each(patches.begin(), patches.end(), [&patchesFound, mem_block](const IosPatch& iosPatch)
		{
			s32 patchSize = iosPatch.Pattern.size();
			if(!memcmp(mem_block, &iosPatch.Pattern[0], patchSize))
			{
				iosPatch.Patch(mem_block);

				u8* address = (u8*)(((u32)mem_block) >> 5 << 5);
				DCFlushRange(address, (patchSize >> 5 << 5) + 64);
				ICInvalidateRange(address, (patchSize >> 5 << 5) + 64);
				patchesFound++;
			}
		});

		if (patchesFound == patches.size())
			break;

		mem_block++;
	}

	return patchesFound;
}

s32 ReloadIOS(s32 iosToLoad, s8 keepAhbprot)
{
	if (keepAhbprot)
		PatchIOS({ AhbProtPatcher });

	IOS_ReloadIOS(iosToLoad);

	if(system_state.ReloadedIOS == 0)
		system_state.ReloadedIOS = read32(0x0d800064) != 0xFFFFFFFF;

	if (iosToLoad != IOS_GetVersion())
		return -1;

	// Any IOS < 28 does not have to required ES calls to get a title TMD, which sucks.
	// Therefor we will patch in NAND Access so we can load the TMD directly from nand.
	// Nasty fix, but this fixes System menu 1.0, which uses IOS9
	if (iosToLoad < 28)
	{
		gprintf("IOS < 28, patching in NAND Access");
		PatchIOS({ NandAccessPatcher });
	}

	return iosToLoad;
}