//compiler includes
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include <ogc/machine/processor.h>

//Project includes
#include "mem2_manager.h"
#include "gecko.h"
#include "error.h"
#include "state.h"


bool isIOSstub(u8 ios_number)
{
	u32 tmd_size = 0;
	tmd_view *ios_tmd;

	ES_GetTMDViewSize(0x0000000100000000ULL | ios_number, &tmd_size);
	if (!tmd_size)
	{
		//getting size failed. invalid or fake tmd for sure!
		gdprintf("isIOSstub : ES_GetTMDViewSize fail,ios %d\n",ios_number);
		return true;
	}
	ios_tmd = (tmd_view *)mem_align( 32, ALIGN32(tmd_size) );
	if(!ios_tmd)
	{
		gdprintf("isIOSstub : TMD alloc failure\n");
		return true;
	}
	memset(ios_tmd , 0, tmd_size);
	ES_GetTMDView(0x0000000100000000ULL | ios_number, (u8*)ios_tmd , tmd_size);
	gdprintf("isIOSstub : IOS %d is rev %d(0x%x) with tmd size of %u and %u contents\n",ios_number,ios_tmd->title_version,ios_tmd->title_version,tmd_size,ios_tmd->num_contents);
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
			gdprintf("isIOSstub : %d != stub\n",ios_number);
			mem_free(ios_tmd);
			return false;
		}
	}
	gdprintf("isIOSstub : %d != stub\n",ios_number);
	mem_free(ios_tmd);
	return false;
}
s32 ReloadIos(s32 Ios_version,s8* bool_ahbprot_after_reload)
{
	//this function would not be possible without tjeuidj releasing his patch. its not that davebaol's patch is less safe, but teuidj's is cleaner. and clean == good
	if(
		((bool_ahbprot_after_reload != NULL) && *bool_ahbprot_after_reload > 0)
		&& read32(0x0d800064) == 0xFFFFFFFF)
	{
		if(read16(0x0d8b420a))
		{
			//there is more you can do to make more available but meh, not needed
			write16(0x0d8b420a, 0);
		}
		if(!( read16(0x0d8b420a) ) )
		{
			static const u16 es_set_ahbprot[] = {
			  0x685B,          // ldr r3,[r3,#4]  ; get TMD pointer
			  0x22EC, 0x0052,  // movls r2, 0x1D8
			  0x189B,          // adds r3, r3, r2 ; add offset of access rights field in TMD
			  0x681B,          // ldr r3, [r3]    ; load access rights (haxxme!)
			  0x4698,          // mov r8, r3      ; store it for the DVD video bitcheck later
			  0x07DB           // lsls r3, r3, #31; check AHBPROT bit
			};
			u8* mem_block = (u8*)read32(0x80003130);
			while((u32)mem_block < 0x93FFFFFF)
			{
				u32 address = (u32)mem_block;
				if (!memcmp(mem_block, es_set_ahbprot, sizeof(es_set_ahbprot)))
				{
					//pointers suck but do the trick. the installer uses a more safe method in its official, closed source version.but untill people start nagging ill use pointers
					*(u8*)(address+8) = 0x23;
					*(u8*)(address+9) = 0xFF;
					DCFlushRange((u8 *)((address) >> 5 << 5), (sizeof(es_set_ahbprot) >> 5 << 5) + 64);
					ICInvalidateRange((u8 *)((address) >> 5 << 5), (sizeof(es_set_ahbprot) >> 5 << 5) + 64);
					break;
				}
				mem_block++;
			}
		}
	}
	IOS_ReloadIOS(Ios_version);
	if (bool_ahbprot_after_reload != NULL)
	{
		if(read32(0x0d800064) == 0xFFFFFFFF)
		{
			*bool_ahbprot_after_reload = 1;
		}
		else
		{
			*bool_ahbprot_after_reload = 0;
			system_state.ReloadedIOS = 1;
		}
	}
	if(Ios_version == IOS_GetVersion())
		return Ios_version;
	else
		return -1;
}