/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2013  crediar

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
//#define DEBUG
#define MAGIC_WORD_ADDRESS_1 0x8132FFFB
#define MAGIC_WORD_ADDRESS_2 0x817FEFF0

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>


#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>

#include <sys/dir.h>
#include <vector>
#include <algorithm>
#include <time.h>


//Project files
#include "../../Shared/svnrev.h"
#include "Global.h"
#include "settings.h"
#include "state.h"
#include "elf.h"
#include "error.h"
#include "hacks.h"
#include "font.h"
#include "gecko.h"
#include "password.h"
#include "sha1.h"
#include "HTTP_Parser.h"
#include "dvd.h"
#include "titles.h"
#include "mem2_manager.h"


//Bin includes
#include "certs_bin.h"
#include "stub_bin.h"

extern "C"
{
	extern void _unstub_start(void);
	extern void __exception_closeall();
}
typedef struct {
	unsigned int offsetText[7];
	unsigned int offsetData[11];
	unsigned int addressText[7];
	unsigned int addressData[11];
	unsigned int sizeText[7];
	unsigned int sizeData[11];
	unsigned int addressBSS;
	unsigned int sizeBSS;
	unsigned int entrypoint;
} dolhdr;
typedef struct _dol_settings 
{
	s8 HW_AHBPROT_bit;
	u8 argument_count;
	s32 arg_cli_lenght;
	char* arg_command_line;
}__attribute((packed))_dol_settings;
typedef struct {
	std::string app_name;
	std::string app_path;
	u8 HW_AHBPROT_ENABLED;
	std::vector<std::string> args;
}Binary_struct;

extern DVD_status DVD_state;
extern Settings *settings;
extern u8 error;
extern std::vector<hack_hash> hacks_hash;
extern u32 *states_hash;

typedef struct wii_state {
	s8 Shutdown:2;
	s8 BootSysMenu:2;
	s8 ReloadedIOS:2;
	s8 InMainMenu:2;
} wii_state;

wii_state system_state;
time_t startloop = 0;
u32 appentrypoint = 0;

//overwrite the weak variable in libogc that enables malloc to use mem2. this disables it
u32 MALLOC_MEM2 = 0;

s32 __IOS_LoadStartupIOS()
{
	return 0;
}
u8 DetectHBC( void )
{
    u64 *list;
    u32 titlecount;
    s32 ret;

    ret = ES_GetNumTitles(&titlecount);
    if(ret < 0)
	{
		gprintf("DetectHBC : ES_GetNumTitles failure\n");
		return 0;
	}

    list = (u64*)mem_align(32, ALIGN32( titlecount * sizeof(u64) ) );

    ret = ES_GetTitles(list, titlecount);
    if(ret < 0) {
		gprintf("DetectHBC :ES_GetTitles failure. error %d\n",ret);
		mem_free(list);
		return 0;
    }
	ret = 0;
	//lets check for known HBC title id's.
    for(u32 i=0; i<titlecount; i++) 
	{
		if (list[i] == 0x000100014C554C5ALL)
		{
			if(ret < 4)
				ret = 4;
		}
		if (list[i] == 0x00010001AF1BF516LL)
		{
			if(ret < 3)
				ret = 3;
		}
		if (list[i] == 0x000100014A4F4449LL)
		{
			if (ret < 2)
				ret = 2;
		}
        if (list[i] == 0x0001000148415858LL)
        {
			if (ret < 1)
				ret = 1;
        }
    }
	mem_free(list);
    if(ret < 1)
	{
		gprintf("DetectHBC: HBC not found\n");
	}
	return ret;
}
void LoadHBCStub ( void )
{
	//LoadHBCStub: Load HBC JODI reload Stub and change stub to haxx if needed. 
	//the first part of the title is at 0x800024CA (first 2 bytes) and 0x800024D2 (last 2 bytes)
	//HBC < 1.0.5 = HAXX or 4841 5858
	//HBC >= 1.0.5 = JODI or 4A4F 4449
	//HBC >= 1.0.7 = .... or AF1B F516

	//in the JODI stub 0x800024CA & 24D2 needed to be changed. in the new 0.10 its 0x80001F62 & 1F6A
	/*if ( *(vu32*)0x80001804 == 0x53545542 && *(vu32*)0x80001808 == 0x48415858 )
	{
		return;
	}*/
	//load Stub, contains JODI by default.
	memcpy((void*)0x80001800, stub_bin, stub_bin_size);
	DCFlushRange((void*)0x80001800,stub_bin_size);
	
	//see if changes are needed to change it to the right ID
	//TODO : try the "LOADKTHX" function of the stub instead of changing titleID
	//(9:16:33 AM) megazig: 0x80002760 has a ptr. write 'LOADKTHX' where that points
	//(9:16:49 AM) megazig: 0x80002764 has a pttr. write u64 titleid there
	//(12:55:06 PM) megazig: DacoTaco: the trick of overwriting their memset does. without nopping then memset it does not
	/*
	//disable the memset that disables this LOADKTHX function in the stub
	*(u32*)0x800019B0 = 0x60000000; 
	DCFlushRange((void*)0x800019A0, 0x20);
	//set up the LOADKTHX parameters
	*(u32*)(*(u32*)0x80002760) = 0x4c4f4144;
	*(u32*)((*(u32*)0x80002760)+4) = 0x4b544858;
	*(u32*)0x80002F08 = 0x00010001;
	*(u32*)0x80002F0C = 0x4441434F;
	DCFlushRange((void*)0x80001800,stub_bin_size);*/

    switch(DetectHBC())
	{
		default: //not good, no HBC was detected >_> lets keep the stub anyway
			gprintf("HBC stub : No HBC Detected!1.1.0 stub loaded by default\n");
			break;
		case 4:
			*(vu16*)0x80001F62 = 0x4C55;
			*(vu16*)0x80001F6A = 0x4C5A;
			DCFlushRange((void*)0x80001800,stub_bin_size);
			break;
		case 3:
			/**(vu16*)0x80001F62 = 0xAF1B;
			*(vu16*)0x80001F6A = 0xF516;
			DCFlushRange((void*)0x80001800,stub_bin_size);*/
			break;
		case 1: //HAXX
			*(vu16*)0x80001F62 = 0x4841;//"HA";
			*(vu16*)0x80001F6A = 0x5858;//"XX";
			DCFlushRange((void*)0x80001800,stub_bin_size);
			break;
		case 2: //JODI
			*(vu16*)0x80001F62 = 0x4A4F; //"JO";
			*(vu16*)0x80001F6A = 0x4449; //"DI";
			DCFlushRange((void*)0x80001800,stub_bin_size);
			break;
	}
	gprintf("HBC stub : Loaded\n");
	return;	
}
void UnloadHBCStub( void )
{
	//some apps apparently dislike it if the stub stays in memory but for some reason isn't active :/
	//this isn't used cause as odd as the bug sounds, it vanished...
	memset((void*)0x80001800, 0, stub_bin_size);
	DCFlushRange((void*)0x80001800,stub_bin_size);	
	return;
}
void ClearScreen()
{
	if( !SGetSetting(SETTING_BLACKBACKGROUND))
		VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
	else
		VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	printf("\x1b[5;0H");
	fflush(stdout);
	return;
}
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
void SysHackHashSettings( void )
{
	if( !LoadHacks_Hash(false) )
	{
		if(GetMountedValue() == 0)
		{
			PrintFormat( 1, TEXT_OFFSET("Failed to mount FAT device"), 208+16, "Failed to mount FAT device");
		}
		else
		{
			PrintFormat( 1, TEXT_OFFSET("Can't find fat:/apps/priiloader/hacks_hash.ini"), 208+16, "Can't find fat:/apps/priiloader/hacks_hash.ini");
		}
		PrintFormat( 1, TEXT_OFFSET("Can't find hacks_hash.ini on NAND"), 208+16+16, "Can't find hacks_hash.ini on NAND");
		sleep(5);
		return;
	}

//Count hacks for current sys version
	u32 HackCount=0;
	u32 SysVersion=GetSysMenuVersion();
	for( unsigned int i=0; i<hacks_hash.size(); ++i)
	{
		if( hacks_hash[i].max_version >= SysVersion && hacks_hash[i].min_version <= SysVersion)
		{
			HackCount++;
		}
	}

	if( HackCount == 0 )
	{
		PrintFormat( 1, TEXT_OFFSET("Couldn't find any hacks for"), 208, "Couldn't find any hacks for");
		PrintFormat( 1, TEXT_OFFSET("System Menu version:vxxx"), 228, "System Menu version:v%d", SysVersion );
		sleep(5);
		return;
	}

	u32 DispCount=HackCount;

	if( DispCount > 25 )
		DispCount = 25;

	u16 cur_off=0;
	s32 menu_off=0;
	bool redraw=true;
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

		if ( WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B )
		{
			break;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			if( cur_off == DispCount)
			{
				//first try to open the file on the SD card/USB, if we found it copy it, other wise skip
				s16 fail = 0;
				FILE *in = NULL;
				if (GetMountedValue() != 0)
				{
					in = fopen ("fat:/apps/priiloader/hacks_hash.ini","rb");
				}
				else
				{
					gprintf("no FAT device found\n");
				}
				if ( ( (GetMountedValue() & 2) && !__io_wiisd.isInserted() ) || ( (GetMountedValue() & 1) && !__io_usbstorage.isInserted() ) )
				{
					PrintFormat( 0, 103, rmode->viHeight-48, "saving failed : SD/USB error");
					continue;
				}
				if( in != NULL )
				{
					//Read in whole file & create it on nand
					fseek( in, 0, SEEK_END );
					u32 size = ftell(in);
					fseek( in, 0, 0);

					char *buf = (char*)mem_align( 32, ALIGN32(size) );
					memset( buf, 0, size );
					fread( buf, sizeof( char ), size, in );

					fclose(in);

					s32 fd = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1|2 );
					if( fd >= 0 )
					{
						//File already exists, delete and recreate!
						ISFS_Close( fd );
						if(ISFS_Delete("/title/00000001/00000002/data/hackshas.ini") <0)
						{
							fail=1;
							mem_free(buf);
							goto handle_hacks_fail;
						}
					}
					if(ISFS_CreateFile("/title/00000001/00000002/data/hackshas.ini", 0, 3, 3, 3)<0)
					{
						fail=2;
						mem_free(buf);
						goto handle_hacks_fail;
					}
					fd = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1|2 );
					if( fd < 0 )
					{
						fail=3;
						ISFS_Close( fd );
						mem_free(buf);
						goto handle_hacks_fail;
					}

					if(ISFS_Write( fd, buf, size )<0)
					{
						fail = 4;
						ISFS_Close( fd );
						mem_free(buf);
						goto handle_hacks_fail;
					}
					ISFS_Close( fd );
					mem_free(buf);
				}
handle_hacks_fail:
				if(fail > 0)
				{
					gprintf("hacks_hash.ini save error %d\n",fail);
				}
				
				s32 fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );

				if( fd >= 0 )
				{
					//File already exists, delete and recreate!
					ISFS_Close( fd );
					if(ISFS_Delete("/title/00000001/00000002/data/hacksh_s.ini")<0)
					{
						fail = 5;
						goto handle_hacks_s_fail;
					}
				}

				if(ISFS_CreateFile("/title/00000001/00000002/data/hacksh_s.ini", 0, 3, 3, 3)<0)
				{
					fail = 6;
					goto handle_hacks_s_fail;
				}
				fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );
				if( fd < 0 )
				{
					fail=7;
					goto handle_hacks_s_fail;
				}
				if(ISFS_Write( fd, states_hash, sizeof( u32 ) * hacks_hash.size() )<0)
				{
					fail = 8;
					ISFS_Close(fd);
					goto handle_hacks_s_fail;
				}

				ISFS_Close( fd );
handle_hacks_s_fail:
				if(fail > 0)
				{
					gprintf("hacksh_s.ini save error %d\n",fail);
				}

				if( fail > 0 )
					PrintFormat( 0, 118, rmode->viHeight-48, "saving failed");
				else
					PrintFormat( 0, 118, rmode->viHeight-48, "settings saved");
			} 
			else 
			{

				s32 j = 0;
				u32 i = 0;
				for(i=0; i<hacks_hash.size(); ++i)
				{
					if( hacks_hash[i].max_version >= SysVersion && hacks_hash[i].min_version <= SysVersion)
					{
						if( cur_off+menu_off == j++)
							break;
					}
				}

				if(states_hash[i])
					states_hash[i]=0;
				else 
					states_hash[i]=1;

				redraw = true;
			}
		}

		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;

			if( cur_off > DispCount )
			{
				cur_off--;
				menu_off++;
			}

			if( cur_off+menu_off > (s32)HackCount )
			{
				cur_off = 0;
				menu_off= 0;
			}
			
			redraw=true;
		} else if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			if( cur_off == 0 )
			{
				if( menu_off > 0 )
				{
					cur_off++;
					menu_off--;

				} else {

					//if( DispCount < 20 )
					//{
						cur_off=DispCount;
						menu_off=(HackCount-DispCount);

					//} else {

					//	cur_off=DispCount;
					//	menu_off=(HackCount-DispCount)-1;

					//}
				}
			}
			else
				cur_off--;
	
			redraw=true;
		}
		if( redraw )
		{
			//printf("\x1b[2;0HHackCount:%d DispCount:%d cur_off:%d menu_off:%d Hacks:%d   \n", HackCount, DispCount, cur_off, menu_off, hacks_hash.size() );
			u32 j=0;
			u32 skip=menu_off;
			for( u32 i=0; i<hacks_hash.size(); ++i)
			{
				if( hacks_hash[i].max_version >= SysVersion && hacks_hash[i].min_version <= SysVersion)
				{
					if( skip > 0 )
					{
						skip--;
					} else {
						
						PrintFormat( cur_off==j, 16, 48+j*16, "%s", hacks_hash[i].desc.c_str() );

						if( states_hash[i] )
						{
							PrintFormat( cur_off==j, 256, 48+j*16, "enabled ");
						}
						else
						{
							PrintFormat( cur_off==j, 256, 48+j*16, "disabled");
						}
						j++;
					}
				}
				if( j >= 25 ) 
					break;
			}
			PrintFormat( cur_off==(signed)DispCount, 118, rmode->viHeight-64, "save settings");
			PrintFormat( 0, 103, rmode->viHeight-48, "                            ");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}

	return;
}

void shellsort(u64 *a,int n)
{
	int j,i,m;
	u64 mid;
	for(m = n/2;m>0;m/=2)
	{
		for(j = m;j< n;j++)
		{
			for(i=j-m;i>=0;i-=m)
			{
				if(a[i+m]>=a[i])
					break;
				else
				{
					mid = a[i];
					a[i] = a[i+m];
					a[i+m] = mid;
				}
			}
		}
	}
}
void SetSettings( void )
{
	//clear screen and reset the background
	ClearScreen();

	//get a list of all installed IOSs
	u32 TitleCount = 0;
	ES_GetNumTitles(&TitleCount);
	u64 *TitleIDs=(u64*)mem_align(32, TitleCount * sizeof(u64) );
	ES_GetTitles(TitleIDs, TitleCount);
	shellsort(TitleIDs, TitleCount);

	//get ios
	unsigned int IOS_off=0;
	if( SGetSetting(SETTING_SYSTEMMENUIOS) == 0 )
	{
		for( unsigned int i=0; i < TitleCount; ++i)
		{
			if( (u32)(TitleIDs[i]>>32) != 0x00000001 )
				continue;

			if( GetSysMenuIOS() == (u32)(TitleIDs[i]&0xFFFFFFFF) )
			{
				IOS_off=i;
				break;
			}
		}

	} else {

		for( unsigned int i=0; i < TitleCount; ++i)
		{
			if( (u32)(TitleIDs[i]>>32) != 0x00000001 )
				continue;

			if( SGetSetting(SETTING_SYSTEMMENUIOS) == (u32)(TitleIDs[i]&0xFFFFFFFF) )
			{
				IOS_off=i;
				break;
			}
		}
	}


	int cur_off=0;
	int redraw=true;
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0)  | PAD_ButtonsDown(1)  | PAD_ButtonsDown(2)  | PAD_ButtonsDown(3);

		if ( WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B )
		{
			LoadSettings();
			SetDumpDebug(SGetSetting(SETTING_DUMPGECKOTEXT));
			break;
		}
		switch( cur_off )
		{
			case 0:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT || PAD_Pressed & PAD_BUTTON_LEFT )
				{
					if( settings->autoboot == AUTOBOOT_DISABLED )
						settings->autoboot = AUTOBOOT_FILE;
					else
						settings->autoboot--;
					redraw=true;
				}else if ( WPAD_Pressed & WPAD_BUTTON_RIGHT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT || PAD_Pressed & PAD_BUTTON_RIGHT )
				{
					if( settings->autoboot == AUTOBOOT_FILE )
						settings->autoboot = AUTOBOOT_DISABLED;
					else
						settings->autoboot++;
					redraw=true;
				}
			} break;
			case 1:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					settings->ReturnTo++;
					if( settings->ReturnTo > RETURNTO_AUTOBOOT )
						settings->ReturnTo = RETURNTO_SYSMENU;

					redraw=true;
				} else if ( WPAD_Pressed & WPAD_BUTTON_LEFT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT || PAD_Pressed & PAD_BUTTON_LEFT ) {

					if( settings->ReturnTo == RETURNTO_SYSMENU )
						settings->ReturnTo = RETURNTO_AUTOBOOT;
					else
						settings->ReturnTo--;

					redraw=true;
				}


			} break;
			case 2:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->ShutdownToPreloader )
						settings->ShutdownToPreloader = 0;
					else 
						settings->ShutdownToPreloader = 1;

					redraw=true;
				}


			} break;
			case 3:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->StopDisc )
						settings->StopDisc = 0;
					else 
						settings->StopDisc = 1;

					redraw=true;
				}

			} break;
			case 4:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->LidSlotOnError )
						settings->LidSlotOnError = 0;
					else 
						settings->LidSlotOnError = 1;
				
					redraw=true;
				}


			} break;
			case 5:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->IgnoreShutDownMode )
						settings->IgnoreShutDownMode = 0;
					else 
						settings->IgnoreShutDownMode = 1;
				
					redraw=true;
				}


			} break;
			case 6:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->BlackBackground )
					{
						settings->BlackBackground = false;
					}
					else
					{
						settings->BlackBackground = true;
					}
					ClearScreen();
					redraw=true;
				}
			}
			break;
			case 7:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->PasscheckPriiloader )
					{
						settings->PasscheckPriiloader = false;
					}
					else
					{
						ClearScreen();
						PrintFormat( 1, TEXT_OFFSET("!!!!!WARNING!!!!!"), 208, "!!!!!WARNING!!!!!");
						PrintFormat( 1, TEXT_OFFSET("Setting Password can lock you out"), 228, "Setting Password can lock you out" );
						PrintFormat( 1, TEXT_OFFSET("off your own wii. proceed? (A = Yes, B = No)"), 248, "off your own wii. proceed? (A = Yes, B = No)" );
						while(1)
						{
							WPAD_ScanPads();
							PAD_ScanPads();
							u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
							u32 PAD_Pressed  = PAD_ButtonsDown(0)  | PAD_ButtonsDown(1)  | PAD_ButtonsDown(2)  | PAD_ButtonsDown(3);
							if(WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A)
							{
								settings->PasscheckPriiloader = true;
								break;
							}
							else if(WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B)
							{
								break;
							}
							VIDEO_WaitVSync();
						}
						ClearScreen();
						
					}
					redraw=true;
				}
			}
			break;
			case 8:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->PasscheckMenu )
					{
						settings->PasscheckMenu = false;
					}
					else
					{
						ClearScreen();
						PrintFormat( 1, TEXT_OFFSET("!!!!!WARNING!!!!!"), 208, "!!!!!WARNING!!!!!");
						PrintFormat( 1, TEXT_OFFSET("Setting Password can lock you out"), 228, "Setting Password can lock you out" );
						PrintFormat( 1, TEXT_OFFSET("off your own wii. proceed? (A = Yes, B = No)"), 248, "off your own wii. proceed? (A = Yes, B = No)" );
						while(1)
						{
							WPAD_ScanPads();
							PAD_ScanPads();
							u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
							u32 PAD_Pressed  = PAD_ButtonsDown(0)  | PAD_ButtonsDown(1)  | PAD_ButtonsDown(2)  | PAD_ButtonsDown(3);
							if(WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A)
							{
								settings->PasscheckMenu = true;
								break;
							}
							else if(WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B)
							{
								break;
							}
							VIDEO_WaitVSync();
						}
						ClearScreen();
					}
					redraw=true;
				}
			}
			break;
			case 9: //show Debug Info
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if ( settings->DumpGeckoText )
						settings->DumpGeckoText = 0;			
					else
						settings->DumpGeckoText = 1;
					SetDumpDebug(settings->DumpGeckoText);
					redraw=true;
				}
			break;
			case 10: //download beta updates
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if ( settings->ShowBetaUpdates )
						settings->ShowBetaUpdates = 0;
					else
						settings->ShowBetaUpdates = 1;
					redraw=true;
				}
			break;		
			case 11: //ignore ios reloading for system menu?
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT			||
					 PAD_Pressed & PAD_BUTTON_LEFT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT	|| 
					 WPAD_Pressed & WPAD_BUTTON_RIGHT			||
					 PAD_Pressed & PAD_BUTTON_RIGHT				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT	|| 
					 WPAD_Pressed & WPAD_BUTTON_A				||
					 WPAD_Pressed & WPAD_CLASSIC_BUTTON_A		|| 
					 PAD_Pressed & PAD_BUTTON_A
					)
				{
					if( settings->UseSystemMenuIOS )
					{
						settings->UseSystemMenuIOS = false;
						if(settings->SystemMenuIOS == 0)
						{
							u16 counter = 0;
							while( ((u32)(TitleIDs[IOS_off]&0xFFFFFFFF) < 3  || (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 256) && counter < 256 )
							{
								if( (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 256 )
									IOS_off--;
								else
									IOS_off++;
								counter++;
							}
							settings->SystemMenuIOS = (u32)(TitleIDs[IOS_off]&0xFFFFFFFF);
						}
					}
					else
					{
						settings->UseSystemMenuIOS = true;
					}
					redraw=true;
				}
			}
			break;
			case 12:		//	System Menu IOS
			{
				if ( WPAD_Pressed & WPAD_BUTTON_LEFT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT || PAD_Pressed & PAD_BUTTON_LEFT )
				{
					while(1)
					{
						IOS_off--;
						if( (signed)IOS_off <= 0 )
							IOS_off = TitleCount;
						if( (u32)(TitleIDs[IOS_off]>>32) == 0x00000001 && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 2 && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) < 255 )
							break;
					}

					settings->SystemMenuIOS = (u32)(TitleIDs[IOS_off]&0xFFFFFFFF);
#ifdef DEBUG
					isIOSstub(settings->SystemMenuIOS);
#endif

					redraw=true;
				} else if( WPAD_Pressed & WPAD_BUTTON_RIGHT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT || PAD_Pressed & PAD_BUTTON_RIGHT ) 
				{
					while(1)
					{
						IOS_off++;
						if( IOS_off >= TitleCount )
							IOS_off = 2;
						if( (u32)(TitleIDs[IOS_off]>>32) == 0x00000001 && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 2  && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) < 255 )
							break;
					}

					settings->SystemMenuIOS = (u32)(TitleIDs[IOS_off]&0xFFFFFFFF);
#ifdef DEBUG
					isIOSstub(settings->SystemMenuIOS);
#endif
					redraw=true;
				}

			} break;
			case 13:
			{
				if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
				{
					if( SaveSettings() )
						PrintFormat( 0, 114, 128+224+16, "settings saved");
					else
						PrintFormat( 0, 118, 128+224+16, "saving failed");
				}
			} break;

			default:
				cur_off = 0;
				break;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;
			if( (settings->UseSystemMenuIOS) && (cur_off == 12))
				cur_off++;
			if( cur_off >= 14)
				cur_off = 0;
			
			redraw=true;
		} else if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;
			if( (settings->UseSystemMenuIOS) && (cur_off == 12))
				cur_off--;
			if( cur_off < 0 )
				cur_off = 13;
			
			redraw=true;
		}
		if( redraw )
		{
			switch(settings->autoboot)
			{
				case AUTOBOOT_DISABLED:
					PrintFormat( cur_off==0, 0, 112,    "              Autoboot:          Disabled        ");
				break;

				case AUTOBOOT_SYS:
					PrintFormat( cur_off==0, 0, 112,    "              Autoboot:          System Menu     ");
				break;
				case AUTOBOOT_HBC:
					PrintFormat( cur_off==0, 0, 112,    "              Autoboot:          Homebrew Channel");
				break;

				case AUTOBOOT_BOOTMII_IOS:
					PrintFormat( cur_off==0, 0, 112,    "              Autoboot:          BootMii IOS     ");
				break;

				case AUTOBOOT_FILE:
					PrintFormat( cur_off==0, 0, 112,    "              Autoboot:          Installed File  ");
				break;
				default:
					settings->autoboot = AUTOBOOT_DISABLED;
					break;
			}

			switch( settings->ReturnTo )
			{
				case RETURNTO_SYSMENU:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          System Menu");
				break;
				case RETURNTO_PRIILOADER:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Priiloader  ");
				break;
				case RETURNTO_AUTOBOOT:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Autoboot   ");
				break;
				default:
					gdprintf("SetSettings : unknown return to value %d\n",settings->ReturnTo);
					settings->ReturnTo = RETURNTO_PRIILOADER;
					break;
			}
			
			//PrintFormat( 0, 16, 64, "Pos:%d", ((rmode->viWidth /2)-(strlen("settings saved")*13/2))>>1);

			PrintFormat( cur_off==2, 0, 128+(16*1), "           Shutdown to:          %s", settings->ShutdownToPreloader?"Priiloader":"off       ");
			PrintFormat( cur_off==3, 0, 128+(16*2), "  Stop disc on startup:          %s", settings->StopDisc?"on ":"off");
			PrintFormat( cur_off==4, 0, 128+(16*3), "   Light slot on error:          %s", settings->LidSlotOnError?"on ":"off");
			PrintFormat( cur_off==5, 0, 128+(16*4), "        Ignore standby:          %s", settings->IgnoreShutDownMode?"on ":"off");
			PrintFormat( cur_off==6, 0, 128+(16*5), "      Background Color:          %s", settings->BlackBackground?"Black":"White");
			PrintFormat( cur_off==7, 0, 128+(16*6), "    Protect Priiloader:          %s", settings->PasscheckPriiloader?"on ":"off");
			PrintFormat( cur_off==8, 0, 128+(16*7), "      Protect Autoboot:          %s", settings->PasscheckMenu?"on ":"off");
			PrintFormat( cur_off==9, 0, 128+(16*8), "     Dump Gecko output:          %s", settings->DumpGeckoText?"on ":"off");
			PrintFormat( cur_off==10,0, 128+(16*9), "     Show Beta Updates:          %s", settings->ShowBetaUpdates?"on ":"off");
			PrintFormat( cur_off==11,0, 128+(16*10),"   Use System Menu IOS:          %s", settings->UseSystemMenuIOS?"on ":"off");
			if(!settings->UseSystemMenuIOS)
			{
				PrintFormat( cur_off==12, 0, 128+(16*11), "     IOS to use for SM:          %d  ", (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) );
			}
			else
			{
				PrintFormat( cur_off==12, 0, 128+(16*11),	"                                        ");
			}
			PrintFormat( cur_off==13, 118, 128+224, "save settings");
			PrintFormat( 0, 114, 128+224+16, "                 ");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}
	mem_free(TitleIDs);
	return;
}
void LoadHBC( void )
{
	//Note By DacoTaco :check for (0x00010001/4A4F4449 - JODI) HBC id
	//or old one(0x00010001/148415858 - HAXX)
	//or latest 0x00010001/AF1BF516
	u64 TitleID = 0;
	switch (DetectHBC())
	{
		case 1: //HAXX
			gprintf("LoadHBC : HAXX detected\n");
			TitleID = 0x0001000148415858LL;
			break;
		case 2: //JODI
			gprintf("LoadHBC : JODI detected\n");
			TitleID = 0x000100014A4F4449LL;
			break;
		case 3: //0.7
			gprintf("LoadHBC : >=0.7 detected\n");
			TitleID = 0x00010001AF1BF516LL;
			break;
		case 4: //1.2 , 'LULZ'
			gprintf("LoadHBC : 1.2 detected\n");
			TitleID = 0x000100014C554C5ALL;
			break;
		default: //LOL nothing?
			error = ERROR_BOOT_HBC;
			return;
	}
	u32 cnt ATTRIBUTE_ALIGN(32);
	ES_GetNumTicketViews(TitleID, &cnt);
	tikview *views = (tikview *)mem_align( 32, sizeof(tikview)*cnt );
	ES_GetTicketViews(TitleID, views, cnt);
	ClearState();
	WPAD_Shutdown();
	ES_LaunchTitle(TitleID, &views[0]);
	//well that went wrong
	error = ERROR_BOOT_HBC;
	mem_free(views);
	return;
}
void LoadBootMii( void )
{
	//when this was coded on 6th of Oct 2009 Bootmii ios was in IOS slot 254
	PollDevices();
	if(isIOSstub(254))
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Bootmii(IOS254) Not found!"), 208, "Bootmii(IOS254) Not found!");
			sleep(5);
		}
		return;
	}
	if ( !(GetMountedValue() & 2) )
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Could not mount SD card"), 208, "Could not mount SD card");
			sleep(5);
		}
		return;
	}
	FILE* BootmiiFile = fopen("fat:/bootmii/armboot.bin","r");
	if (!BootmiiFile)
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Could not find fat:/bootmii/armboot.bin"), 208, "Could not find fat:/bootmii/armboot.bin");
			sleep(5);
		}
		return;
	}
	fclose(BootmiiFile);
		
	/*BootmiiFile = fopen("fat:/bootmii/ppcboot.elf","r");
	if(!BootmiiFile)
	{
		if(rmode != NULL)
		{	
			PrintFormat( 1, TEXT_OFFSET("Could not find fat:/bootmii/ppcboot.elf"), 208, "Could not find fat:/bootmii/ppcboot.elf");
			sleep(5);
		}
		return;
	}
	fclose(BootmiiFile);*/
	u8 currentIOS = IOS_GetVersion();
	for(u8 i=0;i<WPAD_MAX_WIIMOTES;i++) {
		if(WPAD_Probe(i,0) < 0)
			continue;
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}
	WPAD_Shutdown();
	IOS_ReloadIOS(254);
	//launching bootmii failed. lets wait a bit for the launch(it could be delayed) and then load the other ios back
	sleep(3);
	IOS_ReloadIOS(currentIOS);
	system_state.ReloadedIOS = 1;
	WPAD_Init();
	return;
}
s8 BootDolFromFat( FILE* fat_fd , u8 HW_AHBPROT_ENABLED, struct __argv *args )
{
	void	(*entrypoint)();
	Elf32_Ehdr ElfHdr;
	fread( &ElfHdr, sizeof( ElfHdr ), 1, fat_fd );
	if( ElfHdr.e_ident[EI_MAG0] == 0x7F &&
		ElfHdr.e_ident[EI_MAG1] == 'E' &&
		ElfHdr.e_ident[EI_MAG2] == 'L' &&
		ElfHdr.e_ident[EI_MAG3] == 'F' )
	{
		//ELF detected
		gdprintf("ELF Found\n");
		gdprintf("Type:      \t%04X\n", ElfHdr.e_type );
		gdprintf("Machine:   \t%04X\n", ElfHdr.e_machine );
		gdprintf("Version:  %08X\n", ElfHdr.e_version );
		gdprintf("Entry:    %08X\n", ElfHdr.e_entry );
		gdprintf("Flags:    %08X\n", ElfHdr.e_flags );
		gdprintf("EHsize:    \t%04X\n\n", ElfHdr.e_ehsize );

		gdprintf("PHoff:    %08X\n",	ElfHdr.e_phoff );
		gdprintf("PHentsize: \t%04X\n",	ElfHdr.e_phentsize );
		gdprintf("PHnum:     \t%04X\n\n",ElfHdr.e_phnum );

		gdprintf("SHoff:    %08X\n",	ElfHdr.e_shoff );
		gdprintf("SHentsize: \t%04X\n",	ElfHdr.e_shentsize );
		gdprintf("SHnum:     \t%04X\n",	ElfHdr.e_shnum );
		gdprintf("SHstrndx:  \t%04X\n\n",ElfHdr.e_shstrndx );
		
		if( ( (ElfHdr.e_entry | 0x80000000) >= 0x81000000 ) && ( (ElfHdr.e_entry | 0x80000000) <= 0x81330000) )
		{
			gprintf("BootDolFromFat : ELF entrypoint error: abort!\n");
			return -1;
		}

		if( ElfHdr.e_phnum == 0 )
			gdprintf("Warning program header entries are zero!\n");
		else 
		{
			//program headers
			for( int i=0; i < ElfHdr.e_phnum; ++i )
			{
				fseek( fat_fd, ElfHdr.e_phoff + sizeof( Elf32_Phdr ) * i, SEEK_SET );
				Elf32_Phdr phdr;
				fread( &phdr, sizeof( phdr ), 1, fat_fd );
				gdprintf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz );
				if(phdr.p_type == PT_LOAD )
				{
					fseek( fat_fd, phdr.p_offset, 0 );
					fread( (void*)(phdr.p_vaddr | 0x80000000), sizeof( u8 ), phdr.p_filesz, fat_fd);
					ICInvalidateRange ((void*)(phdr.p_vaddr | 0x80000000),phdr.p_filesz);
				}
			}
		}

		if( ElfHdr.e_shnum == 0 )
		{
			gdprintf("BootDolFromFat : Warning section header entries are zero!\n");
		} 
		else 
		{

			for( s32 i=0; i < ElfHdr.e_shnum; ++i )
			{
				fseek( fat_fd, ElfHdr.e_shoff + sizeof( Elf32_Shdr ) * i, SEEK_SET );

				Elf32_Shdr shdr;
				fread( &shdr, sizeof( shdr ), 1, fat_fd );

				if( shdr.sh_type == SHT_NULL )
					continue;

				if( shdr.sh_type > SHT_GROUP )
					gdprintf("Warning the type: %08X could be invalid!\n", shdr.sh_type );

				if( shdr.sh_flags & ~0xF0000007 )
					gdprintf("Warning the flag: %08X is invalid!\n", shdr.sh_flags );

				gdprintf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr.sh_type, shdr.sh_offset, shdr.sh_name, shdr.sh_addr, shdr.sh_size );
				
				if (shdr.sh_type == SHT_NOBITS)
				{
					fseek( fat_fd, shdr.sh_offset, 0 );
					fread( (void*)(shdr.sh_addr | 0x80000000), sizeof( u8 ), shdr.sh_size, fat_fd);
					DCFlushRange((void*)(shdr.sh_addr | 0x80000000),shdr.sh_size);
				}
			}
		}
		entrypoint = (void (*)())(ElfHdr.e_entry | 0x80000000);
	}
	else
	{
		//DOL detected
		dolhdr hdr;
		fseek( fat_fd, 0, 0);
		fread( &hdr, sizeof( dolhdr ), 1, fat_fd );
		if( hdr.entrypoint >= 0x81000000 && hdr.entrypoint <= 0x81330000 )
		{
			gprintf("BootDolFromFat : entrypoint/BSS error: abort!\n");
			fclose(fat_fd);
			return -1;
		}
		else if ( hdr.entrypoint < 0x80000000 )
		{
			gprintf("BootDolFromFat : bogus entrypoint of %u detected\n",hdr.entrypoint);
			fclose(fat_fd);
			return -2;
		}
		
		if(DVDCheckCover())
		{
			gprintf("BootDolFromFat : excecuting StopDisc Async...\n");
			DVDStopDisc(true);
		}
		else
		{
			gprintf("BootDolFromFat : Skipping StopDisc -> no drive or disc in drive\n");
		}
		//Text Sections
		for (s8 i = 0; i < 6; i++)
		{
			if( hdr.sizeText[i] && hdr.addressText[i] && hdr.offsetText[i] )
			{
				fseek( fat_fd, hdr.offsetText[i], SEEK_SET );
				fread( (void*)(hdr.addressText[i]), sizeof( char ), hdr.sizeText[i], fat_fd );
				DCFlushRange ((void *) hdr.addressText[i], hdr.sizeText[i]);
				DCInvalidateRange( (void*)(hdr.addressText[i]), hdr.sizeText[i] );
			}
		}
		// data sections
		for (s8 i = 0; i <= 10; i++)
		{
			if( hdr.sizeData[i] && hdr.addressData[i] && hdr.offsetData[i] )
			{
				fseek( fat_fd, hdr.offsetData[i], SEEK_SET );
				fread( (void*)(hdr.addressData[i]), sizeof( char ), hdr.sizeData[i], fat_fd );
				DCFlushRange( (void*)(hdr.addressData[i]), hdr.sizeData[i] );
			}
		}
		if( 
			( hdr.addressBSS + hdr.sizeBSS < 0x80F00000 ||(hdr.addressBSS > 0x81500000 && hdr.addressBSS + hdr.sizeBSS < 0x817FFFFF) ) &&
			hdr.addressBSS > 0x80003400 )
		{
			memset ((void *) hdr.addressBSS, 0, hdr.sizeBSS);
			DCFlushRange((void *) hdr.addressBSS, hdr.sizeBSS);
		}
		if (args != NULL && args->argvMagic == ARGV_MAGIC)
        {
			void* new_argv = (void*)(hdr.entrypoint + 8);
			memmove(new_argv, args, sizeof(__argv));
			DCFlushRange(new_argv, sizeof(__argv));
        }
		entrypoint = (void (*)())(hdr.entrypoint);
	}
	fclose( fat_fd );
	gprintf("BootDolFromFat : starting binary...\n");
	for (int i = 0;i < WPAD_MAX_WIIMOTES ;i++)
	{
		if(WPAD_Probe(i,0) > 0)
		{
			if(WPAD_Probe(i,0) < 0)
				continue;
			WPAD_Flush(i);
			WPAD_Disconnect(i);
		}
	}
	ClearState();
	WPAD_Shutdown();
	ShutdownDevices();
	if(DvdKilled() < 1)
	{
		gprintf("checking DVD drive...\n");
		while(DvdKilled() < 1);
	}

	s8 bAHBPROT = 0;
	s32 Ios_to_load = 0;
	if(read32(0x0d800064) == 0xFFFFFFFF )
		bAHBPROT = HW_AHBPROT_ENABLED;
	if( !isIOSstub(58) )
	{
		Ios_to_load = 58;
	}
	else if( !isIOSstub(61) )
	{
		Ios_to_load = 61;
	}
	else if( !isIOSstub(IOS_GetPreferredVersion()) )
	{
		Ios_to_load = IOS_GetPreferredVersion();
	}
	else
	{
		PrintFormat( 1, TEXT_OFFSET("failed to reload ios for homebrew! ios is a stub!"), 208, "failed to reload ios for homebrew! ios is a stub!");
		sleep(2);	
	}

	if(Ios_to_load > 2 && Ios_to_load < 255)
	{
		ReloadIos(Ios_to_load,&bAHBPROT);
		system_state.ReloadedIOS = 1;
	}

	gprintf("IOS state : ios %d - ahbprot : %d \n",IOS_GetVersion(),(read32(0x0d800064) == 0xFFFFFFFF ));
	gprintf("BootDolFromFat : Entrypoint: 0x%08X\n", (u32)(entrypoint) );

	ISFS_Deinitialize();
	ShutdownDevices();
	USB_Deinitialize();
	__IOS_ShutdownSubsystems();
	VIDEO_Flush();
	VIDEO_WaitVSync();
	__exception_closeall();
	ICSync();
	entrypoint();
	//it failed. FAIL!
	__IOS_InitializeSubsystems();
	PollDevices();
	ISFS_Initialize();
	WPAD_Init();
	PAD_Init();
	return -1;
}
s8 BootDolFromMem( u8 *dolstart , u8 HW_AHBPROT_ENABLED, struct __argv *args )
{
	if(dolstart == NULL)
		return -1;

	void	(*entrypoint)();
	
	Elf32_Ehdr ElfHdr;

	memcpy(&ElfHdr,dolstart,sizeof(Elf32_Ehdr));
	
	if( ElfHdr.e_ident[EI_MAG0] == 0x7F &&
		ElfHdr.e_ident[EI_MAG1] == 'E' &&
		ElfHdr.e_ident[EI_MAG2] == 'L' &&
		ElfHdr.e_ident[EI_MAG3] == 'F' )
	{

		gdprintf("BootDolFromMem : ELF Found\n");
		gdprintf("Type:      \t%04X\n", ElfHdr.e_type );
		gdprintf("Machine:   \t%04X\n", ElfHdr.e_machine );
		gdprintf("Version:  %08X\n", ElfHdr.e_version );
		gdprintf("Entry:    %08X\n", ElfHdr.e_entry );
		gdprintf("Flags:    %08X\n", ElfHdr.e_flags );
		gdprintf("EHsize:    \t%04X\n\n", ElfHdr.e_ehsize );

		gdprintf("PHoff:    %08X\n",	ElfHdr.e_phoff );
		gdprintf("PHentsize: \t%04X\n",	ElfHdr.e_phentsize );
		gdprintf("PHnum:     \t%04X\n\n",ElfHdr.e_phnum );

		gdprintf("SHoff:    %08X\n",	ElfHdr.e_shoff );
		gdprintf("SHentsize: \t%04X\n",	ElfHdr.e_shentsize );
		gdprintf("SHnum:     \t%04X\n",	ElfHdr.e_shnum );
		gdprintf("SHstrndx:  \t%04X\n\n",ElfHdr.e_shstrndx );
		if( ( (ElfHdr.e_entry | 0x80000000) >= 0x81000000 ) && ( (ElfHdr.e_entry | 0x80000000) <= 0x81330000) )
		{
			gprintf("BootDolFromMem : ELF entrypoint error: abort!\n");
			return -1;
		}
		//its an elf; lets start killing DVD :')
		if(DVDCheckCover())
		{
			gprintf("BootDolFromMem : excecuting StopDisc Async...\n");
			DVDStopDisc(true);
		}
		else
		{
			gprintf("BootDolFromMem : Skipping StopDisc -> no drive or disc in drive\n");
		}
		if( ElfHdr.e_phnum == 0 )
		{
			gdprintf("BootDolFromMem : Warning program header entries are zero!\n");
		} 
		else 
		{
			for( s32 i=0; i < ElfHdr.e_phnum; ++i )
			{
				Elf32_Phdr phdr;
				ICInvalidateRange (&phdr ,sizeof( phdr ) );
				memmove(&phdr,dolstart + (ElfHdr.e_phoff + sizeof( Elf32_Phdr ) * i) ,sizeof( phdr ) );
				gdprintf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz );
				ICInvalidateRange ((void*)(phdr.p_vaddr | 0x80000000),phdr.p_filesz);
				if(phdr.p_type == PT_LOAD )
					memmove((void*)(phdr.p_vaddr | 0x80000000), dolstart + phdr.p_offset , phdr.p_filesz);
			}
		}

		//according to dhewg the section headers are totally un-needed (infact, they break a few elf loading)
		//however, checking for the type does the trick to make them work :)
		if( ElfHdr.e_shnum == 0 )
		{
			gdprintf("BootDolFromMem : Warning section header entries are zero!\n");
		} 
		else 
		{

			for( s32 i=0; i < ElfHdr.e_shnum; ++i )
			{

				Elf32_Shdr shdr;
				memmove(&shdr, dolstart + (ElfHdr.e_shoff + sizeof( Elf32_Shdr ) * i) ,sizeof( shdr ) );
				DCFlushRangeNoSync(&shdr ,sizeof( shdr ) );

				if( shdr.sh_type == SHT_NULL )
					continue;

				if( shdr.sh_type > SHT_GROUP )
					gdprintf("Warning the type: %08X could be invalid!\n", shdr.sh_type );

				if( shdr.sh_flags & ~0xF0000007 )
					gdprintf("Warning the flag: %08X is invalid!\n", shdr.sh_flags );

				gdprintf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr.sh_type, shdr.sh_offset, shdr.sh_name, shdr.sh_addr, shdr.sh_size );
				if (shdr.sh_type == SHT_NOBITS)
				{
					memmove((void*)(shdr.sh_addr | 0x80000000), dolstart + shdr.sh_offset,shdr.sh_size);
					DCFlushRangeNoSync((void*)(shdr.sh_addr | 0x80000000),shdr.sh_size);
				}
			}
		}
		entrypoint = (void (*)())(ElfHdr.e_entry | 0x80000000);
	}
	else
	{
		gprintf("BootDolFromMem : DOL detected\n");

		dolhdr *dolfile;
		dolfile = (dolhdr *) dolstart;

		//entrypoint & BSS checking
		if( dolfile->entrypoint >= 0x80F00000 && dolfile->entrypoint <= 0x81330000 )
		{
			gprintf("BootDolFromMem : entrypoint/BSS error: abort!\n");
			return -1;
		}
		else if ( dolfile->entrypoint < 0x80000000 )
		{
			gprintf("BootDolFromMem : bogus entrypoint detected\n");
			return -2;
		}
		if( dolfile->addressBSS >= 0x90000000 )
		{
			//BSS is in mem2 which means its better to reload ios & then load app. i dont really get it but thats what tantric said
			//currently unused cause this is done for wiimc. however reloading ios also looses ahbprot/dvd access...
			
			//place IOS reload here
		}
		//start killing DVD :')
		if(DVDCheckCover())
		{
			gprintf("BootDolFromMem : excecuting StopDisc Async...\n");
			DVDStopDisc(true);
		}
		else
		{
			gprintf("BootDolFromMem : Skipping StopDisc -> no drive or disc in drive\n");
		}

		for (s8 i = 0; i < 7; i++) {
			if ((!dolfile->sizeText[i]) || (dolfile->addressText[i] < 0x100)) continue;
			memmove ((void *) dolfile->addressText[i],dolstart+dolfile->offsetText[i],dolfile->sizeText[i]);
			DCFlushRange ((void *) dolfile->addressText[i], dolfile->sizeText[i]);
			DCInvalidateRange((void *) dolfile->addressText[i],dolfile->sizeText[i]);
		}
		gdprintf("Data Sections :\n");
		for (s8 i = 0; i < 11; i++) {
			if ((!dolfile->sizeData[i]) || (dolfile->offsetData[i] < 0x100)) continue;
			memmove ((void *) dolfile->addressData[i],dolstart+dolfile->offsetData[i],dolfile->sizeData[i]);
			DCFlushRange((void *) dolfile->offsetData[i],dolfile->sizeData[i]);
			gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (dolfile->offsetData[i]), dolfile->addressData[i], dolfile->sizeData[i]);
		}
		if( 
			( dolfile->addressBSS + dolfile->sizeBSS < 0x80F00000 ||(dolfile->addressBSS > 0x81500000 && dolfile->addressBSS + dolfile->sizeBSS < 0x817FFFFF) ) &&
			dolfile->addressBSS > 0x80003400 )
		{
			memset ((void *) dolfile->addressBSS, 0, dolfile->sizeBSS);
			DCFlushRange((void *) dolfile->addressBSS, dolfile->sizeBSS);
		}
		if (args != NULL && args->argvMagic == ARGV_MAGIC)
        {
			void* new_argv = (void*)(dolfile->entrypoint + 8);
			memmove(new_argv, args, sizeof(__argv));
			DCFlushRange(new_argv, sizeof(__argv));
        }
		entrypoint = (void (*)())(dolfile->entrypoint);
	}
	gprintf("BootDolFromMem : starting binary...\n");
	for (int i = 0;i < WPAD_MAX_WIIMOTES ;i++)
	{
		if(WPAD_Probe(i,0) > 0)
		{
			if(WPAD_Probe(i,0) < 0)
				continue;
			WPAD_Flush(i);
			WPAD_Disconnect(i);
		}
	}
	ClearState();
	WPAD_Shutdown();
	ShutdownDevices();

	if(DvdKilled() < 1)
	{
		gprintf("checking DVD drive...\n");
		while(DvdKilled() < 1);
	}

	s8 bAHBPROT = 0;
	s32 Ios_to_load = 0;
	if(read32(0x0d800064) == 0xFFFFFFFF )
		bAHBPROT = HW_AHBPROT_ENABLED;
	if( !isIOSstub(58) )
	{
		Ios_to_load = 58;
	}
	else if( !isIOSstub(61) )
	{
		Ios_to_load = 61;
	}
	else if( !isIOSstub(IOS_GetPreferredVersion()) )
	{
		Ios_to_load = IOS_GetPreferredVersion();
	}
	else
	{
		PrintFormat( 1, TEXT_OFFSET("failed to reload ios for homebrew! ios is a stub!"), 208, "failed to reload ios for homebrew! ios is a stub!");
		sleep(2);	
	}

	if(Ios_to_load > 2 && Ios_to_load < 255)
	{
		ReloadIos(Ios_to_load,&bAHBPROT);
		system_state.ReloadedIOS = 1;
	}
	
	gprintf("IOS state : ios %d - ahbprot : %d \n",IOS_GetVersion(),(read32(0x0d800064) == 0xFFFFFFFF ));
	gprintf("BootDolFromMem : Entrypoint: 0x%08X\n", (u32)(entrypoint) );

	ISFS_Deinitialize();
	ShutdownDevices();
	USB_Deinitialize();
	__IOS_ShutdownSubsystems();
	VIDEO_Flush();
    VIDEO_WaitVSync();
    __exception_closeall();
	ICSync();
    entrypoint();



	
	//alternate booting code. seems to be as good(or bad) as the above code
	/*u32 level;
	__STM_Close();
	ISFS_Deinitialize();
	ShutdownDevices();
	USB_Deinitialize();
	__IOS_ShutdownSubsystems();
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    _CPU_ISR_Disable (level);
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	ICSync();
    entrypoint();
    _CPU_ISR_Restore (level);*/

	//it failed. FAIL!
	__IOS_InitializeSubsystems();
	PollDevices();
	ISFS_Initialize();
	WPAD_Init();
	PAD_Init();
	return -1;
}

s8 BootDolFromDir( const char* Dir , u8 HW_AHBPROT_ENABLED,const std::vector<std::string> &args_list)
{
	if (GetMountedValue() == 0)
	{
		return -5;
	}
	struct __argv args;
	bzero(&args, sizeof(args));
	args.argvMagic = 0;
	args.length = 0;
	args.commandLine = NULL;
	args.argc = 0;
	args.argv = &args.commandLine;
	args.endARGV = args.argv + 1;
	
	if(args_list.size() > 0)
	{
		//loading args
		for(u32 i = 0; i < args_list.size(); i++)
		{
			if(args_list[i].c_str())
				args.length += strlen(args_list[i].c_str())+1;
		}
		args.length += 1;
		args.commandLine = (char*) mem_malloc(args.length);
		if (args.commandLine != NULL)
		{
			u32 pos = 0;
			for(u32 i = 0; i < args_list.size(); i++)
			{
				if(args_list[i].c_str())
				{
					strcpy(&args.commandLine[pos], args_list[i].c_str());
					pos += strlen(args_list[i].c_str())+1;
				}
			}
			args.commandLine[args.length - 1] = '\0';
			args.argc = args_list.size();
			args.argv = &args.commandLine;
			args.endARGV = args.argv + 1;
			args.argvMagic = ARGV_MAGIC; //everything is set so the magic is set so it becomes valid
		}
		else
		{
			args.commandLine = 0;
			args.length = 0;
		}
	}
	FILE* dol;
	dol = fopen(Dir,"rb");
	s8 ret = 0;
	if (dol)
	{
		gdprintf("booting from fat...\n");
		ret = BootDolFromFat(dol,HW_AHBPROT_ENABLED,&args);
		fclose(dol);
	}
	else
	{
		fclose(dol);
		ret = -6;
	}
	if(args.commandLine != NULL)
		mem_free(args.commandLine);
	return ret;
}
void BootMainSysMenu( u8 init )
{
	//memory block variables used within the function:
	//ticket stuff:
	char * buf = NULL;
	fstats * tstatus = NULL;

	//TMDview stuff:
	u64 TitleID=0x0000000100000002LL;
	u32 tmd_size;
	static u8 tmd_buf[(sizeof(tmd_view) + MAX_NUM_TMD_CONTENTS*sizeof(tmd_view_content))] ATTRIBUTE_ALIGN(32);
	tmd_view *rTMD = NULL;

	//TMD:
	signed_blob *TMD = NULL;

	//boot file:
	u32 fileID = 0;
	char file[265] ATTRIBUTE_ALIGN(32);
	fstats *status = NULL;
	dolhdr *boot_hdr = NULL;

	//hacks : hashes
	u8* mem_block = 0;
	u32 max_address = 0;


	//general:
	s32 r = 0;
	s32 fd = 0;
	void	(*entrypoint)();


	if(init == 0)
	{
		//PollDevices();
	}
	//booting sys menu
	
	//expermintal code for getting the needed tmd info. no boot index is in the views but lunatik and i think last file = boot file
	r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("GetTMDViewSize error %d\n",r);
		error = ERROR_SYSMENU_GETTMDSIZEFAILED;
		goto free_and_return;
	}
	r = ES_GetTMDView(TitleID, tmd_buf, tmd_size);
	if(r<0)
	{
		gprintf("GetTMDView error %d\n",r);
		error = ERROR_SYSMENU_GETTMDFAILED;
		goto free_and_return;
	}
	rTMD = (tmd_view *)tmd_buf;
	gdprintf("ios version: %u\n",(u8)rTMD->sys_version);

	//get main.dol filename
	/*for(u32 z=0; z < rTMD->num_contents; ++z)
	{
		if( rTMD->contents[z].index == rTMD->num_contents )//rTMD->boot_index )
		{
			gdprintf("content[%i] id=%08X type=%u\n", z, content->cid, content->type | 0x8001 );
			fileID = rTMD->contents[z].cid;
			break;
		}
	}*/
	fileID = rTMD->contents[rTMD->num_contents-1].cid;
	gprintf("using %08X for booting\n",rTMD->contents[rTMD->num_contents-1].cid);

	if( fileID == 0 )
	{
		error = ERROR_SYSMENU_BOOTNOTFOUND;
		goto free_and_return;
	}

	sprintf( file, "/title/00000001/00000002/content/%08x.app", fileID );
	file[33] = '1'; // installing preloader renamed system menu so we change the app file to have the right name

	fd = ISFS_Open( file, 1 );
	if( fd < 0 )
	{
		gprintf("error opening %08x.app! error %d\n",fileID,fd);
		ISFS_Close( fd );
		error = ERROR_SYSMENU_BOOTOPENFAILED;
		goto free_and_return;
	}

	status = (fstats *)mem_align(32, ALIGN32(sizeof( fstats )) );
	if( status == NULL )
	{
		ISFS_Close( fd );
		error = ERROR_MALLOC;
		goto free_and_return;
	}
	memset(status,0, sizeof( fstats ) );
	r = ISFS_GetFileStats( fd, status);
	if( r < 0 || status->file_length == 0)
	{
		ISFS_Close( fd );
		error = ERROR_SYSMENU_BOOTGETSTATS;
		mem_free(status);
		goto free_and_return;
	}

	mem_free(status);
	boot_hdr = (dolhdr *)mem_align(32, ALIGN32(sizeof( dolhdr )) );
	if(boot_hdr == NULL)
	{
		error = ERROR_MALLOC;
		ISFS_Close(fd);
		goto free_and_return;
	}
	memset( boot_hdr, 0, ALIGN32(sizeof( dolhdr )) );
	
	r = ISFS_Seek( fd, 0, SEEK_SET );
	if ( r < 0)
	{
		gprintf("ISFS_Seek error %d(dolhdr)\n",r);
		ISFS_Close(fd);
		goto free_and_return;
	}
	r = ISFS_Read( fd, boot_hdr, sizeof(dolhdr) );

	if( r < 0 || r != sizeof(dolhdr) )
	{
		gprintf("ISFS_Read error %d of dolhdr\n",r);
		ISFS_Close( fd );
		goto free_and_return;
	}
	if( boot_hdr->entrypoint != 0x3400 )
	{
		gprintf("Bogus Entrypoint detected!\n");
		ISFS_Close( fd );
		goto free_and_return;
	}

	for (u8 i = 0; i < 6; i++)
	{
		if( boot_hdr->sizeText[i] && boot_hdr->addressText[i] && boot_hdr->offsetText[i] )
		{
			ICInvalidateRange((void*)(boot_hdr->addressText[i]), boot_hdr->sizeText[i]);
			gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (boot_hdr->offsetText[i]), boot_hdr->addressText[i], boot_hdr->sizeText[i]);
			if( (((boot_hdr->addressText[i])&0xF0000000) != 0x80000000) || (boot_hdr->sizeText[i]>(10*1024*1024)) )
			{
				gprintf("bogus Text offset\n");
				ISFS_Close( fd );
				goto free_and_return;
			}

			r = ISFS_Seek( fd, boot_hdr->offsetText[i], SEEK_SET );
			if ( r < 0)
			{
				gprintf("ISFS_Seek error %d(offsetText)\n");
				ISFS_Close(fd);
				goto free_and_return;
			}
			ISFS_Read( fd, (void*)(boot_hdr->addressText[i]), boot_hdr->sizeText[i] );

			DCFlushRange((void*)(boot_hdr->addressText[i]), boot_hdr->sizeText[i]);
		}
	}
	// data sections
	for (u8 i = 0; i <= 10; i++)
	{
		if( boot_hdr->sizeData[i] && boot_hdr->addressData[i] && boot_hdr->offsetData[i] )
		{
			ICInvalidateRange((void*)(boot_hdr->addressData[i]), boot_hdr->sizeData[i]);
			gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (boot_hdr->offsetData[i]), boot_hdr->addressData[i], boot_hdr->sizeData[i]);
			if( (((boot_hdr->addressData[i])&0xF0000000) != 0x80000000) || (boot_hdr->sizeData[i]>(10*1024*1024)) )
			{
				gprintf("bogus Data offsets\n");
				ISFS_Close(fd);
				goto free_and_return;
			}

			r = ISFS_Seek( fd, boot_hdr->offsetData[i], SEEK_SET );
			if ( r < 0)
			{
				gdprintf("ISFS_Seek error %d(offsetData)\n");
				ISFS_Close(fd);
				goto free_and_return;
			}
			r = ISFS_Read( fd, (void*)boot_hdr->addressData[i], boot_hdr->sizeData[i] );
			if (r < 0)
			{
				gdprintf("ISFS_Read error %d(addressdata)\n",r);
				ISFS_Close(fd);
				goto free_and_return;
			}
			DCFlushRange((void*)boot_hdr->addressData[i], boot_hdr->sizeData[i]);
		}

	}
	ISFS_Close(fd);
	entrypoint = (void (*)())(boot_hdr->entrypoint);
	gdprintf("entrypoint %08X\n", entrypoint );

	LoadHacks_Hash(true);

	for(u8 i=0;i<WPAD_MAX_WIIMOTES;i++) {
		if(WPAD_Probe(i,0) < 0)
			continue;
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}
	WPAD_Shutdown();

	//Step 1 of IOS handling : Reloading IOS if needed;
	if( !SGetSetting( SETTING_USESYSTEMMENUIOS ) )
	{
		s32 ToLoadIOS = SGetSetting(SETTING_SYSTEMMENUIOS);
		if ( ToLoadIOS != (u8)IOS_GetVersion() )
		{
			if ( !isIOSstub(ToLoadIOS) )
			{
				__ES_Close();
				__IOS_ShutdownSubsystems();
				__ES_Init();
				__IOS_LaunchNewIOS ( ToLoadIOS );
				//why the hell the es needs 2 init's is beyond me... it just happens (see IOS_ReloadIOS in libogc's ios.c)
				__ES_Init();
				//gprintf("BootMainSysMenu : ios %d launched\n",IOS_GetVersion());
				//__IOS_LaunchNewIOS ( (u8)rTMD->sys_version );
				//__IOS_LaunchNewIOS ( 249 );
				system_state.ReloadedIOS = 1;
			}
			else
			{
				WPAD_Init();
				error=ERROR_SYSMENU_IOSSTUB;
				goto free_and_return;
			}
		}
	}
	/*
	//technically its needed... but i fail to see why...
	else if ((u8)IOS_GetVersion() != (u8)rTMD->sys_version)
	{
		gprintf("Use system menu is ON, but IOS %d isn't loaded. reloading IOS...\n",(u8)rTMD->sys_version);
		__ES_Close();
		__IOS_ShutdownSubsystems();
		__ES_Init();
		__IOS_LaunchNewIOS ( (u8)rTMD->sys_version );
		__IOS_InitializeSubsystems();

		gprintf("launched ios %d for system menu\n",IOS_GetVersion());
		system_state.ReloadedIOS = 1;
	}*/
	//Step 2 of IOS handling : ES_Identify if we are on a different ios or if we reloaded ios once already. note that the ES_Identify is only supported by ios > 20
	if (((u8)IOS_GetVersion() != (u8)rTMD->sys_version) || (system_state.ReloadedIOS) )
	{
		if (system_state.ReloadedIOS)
			gprintf("Forced into ES_Identify\n");
		else
			gprintf("Forcing ES_Identify\n",IOS_GetVersion(),(u8)rTMD->sys_version);
		//read ticket from FS
		fd = ISFS_Open("/title/00000001/00000002/content/ticket", 1 );
		if( fd < 0 )
		{
			error = ERROR_SYSMENU_TIKNOTFOUND;
			goto free_and_return;
		}

		//get size
		tstatus = (fstats*)mem_align( 32, sizeof( fstats ) );
		if(tstatus == NULL)
		{
			ISFS_Close( fd );
			error = ERROR_MALLOC;
			goto free_and_return;
		}
		r = ISFS_GetFileStats( fd, tstatus );
		if( r < 0 )
		{
			ISFS_Close( fd );
			error = ERROR_SYSMENU_TIKSIZEGETFAILED;
			goto free_and_return;
		}

		//create buffer
		buf = (char*)mem_align( 32, ALIGN32(tstatus->file_length) );
		if( buf == NULL )
		{
			ISFS_Close( fd );
			error = ERROR_MALLOC;
			goto free_and_return;
		}
		memset(buf, 0, tstatus->file_length );

		//read file
		r = ISFS_Read( fd, buf, tstatus->file_length );
		if( r < 0 )
		{
			gprintf("ES_Identify: R-err %d\n",r);
			ISFS_Close( fd );
			error = ERROR_SYSMENU_TIKREADFAILED;
			goto free_and_return;
		}

		ISFS_Close( fd );
		//get the real TMD. we didn't get the real TMD before. the views will fail to be used in identification
		u32 tmd_size_temp;
		r=ES_GetStoredTMDSize(TitleID, &tmd_size_temp);
		if(r < 0)
		{
			error=ERROR_SYSMENU_ESDIVERFIY_FAILED;
			gprintf("ES_Identify: GetStoredTMDSize error %d\n",r);
			__IOS_InitializeSubsystems();
			goto free_and_return;
		}
		TMD = (signed_blob *)mem_align( 32, ALIGN32(tmd_size_temp) );
		if(TMD == NULL)
		{
			gprintf("ES_Identify: memalign TMD failure\n");
			error = ERROR_MALLOC;
			__IOS_InitializeSubsystems();
			goto free_and_return;
		}
		memset(TMD, 0, tmd_size_temp);

		r=ES_GetStoredTMD(TitleID, TMD, tmd_size_temp);
		if(r < 0)
		{
			error=ERROR_SYSMENU_ESDIVERFIY_FAILED;
			gprintf("ES_Identify: GetStoredTMD error %d\n",r);
			__IOS_InitializeSubsystems();
			goto free_and_return;
		}
		r = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)TMD, tmd_size_temp, (signed_blob *)buf, tstatus->file_length, 0);
		if( r < 0 )
		{	
			error=ERROR_SYSMENU_ESDIVERFIY_FAILED;
			gprintf("ES_Identify error %d\n",r);
			__IOS_InitializeSubsystems();
			goto free_and_return;
		}
		if(TMD)
			mem_free(TMD);
	}

	//ES_SetUID(TitleID);
	if(tstatus)
		mem_free( tstatus );
	if(buf)
		mem_free( buf );

	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed
	*(vu32*)0x8000315C = 0x80800113;				// DI Legacy mode ?
	DCFlushRange((void*)0x80000000,0x3400);

	gprintf("Hacks:%d\n",hacks_hash.size());
	if(hacks_hash.size() != 0)
	{
		mem_block = (u8*)(*boot_hdr->addressData - *boot_hdr->offsetData);
		max_address = (u32)(*boot_hdr->sizeData + *boot_hdr->addressData);
		for(u32 i = 0;i < hacks_hash.size();i++)
		{
			if(states_hash[i] == 1)
			{
				u32 add = 0;
				for(u32 y = 0; y < hacks_hash[i].amount;y++)
				{
					while( add + (u32)mem_block < max_address)
					{
						u8 temp_hash[hacks_hash[i].patches[y].hash.size()];
						u8 temp_patch[hacks_hash[i].patches[y].patch.size()];
						for(u32 z = 0;z < hacks_hash[i].patches[y].hash.size(); z++)
						{
							temp_hash[z] = hacks_hash[i].patches[y].hash[z];
						}
						if ( !memcmp(mem_block+add, temp_hash ,sizeof(temp_hash)) )
						{
							gprintf("Found %s @ 0x%X, patching hash # %d...\n",hacks_hash[i].desc.c_str(), add+(u32)mem_block, y+1);
							for(u32 z = 0;z < hacks_hash[i].patches[y].patch.size(); z++)
							{
								temp_patch[z] = hacks_hash[i].patches[y].patch[z];
							}
							memcpy(mem_block+add,temp_patch,sizeof(temp_patch) );
							DCFlushRange((u8 *)((add+(u32)mem_block) >> 5 << 5), (sizeof(temp_patch) >> 5 << 5) + 64);
							break;
						}
						add++;
					}//end while loop
				} //end for loop of all hashes of hack[i]
			} //end if state[i] = 1
		} // end general hacks loop
	} //end if hacks > 0
	if(TMD)
		mem_free(TMD);
	if(tstatus)
		mem_free( tstatus );
	if(buf)
		mem_free( buf );
	if(boot_hdr)
		mem_free(boot_hdr);

	ShutdownDevices();
	USB_Deinitialize();
	if(init == 1 || SGetSetting(SETTING_DUMPGECKOTEXT) != 0 )
		Control_VI_Regs(2);
	ISFS_Deinitialize();
	__STM_Close();
	__IPC_Reinitialize();
	__IOS_ShutdownSubsystems();
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	ICSync();
	_unstub_start();
free_and_return:
	if(TMD)
		mem_free(TMD);
	if(tstatus)
		mem_free( tstatus );
	if(buf)
		mem_free( buf );
	if(boot_hdr)
		mem_free(boot_hdr);

	WPAD_Init();

	return;
}
void InstallLoadDOL( void )
{
	char filename[MAXPATHLEN],filepath[MAXPATHLEN];
	std::vector<Binary_struct> app_list;
	DIR* dir;
	s8 reload = 1;
	s8 redraw = 1;
	s8 DevStat = GetMountedValue();
	s16 cur_off = 0;
	s16 max_pos = 0;
	s16 min_pos = 0;
	u32 ret = 0;
	while(1)
	{
		PollDevices();
		if (DevStat != GetMountedValue())
		{
			ClearScreen();
			PrintFormat( 1, TEXT_OFFSET("Reloading Binaries..."), 208, "Reloading Binaries...");
			sleep(1);
			app_list.clear();
			reload = 1;
			min_pos = 0;
			max_pos = 0;
			cur_off = 0;
			redraw=1;
		}
		if(GetMountedValue() == 0)
		{
			ClearScreen();
			PrintFormat( 1, TEXT_OFFSET("NO fat device found!"), 208, "NO fat device found!");
			sleep(5);
			return;
		}
		if(reload)
		{
			gprintf("loading shit...\n");
			DevStat = GetMountedValue();
			reload = 0;
			dir = opendir ("fat:/apps/");
			if( dir != NULL )
			{
				//get all files names
				while( readdir(dir) != NULL )
				{
					strncpy(filename,dir->fileData.d_name,NAME_MAX+1);
					if(strncmp(filename,".",1) == 0 || strncmp(filename,"..",2) == 0 )
					{
						//we dont want the root or the dirup stuff. so lets filter them
						continue;
					}
					sprintf(filepath,"fat:/apps/%s/boot.dol",filename);
					FILE* app_bin;
					app_bin = fopen(filepath,"rb");
					if(!app_bin)
					{
						sprintf(filepath,"fat:/apps/%s/boot.elf",filename);
						app_bin = fopen(filepath,"rb");
						if(!app_bin)
							continue;
					}
					fclose(app_bin);
					Binary_struct temp;
					temp.HW_AHBPROT_ENABLED = 0;
					temp.app_path = filepath;
					temp.args.clear();
					sprintf(filepath,"fat:/apps/%s/meta.xml",filename);
					app_bin = fopen(filepath,"rb");
					if(!app_bin)
					{
						gdprintf("failed to open meta.xml of %s\n",filename);
						temp.app_name = filename;
						app_list.push_back(temp);
						continue;
					}
					long size;
					char* buf;

					fseek (app_bin , 0 , SEEK_END);
					size = ftell(app_bin);
					rewind (app_bin);
					buf = (char*)mem_malloc(size+1);
					if(!buf)
					{
						gdprintf("buf == NULL\n");
						fclose(app_bin);
						temp.app_name = filename;
						app_list.push_back(temp);
						continue;
					}
					memset(buf,0,size+1);
					ret = fread(buf,1,size,app_bin) ;
					if(ret != (u32)size)
					{
						mem_free(buf);
						fclose(app_bin);
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("failed to read data error %d\n",ret);
					}
					else
					{
						fclose(app_bin);
						char* tag_start = 0;
						char* tag_end = 0;
						//note to self. find safer fucking to replace strstr
						tag_start = strstr(buf,"<no_ios_reload/>");
						if(tag_start == NULL)
							tag_start = strstr(buf,"<ahb_access/>");
						if(tag_start != NULL)
							temp.HW_AHBPROT_ENABLED = 1;
						tag_start = 0;
						tag_start = strstr(buf,"<name>");
						if(tag_start != NULL)
						{
							tag_start += 6;
							tag_end = strstr(tag_start,"</name>");
							if(tag_start != NULL && tag_end != NULL)
							{
								char _temp[tag_end - tag_start+1];
								memset(_temp,0,sizeof(_temp));
								strncpy(_temp,tag_start,tag_end - tag_start);
								temp.app_name = _temp;
							}
						}
						tag_end = 0;
						tag_start = 0;
						tag_start = strstr(buf,"<arguments>");
						if(tag_start != NULL)
						{
							tag_end = strstr(tag_start+5,"</arguments>");
							if(tag_start != NULL && tag_end != NULL)
							{
								char* arg_start = strstr(buf,"<arg>");
								char* arg_end = 0;
								while(arg_start != NULL)
								{
									//arguments!
									arg_start+= 5;
									arg_end = strstr(arg_start,"</arg>");
									if(arg_end == NULL)
									{
										//oh-ow. no ending. lets bail out
										break;
									}
									char _temp[arg_end - arg_start+1];
									memset(_temp,0,sizeof(_temp));
									strncpy(_temp,arg_start,arg_end - arg_start);
									temp.args.push_back(_temp);
									arg_end = 0;
									arg_start = strstr(arg_start,"<arg>");
								}
							}
						}
						mem_free(buf);
					}
					if(temp.app_name.size())
					{
						gdprintf("added %s to list\n",temp.app_name.c_str());
						app_list.push_back(temp);
						continue;
					}
					else
					{
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("no name found in xml D:<\n");
						continue;
					}
				}
				closedir( dir );
			}
			else
			{
				gprintf("WARNING: could not open fat:/apps/ for binaries\n");
			}
			dir = opendir ("fat:/");
			if(dir == NULL)
			{
				gprintf("WARNING: could not open fat:/ for binaries\n");
			}
			else
			{
				while( readdir(dir) != NULL )
				{
					strncpy(filename,dir->fileData.d_name,NAME_MAX+1);
					if( (strstr( filename, ".dol") != NULL) ||
						(strstr( filename, ".DOL") != NULL) ||
						(strstr( filename, ".elf") != NULL) ||
						(strstr( filename, ".ELF") != NULL) )
					{
						Binary_struct temp;
						temp.HW_AHBPROT_ENABLED = 0;
						temp.app_name = filename;
						sprintf(filepath,"fat:/%s",filename);
						temp.app_path.assign(filepath);
						app_list.push_back(temp);
					}
				}
				closedir( dir );
			}
			
			if( app_list.size() == 0 )
			{
				if((GetMountedValue() & 2) && ToggleUSBOnlyMode() == 1)
				{
					gprintf("switching to USB only...also mounted == %d\n",GetMountedValue());
					reload = 1;
					continue;
				}
				if(GetUsbOnlyMode() == 1)
				{
					gprintf("fixing usbonly mode...\n");
					ToggleUSBOnlyMode();
				}
				PrintFormat( 1, TEXT_OFFSET("Couldn't find any executable files"), 208, "Couldn't find any executable files");
				PrintFormat( 1, TEXT_OFFSET("in the fat:/apps/ on the device!"), 228, "in the fat:/apps/ on the device!");
				sleep(5);
				return;
			}
			if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
			{
				//ye, those tv's want a special treatment again >_>
				max_pos = 14;
			}
			else
			{
				max_pos = 19;
			}
			if ((s32)app_list.size() < max_pos)
				max_pos = app_list.size() -1;
			//sort app lists
			s8 swap = 0;
			for(s32 max = 0;max < (s32)app_list.size() * (s32)app_list.size();max++)
			{
				swap = 0;
				for (int count = 0; count < (s32)app_list.size(); count++)
				{
					if(strncmp(app_list[count].app_path.c_str(),"fat:/apps/",strlen("fat:/apps/")) == 0)
					{
						for(int swap_index = 0;swap_index < (s32)app_list.size();swap_index++)
						{
							if ( strcasecmp(app_list[count].app_name.c_str(), app_list[swap_index].app_name.c_str()) < 0 )
							{
								swap = 1;
								std::swap(app_list[count],app_list[swap_index]);
							}
						}
					}
				}
				if(swap != 0)
					break;
			}
			ClearScreen();
			redraw=true;
		}
		if( redraw )
		{
			s16 i= min_pos;
			if((s32)app_list.size() -1 > max_pos && (min_pos != (s32)app_list.size() - max_pos - 1) )
			{
				PrintFormat( 0,TEXT_OFFSET("-----More-----"),64+(max_pos+2)*16,"-----More-----");
			}
			if(min_pos > 0 )
			{
				PrintFormat( 0,TEXT_OFFSET("-----Less-----"),64,"-----Less-----");
			}
			for(; i<=(min_pos + max_pos); i++ )
			{
				PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "%s%s", app_list[i].app_name.c_str(),(read32(0x0d800064) == 0xFFFFFFFF && app_list[i].HW_AHBPROT_ENABLED != 0)?"(AHBPROT Available)":" ");
			}
			PrintFormat( 0, TEXT_OFFSET("A(A) Install File"), rmode->viHeight-64, "A(A) Install FIle");
			PrintFormat( 0, TEXT_OFFSET("1(Z) Load File   "), rmode->viHeight-48, "1(Y) Load File");
			PrintFormat( 0, TEXT_OFFSET("2(X) Delete installed File"), rmode->viHeight-32, "2(X) Delete installed File");

			redraw = false;
		}
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
 
		if ( WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B )
		{
			break;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			ClearScreen();
			FILE *dol = fopen(app_list[cur_off].app_path.c_str(),"rb");
			if( dol == NULL )
			{
				PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Could not open:\"%s\" for reading")+strlen(app_list[cur_off].app_name.c_str()))*13/2))>>1, 208, "Could not open:\"%s\" for reading", app_list[cur_off].app_name.c_str());
				sleep(5);
				break;
			}
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Installing \"%s\"...")+strlen(app_list[cur_off].app_name.c_str()))*13/2))>>1, 208, "Installing \"%s\"...", app_list[cur_off].app_name.c_str());

			//get size
			fseek( dol, 0, SEEK_END );
			unsigned int size = ftell( dol );
			fseek( dol, 0, 0 );

			char *buf = (char*)mem_align( 32, ALIGN32(sizeof( char ) * size) );
			if(buf != NULL)
			{
				memset( buf, 0, sizeof( char ) * size );

				fread( buf, sizeof( char ), size, dol );
				fclose( dol );

				//Check if there is already a main.dol installed
				s32 fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1|2 );

				if( fd >= 0 )	//delete old file
				{
					ISFS_Close( fd );
					ISFS_Delete("/title/00000001/00000002/data/main.bin");
				}

				//file not found create a new one
				ISFS_CreateFile("/title/00000001/00000002/data/main.bin", 0, 3, 3, 3);
				fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1|2 );

				if( ISFS_Write( fd, buf, sizeof( char ) * size ) != (signed)(sizeof( char ) * size) )
				{
					PrintFormat( 1, TEXT_OFFSET("Writing dol failed!"), 240, "Writing dol failed!");
				}
				else
				{
					ISFS_Close( fd );
					mem_free( buf );
					//dol is saved. lets save the extra info now
					STACK_ALIGN(_dol_settings,dol_settings,sizeof(_dol_settings),32);
					memset(dol_settings,0,sizeof(_dol_settings));
					dol_settings->HW_AHBPROT_bit = app_list[cur_off].HW_AHBPROT_ENABLED;
					dol_settings->argument_count = app_list[cur_off].args.size();
					for(u32 i = 0;i < app_list[cur_off].args.size();i++)
					{
						if(app_list[cur_off].args[i].c_str())
							dol_settings->arg_cli_lenght += strlen(app_list[cur_off].args[i].c_str())+1;
					}
					dol_settings->arg_cli_lenght += 1;

					dol_settings->arg_command_line = (char*)mem_align(32,dol_settings->arg_cli_lenght+1);
					if(dol_settings->arg_command_line != NULL)
					{
						u32 pos = 0;
						for(u32 i = 0; i < app_list[cur_off].args.size(); i++)
						{
							if(app_list[cur_off].args[i].c_str())
							{
								strcpy(&dol_settings->arg_command_line[pos], app_list[cur_off].args[i].c_str());
								pos += strlen(app_list[cur_off].args[i].c_str())+1;
							}
						}
						dol_settings->arg_command_line[dol_settings->arg_cli_lenght -1] = 0x00;
						fd = ISFS_Open("/title/00000001/00000002/data/main.nfo", 1|2 );
						if( fd >= 0 )	//delete old file
						{
							ISFS_Close( fd );
							ISFS_Delete("/title/00000001/00000002/data/main.nfo");
						}
						ISFS_CreateFile("/title/00000001/00000002/data/main.nfo", 0, 3, 3, 3);
						fd = ISFS_Open("/title/00000001/00000002/data/main.nfo", 1|2 );
					}
					else
					{
						fd = -1; //to trigger the error
					}
					if( fd < 0 )
					{
						PrintFormat( 1, TEXT_OFFSET("Writing nfo failed!"), 272, "Writing nfo failed!");
					}
					else
					{
						STACK_ALIGN(s8,null,1,32);
						memset(null,0,1);
						//write the ahbprot byte and arg count by all at once since ISFS_Write fails like that
						ISFS_Write(fd,&dol_settings->HW_AHBPROT_bit,sizeof(s16));//s8(AHBPROT)+s8(argument count)+u32(arg lenght) = 6 bytes
						if(&dol_settings->arg_cli_lenght != NULL && dol_settings->arg_cli_lenght > 0)
							IOS_Write(fd,&dol_settings->arg_cli_lenght,sizeof(dol_settings->arg_cli_lenght));
						//we use IOS_Write + 2 of the 3 checks of ISFS_Write cause ISFS_Write in libogc has an irritating check to see if the address is aligned by 32 
						//(removed in libogc svn @ 22/07/2011)
						if(dol_settings->arg_command_line != NULL && dol_settings->arg_cli_lenght > 0)
							IOS_Write(fd,dol_settings->arg_command_line,dol_settings->arg_cli_lenght);
						ISFS_Close( fd );
					}
					PrintFormat( 0, ((rmode->viWidth /2)-((strlen("\"%s\" installed")+strlen(app_list[cur_off].app_name.c_str()))*13/2))>>1, 240, "\"%s\" installed", app_list[cur_off].app_name.c_str());
				}
			}
			else
			{
				PrintFormat( 1, TEXT_OFFSET("Writing file failed!"), 240, "Writing file failed!");
			}
			sleep(5);
			ClearScreen();
			redraw=true;

		}

		if ( WPAD_Pressed & WPAD_BUTTON_2 || WPAD_Pressed & WPAD_CLASSIC_BUTTON_X || PAD_Pressed & PAD_BUTTON_X )
		{
			ClearScreen();
			//Delete file

			PrintFormat( 0, TEXT_OFFSET("Deleting installed File..."), 208, "Deleting installed File...");

			ISFS_Delete("/title/00000001/00000002/data/main.nfo");

			//Check if there is already a main.dol installed
			s32 fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1|2 );

			if( fd >= 0 )	//delete old file
			{
				ISFS_Close( fd );
				ISFS_Delete("/title/00000001/00000002/data/main.bin");

				fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1|2 );

				if( fd >= 0 )	//file not delete
					PrintFormat( 0, TEXT_OFFSET("Failed"), 240, "Failed");
				else
					PrintFormat( 0, TEXT_OFFSET("Success"), 240, "Success");
			}
			else
				PrintFormat( 0, TEXT_OFFSET("No File installed..."), 240, "No File installed...");

			sleep(5);
			ClearScreen();
			redraw=true;
			ISFS_Close( fd );

		}

		if ( WPAD_Pressed & WPAD_BUTTON_1 || WPAD_Pressed & WPAD_CLASSIC_BUTTON_Y || PAD_Pressed & PAD_BUTTON_Y )
		{
			ClearScreen();
			PrintFormat( 1, TEXT_OFFSET("Loading binary..."), 208, "Loading binary...");	
			ret = BootDolFromDir(app_list[cur_off].app_path.c_str(),app_list[cur_off].HW_AHBPROT_ENABLED,app_list[cur_off].args);
			gprintf("loading %s ret %d\n",app_list[cur_off].app_path.c_str(),ret);
			PrintFormat( 1, TEXT_OFFSET("failed to load binary"), 224, "failed to load binary");
			sleep(3);
			ClearScreen();
			redraw=true;
		}
		if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;
			if (cur_off < min_pos)
			{
				min_pos = cur_off;
				if(app_list.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                        ");
						PrintFormat( 0,TEXT_OFFSET("               "),64+(max_pos+2)*16,"               ");
						PrintFormat( 0,TEXT_OFFSET("               "),64,"               ");
					}
				}
			}
			if (cur_off < 0)
			{
				cur_off = app_list.size() - 1;
				min_pos = app_list.size() - max_pos - 1;
			}
			redraw = true;
		}
		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;
			if (cur_off > (max_pos + min_pos))
			{
				min_pos = cur_off - max_pos;
				if(app_list.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                        ");
						PrintFormat( 0,TEXT_OFFSET("               "),64+(max_pos+2)*16,"               ");
						PrintFormat( 0,TEXT_OFFSET("               "),64,"               ");
					}
				}
			}
			if (cur_off >= (s32)app_list.size())
			{
				cur_off = 0;
				min_pos = 0;
			}
			redraw = true;
		}
		VIDEO_WaitVSync();
	}

	//free memory
	app_list.clear();

	return;
}
void AutoBootDol( void )
{
	s32 fd = 0;
	s32 r = 0;
	s32 Ios_to_load = 0;
	Elf32_Ehdr *ElfHdr = NULL;
	dolhdr *hdr = NULL;

	STACK_ALIGN(_dol_settings,dol_settings,sizeof(_dol_settings),32);
	memset(dol_settings,0,sizeof(_dol_settings));

	struct __argv argv;
	bzero(&argv, sizeof(argv));
	argv.argvMagic = 0;
	argv.length = 0;
	argv.commandLine = NULL;
	argv.argc = 0;
	argv.argv = &argv.commandLine;
	argv.endARGV = argv.argv + 1;
	
	fd = ISFS_Open("/title/00000001/00000002/data/main.nfo", 1 );
	if(fd >= 0)
	{
		STACK_ALIGN(fstats,status,sizeof(fstats),32);
		if (ISFS_GetFileStats(fd,status) < 0)
		{
			gdprintf("stats fail\n");
			ISFS_Close(fd);
			goto read_dol;
		}
		if(status->file_length < 2)
		{
			gdprintf("<2 size\n");
			ISFS_Delete("/title/00000001/00000002/data/main.nfo");
			ISFS_Close(fd);
			goto read_dol;
		}
		//read the argument count,AHBPROT bit and arg lenght
		r = ISFS_Read( fd, dol_settings, sizeof(s16)+sizeof(s32) );
		if(r < 0)
		{
			gdprintf("read fail.r = %x\n",r);
			ISFS_Close(fd);
			goto read_dol;
		}
		if(dol_settings->HW_AHBPROT_bit != 1 && dol_settings->HW_AHBPROT_bit != 0 )
		{
			//invalid. fuck the file then
			gdprintf("invalid. %d\n",dol_settings->HW_AHBPROT_bit);
			ISFS_Delete("/title/00000001/00000002/data/main.nfo");
			ISFS_Close(fd);
			goto reset_and_read;
		}
		if(dol_settings->argument_count == 0)
		{
			//no arguments. so lets not bother anymore
			ISFS_Close(fd);
			gdprintf("no args.\n");
			goto read_dol;
		}
		argv.length = dol_settings->arg_cli_lenght;
		argv.commandLine = (char*) mem_malloc(argv.length);
		if (argv.commandLine == NULL)
			goto reset_and_read;
		r = ISFS_Read( fd, argv.commandLine, argv.length );
		if(r <= 0)
		{
			gdprintf("failed to read arguments\n");
			goto reset_and_read;
		}
		ISFS_Close(fd);
		argv.argc = dol_settings->argument_count;
		argv.commandLine[argv.length - 1] = '\0';
		argv.argv = &argv.commandLine;
		argv.endARGV = argv.argv + 1;
		argv.argvMagic = ARGV_MAGIC; //everything is set so the magic is set so it becomes valid

	}
	goto read_dol;
reset_and_read:
	gprintf("something went wrong and wanted to bail out\n");
	if(argv.commandLine)
		mem_free(argv.commandLine);
	if(argv.length != 0)
		argv.length = 0;
	ISFS_Close(fd);
	dol_settings->HW_AHBPROT_bit = 0;
	dol_settings->argument_count = 0;
read_dol:
	fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1 );
	if( fd < 0 )
	{
		error = ERROR_BOOT_DOL_OPEN;
		goto return_dol;
	}

	void	(*entrypoint)();

	ElfHdr = (Elf32_Ehdr *)mem_align( 32, ALIGN32( sizeof( Elf32_Ehdr ) ) );
	if( ElfHdr == NULL )
	{
		error = ERROR_MALLOC;
		goto return_dol;
	}

	r = ISFS_Read( fd, ElfHdr, sizeof( Elf32_Ehdr ) );
	if( r < 0 || r != sizeof( Elf32_Ehdr ) )
	{
		error = ERROR_BOOT_DOL_READ;
		goto return_dol;
	}

	if( ElfHdr->e_ident[EI_MAG0] == 0x7F &&
		ElfHdr->e_ident[EI_MAG1] == 'E' &&
		ElfHdr->e_ident[EI_MAG2] == 'L' &&
		ElfHdr->e_ident[EI_MAG3] == 'F' )
	{
		gdprintf("ELF Found\n");
		gdprintf("Type:      \t%04X\n", ElfHdr->e_type );
		gdprintf("Machine:   \t%04X\n", ElfHdr->e_machine );
		gdprintf("Version:  %08X\n", ElfHdr->e_version );
		gdprintf("Entry:    %08X\n", ElfHdr->e_entry );
		gdprintf("Flags:    %08X\n", ElfHdr->e_flags );
		gdprintf("EHsize:    \t%04X\n\n", ElfHdr->e_ehsize );

		gdprintf("PHoff:    %08X\n",	ElfHdr->e_phoff );
		gdprintf("PHentsize: \t%04X\n",	ElfHdr->e_phentsize );
		gdprintf("PHnum:     \t%04X\n\n",ElfHdr->e_phnum );

		gdprintf("SHoff:    %08X\n",	ElfHdr->e_shoff );
		gdprintf("SHentsize: \t%04X\n",	ElfHdr->e_shentsize );
		gdprintf("SHnum:     \t%04X\n",	ElfHdr->e_shnum );
		gdprintf("SHstrndx:  \t%04X\n\n",ElfHdr->e_shstrndx );
		
		if( ( (ElfHdr->e_entry | 0x80000000) >= 0x81000000 ) && ( (ElfHdr->e_entry | 0x80000000) <= 0x81330000) )
		{
			gprintf("AutoBoot : ELF entrypoint error: abort!\n");
			goto return_dol;
		}
		if( ElfHdr->e_phnum == 0 )
			gdprintf("Warning program header entries are zero!\n");
		else 
		{
			for( int i=0; i < ElfHdr->e_phnum; ++i )
			{
				r = ISFS_Seek( fd, ElfHdr->e_phoff + sizeof( Elf32_Phdr ) * i, SEEK_SET );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_SEEK;
					mem_free(ElfHdr);
					goto return_dol;
				}

				Elf32_Phdr *phdr = (Elf32_Phdr *)mem_align( 32, ALIGN32( sizeof( Elf32_Phdr ) ) );
				if( phdr == NULL )
				{
					error = ERROR_MALLOC;
					mem_free(ElfHdr);
					goto return_dol;
				}
				memset(phdr,0,sizeof(Elf32_Phdr));
				r = ISFS_Read( fd, phdr, sizeof( Elf32_Phdr ) );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_READ;
					mem_free( phdr );
					mem_free(ElfHdr);
					goto return_dol;
				}
				gdprintf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz );
				r = ISFS_Seek( fd, phdr->p_offset, 0 );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_SEEK;
					mem_free( phdr );
					mem_free(ElfHdr);
					goto return_dol;
				}

				if(phdr->p_type == PT_LOAD && phdr->p_vaddr != 0)
				{
					r = ISFS_Read( fd, (void*)phdr->p_vaddr, phdr->p_filesz);
					if( r < 0 )
					{
						gprintf("AutoBootDol : ISFS_Read(%d)read failed of the program section addr\n",r);
						error = ERROR_BOOT_DOL_READ;
						mem_free( phdr );
						mem_free(ElfHdr);
						goto return_dol;
					}
				}
				mem_free( phdr );
			}
		}
		if( ElfHdr->e_shnum == 0 )
			gdprintf("Warning section header entries are zero!\n");
		else 
		{
			Elf32_Shdr *shdr = (Elf32_Shdr *)mem_align( 32, ALIGN32( sizeof( Elf32_Shdr ) ) );
			if( shdr == NULL )
			{
				error = ERROR_MALLOC;
				mem_free(ElfHdr);
				goto return_dol;
			}
			memset(shdr,0,sizeof(Elf32_Shdr));
			for( int i=0; i < ElfHdr->e_shnum; ++i )
			{
				r = ISFS_Seek( fd, ElfHdr->e_shoff + sizeof( Elf32_Shdr ) * i, SEEK_SET );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_SEEK;
					mem_free( shdr );
					mem_free(ElfHdr);
					goto return_dol;
				}

				r = ISFS_Read( fd, shdr, sizeof( Elf32_Shdr ) );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_READ;
					mem_free( shdr );
					mem_free(ElfHdr);
					goto return_dol;
				}

				if( shdr->sh_type == 0 || shdr->sh_size == 0 )
					continue;

#ifdef DEBUG
				if( shdr->sh_type > 17 )
					gdprintf("Warning the type: %08X could be invalid!\n", shdr->sh_type );

				if( shdr->sh_flags & ~0xF0000007 )
					gdprintf("Warning the flag: %08X is invalid!\n", shdr->sh_flags );

				gdprintf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr->sh_type, shdr->sh_offset, shdr->sh_name, shdr->sh_addr, shdr->sh_size );
#endif
				r = ISFS_Seek( fd, shdr->sh_offset, 0 );
				if( r < 0 )
				{
					error = ERROR_BOOT_DOL_SEEK;
					mem_free( shdr );
					mem_free(ElfHdr);
					goto return_dol;
				}

				
				if (shdr->sh_type == SHT_NOBITS)
				{
					r = ISFS_Read( fd, (void*)(shdr->sh_addr | 0x80000000), shdr->sh_size);
					if( r < 0 )
					{
						gprintf("AutoBootDol : ISFS_Read(%d) data header\n",r);
						error = ERROR_BOOT_DOL_READ;
						mem_free( shdr );
						mem_free(ElfHdr);
						goto return_dol;
					}
				}

			}
			mem_free( shdr );
		}

		ISFS_Close( fd );
		entrypoint = (void (*)())(ElfHdr->e_entry | 0x80000000);
		mem_free(ElfHdr);

	} else {
		if(ElfHdr != NULL)
			mem_free(ElfHdr);
		//Dol
		//read the header
		hdr = (dolhdr *)mem_align(32, ALIGN32( sizeof( dolhdr ) ) );
		if( hdr == NULL )
		{
			error = ERROR_MALLOC;
			goto return_dol;
		}

		s32 r = ISFS_Seek( fd, 0, 0);
		if( r < 0 )
		{
			gprintf("AutoBootDol : ISFS_Seek(%d)\n", r);
			error = ERROR_BOOT_DOL_SEEK;
			goto return_dol;
		}

		r = ISFS_Read( fd, hdr, sizeof(dolhdr) );

		if( r < 0 || r != sizeof(dolhdr) )
		{
			gprintf("AutoBootDol : ISFS_Read(%d)\n", r);
			error = ERROR_BOOT_DOL_READ;
			goto return_dol;
		}
		if( hdr->entrypoint >= 0x81000000 && hdr->entrypoint <= 0x81330000 )
		{
			gprintf("BootDolFromFat : entrypoint/BSS error: abort!\n");
			error = ERROR_BOOT_DOL_ENTRYPOINT;
			goto return_dol;
		}

		if(DVDCheckCover())
		{
			gprintf("AutoBootDol : excecuting StopDisc Async...\n");
			DVDStopDisc(true);
		}
		else
		{
			gprintf("AutoBootDol : Skipping StopDisc -> no drive or disc in drive\n");
		}
		gdprintf("\nText Sections:\n");
		int i=0;
		for (i = 0; i < 6; i++)
		{
			if( hdr->sizeText[i] && hdr->addressText[i] && hdr->offsetText[i] )
			{
				if(ISFS_Seek( fd, hdr->offsetText[i], SEEK_SET )<0)
				{
					error = ERROR_BOOT_DOL_SEEK;
					goto return_dol;
				}
				if(ISFS_Read( fd, (void*)(hdr->addressText[i]), hdr->sizeText[i] )<0)
				{
					error = ERROR_BOOT_DOL_READ;
					goto return_dol;
				}
				DCInvalidateRange( (void*)(hdr->addressText[i]), hdr->sizeText[i] );
				gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetText[i]), hdr->addressText[i], hdr->sizeText[i]);
			}
		}
		gdprintf("\nData Sections:\n");
		// data sections
		for (i = 0; i <= 10; i++)
		{
			if( hdr->sizeData[i] && hdr->addressData[i] && hdr->offsetData[i] )
			{
				if(ISFS_Seek( fd, hdr->offsetData[i], SEEK_SET )<0)
				{
					error = ERROR_BOOT_DOL_SEEK;
					goto return_dol;
				}
				if( ISFS_Read( fd, (void*)(hdr->addressData[i]), hdr->sizeData[i] )<0)
				{
					error = ERROR_BOOT_DOL_READ;
					goto return_dol;
				}

				DCInvalidateRange( (void*)(hdr->addressData[i]), hdr->sizeData[i] );
				gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetData[i]), hdr->addressData[i], hdr->sizeData[i]);
			}
		}
		if (dol_settings->argument_count > 0 && argv.argvMagic == ARGV_MAGIC)
        {
			void* new_argv = (void*)(hdr->entrypoint + 8);
			memmove(new_argv, &argv, sizeof(__argv));
			DCFlushRange(new_argv, sizeof(__argv));
        }
		entrypoint = (void (*)())(hdr->entrypoint);

	}
	ISFS_Close(fd);
	if( entrypoint == 0x00000000 )
	{
		error = ERROR_BOOT_DOL_ENTRYPOINT;
		goto return_dol;
	}
	for(int i=0;i<WPAD_MAX_WIIMOTES;i++) {
		if(WPAD_Probe(i,0) < 0)
			continue;
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}
	if(DvdKilled() < 1)
	{
		gprintf("checking DVD drive...\n");
		while(DvdKilled() < 1);
	}

	gprintf("Entrypoint: %08X\n", (u32)(entrypoint) );
	WPAD_Shutdown();
	ClearState();
	ISFS_Deinitialize();
	ShutdownDevices();
	USB_Deinitialize();

	if( !isIOSstub(58) )
	{
		Ios_to_load = 58;
	}
	else if( !isIOSstub(61) )
	{
		Ios_to_load = 61;
	}
	else if( !isIOSstub(IOS_GetPreferredVersion()) )
	{
		Ios_to_load = IOS_GetPreferredVersion();
	}
	else
	{
		PrintFormat( 1, TEXT_OFFSET("failed to reload ios for homebrew! ios is a stub!"), 208, "failed to reload ios for homebrew! ios is a stub!");
		sleep(2);	
	}

	if(Ios_to_load > 2 && Ios_to_load < 255)
	{
		s8 bAHBPROT = dol_settings->HW_AHBPROT_bit;
		ReloadIos(Ios_to_load,&bAHBPROT);
		system_state.ReloadedIOS = 1;
	}
	__IOS_ShutdownSubsystems();
	//slightly modified loading code from USBLOADER GX...
	u32 level;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (level);
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	entrypoint();
	_CPU_ISR_Restore (level);
	//never gonna happen; but failsafe
	WPAD_Init();
	PAD_Init();
	ISFS_Initialize();
return_dol:
	ISFS_Close(fd);
	if(argv.commandLine != NULL)
		mem_free(argv.commandLine);
	if(hdr != NULL)
		mem_free(hdr);
	if(ElfHdr != NULL)
		mem_free(ElfHdr);
	return;
}
void CheckForUpdate()
{
	ClearScreen();
	PrintFormat( 1, TEXT_OFFSET("Initialising Wifi..."), 208, "Initialising Wifi...");
	if (InitNetwork() < 0 )
	{
		PrintFormat( 1, TEXT_OFFSET("failed to initialise wifi"), 224, "failed to initialise wifi");
		sleep(5);
		return;
	}
	s32 file_size = 0;
//start update
//---------------
	UpdateStruct UpdateFile;
	u8* buffer = NULL;
	file_size = GetHTTPFile("www.dacotaco.com","/priiloader/version.dat",buffer,0);
	if ( file_size <= 0 || file_size != (s32)sizeof(UpdateStruct) || buffer == NULL)
	{
		PrintFormat( 1, TEXT_OFFSET("error getting versions from server"), 224, "error getting versions from server");
		if (file_size < -9)
		{
			//free pointer
			mem_free(buffer);
		}
		if (file_size < 0 && file_size > -4)
		{
			//errors connecting to server
			gprintf("connection failure. error %d\n",file_size);
		}
		else if (file_size == -7)
		{
			gprintf("CheckForUpdate : HTTP Error %s!\n",Get_Last_reply());
		}
		else if ( file_size < 0 )
		{
			gprintf("CheckForUpdate : GetHTTPFile error %d\n",file_size);
		}
		else if (file_size != (s32)sizeof(UpdateStruct))
		{
			gprintf("CheckForUpdate : file_size != UpdateStruct\n");
		}
		sleep(5);
		mem_free(buffer);
		net_deinit();
		return;
	}
	memcpy(&UpdateFile,buffer,sizeof(UpdateStruct));
	mem_free(buffer);


//generate update list if any
//----------------------------------------
	file_size = 0;
	u8* Data = NULL;
	u8 DownloadedBeta = 0;
	u8 BetaUpdates = 0;
	u8 VersionUpdates = 0;
	u8* Changelog = NULL;
	u8 redraw = 1;
	s8 cur_off = 0;
	ClearScreen();
	//make a nice list of the updates
	if ( (VERSION < UpdateFile.version) || (VERSION == UpdateFile.version && BETAVERSION > 0) )
		VersionUpdates = 1;
	//to make the if short :
	// - beta updates should be enabled
	// - the current betaversion should be less then the online beta
	// - the current version should < the beta OR the version == the beta IF a beta is installed
	if ( 
		SGetSetting(SETTING_SHOWBETAUPDATES) && 
		BETAVERSION < UpdateFile.beta_number && 
		( ( VERSION < UpdateFile.beta_version && BETAVERSION == 0 ) || ( VERSION == UpdateFile.beta_version && BETAVERSION > 0 ) ) 
		)
	{
		BetaUpdates = 1;
	}

	while(1)
	{
		if(redraw)
		{
			if ( VersionUpdates )
			{
				if( (UpdateFile.version&0xFF) % 10 == 0 )
				{
					PrintFormat( cur_off == 0, 16, 64+(16*1), "Update to %d.%d",UpdateFile.version >> 8,(UpdateFile.version&0xFF) / 10);
				}
				else
				{
					PrintFormat( cur_off == 0, 16, 64+(16*1), "Update to v%d.%d.%d", UpdateFile.version >>8, (UpdateFile.version&0xFF) / 10,(UpdateFile.version&0xFF) % 10 );
					//PrintFormat( cur_off == 0, 16, 64+(16*1), "Update to %d.%d",UpdateFile.version >> 8,UpdateFile.version&0xFF);
				}
			}
			else
			{
				PrintFormat( cur_off == 0, 16, 64+(16*1), "No major updates\n");
			}
			if (SGetSetting(SETTING_SHOWBETAUPDATES))
			{
				
				if ( BetaUpdates )
				{
					if( (UpdateFile.version&0xFF) % 10 == 0 )
					{
						//PrintFormat( cur_off==1, 16, 64+(16*2), "Update to %d.%d beta %d",UpdateFile.beta_version >> 8,UpdateFile.beta_version&0xFF, UpdateFile.beta_number);
						PrintFormat( cur_off==1, 16, 64+(16*2), "Update to %d.%d beta %d",UpdateFile.beta_version >> 8,UpdateFile.beta_version&0xFF, UpdateFile.beta_number);
					}
					else
					{
						PrintFormat( cur_off==1, 16, 64+(16*2), "Update to %d.%d.%d beta %d",UpdateFile.beta_version >> 8,(UpdateFile.beta_version&0xFF) / 10,(UpdateFile.beta_version&0xFF) % 10, UpdateFile.beta_number);
					}
				}
				else
				{
					PrintFormat( cur_off==1, 16, 64+(16*2), "No Beta update\n");
				}
			}	
			if ( VersionUpdates == 0 && BetaUpdates == 0)
			{
				sleep(2);
				return;
			}
			PrintFormat( 0, TEXT_OFFSET("A(A) Download Update       "), rmode->viHeight-48, "A(A) Download Update       ");
			PrintFormat( 0, TEXT_OFFSET("B(B) Cancel Update         "), rmode->viHeight-32, "B(B) Cancel Update         ");
			redraw = 0;
		}

		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

		if ( WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B )
		{
			return;
		}
		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			if(cur_off == 0 && VersionUpdates == 1)
			{
				DownloadedBeta = 0;
				break;
			}
			else if (cur_off == 1 && BetaUpdates == 1)
			{
				DownloadedBeta = 1;
				break;
			}
			redraw = 1;
		}
		if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;
			if(cur_off < 0)
			{
				if (SGetSetting(SETTING_SHOWBETAUPDATES))
				{	
					
						cur_off = 1;			
				}
				else
				{
						cur_off = 0;	
				}
			}
			redraw = 1;
		}
		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;
			if (SGetSetting(SETTING_SHOWBETAUPDATES))
			{
				if(cur_off > 1)
					cur_off = 0;
			}
			else
			{
				if(cur_off > 0)
					cur_off = 0;
			}
			redraw = 1;
		}
		VIDEO_WaitVSync();
	}
//Download changelog and ask to proceed or not
//------------------------------------------------------
	gprintf("downloading changelog...\n");
	if(DownloadedBeta)
	{
		file_size = GetHTTPFile("www.dacotaco.com","/priiloader/changelog_beta.txt",Changelog,0);
	}
	else
	{
		file_size = GetHTTPFile("www.dacotaco.com","/priiloader/changelog.txt",Changelog,0);
	}
	if (file_size > 0)
	{
		Changelog[file_size-1] = 0; // playing it safe for future shit
		ClearScreen();
		char se[5];
		u8 line = 0;
		u8 min_line = 0;
		u8 max_line = 0;
		if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
			max_line = 14;
		else
			max_line = 19;
		u8 redraw = 1;

		char *ptr;
		std::vector<char*> lines;
		if( strpbrk((char*)Changelog , "\r\n") )
			sprintf(se, "\r\n");
		else
			sprintf(se, "\n");
		ptr = strtok((char*)Changelog, se);
		while (ptr != NULL && (u32)ptr < (u32)Changelog+file_size ) //prevent it from going to far cause strtok is fucking dangerous.
		{
			lines.push_back(ptr);
			ptr = strtok (NULL, se);
		}
		if( max_line >= lines.size() )
			max_line = lines.size()-1;

		PrintFormat( 1, TEXT_OFFSET(" Changelog "), 64+(16*1), " Changelog ");
		PrintFormat( 1, TEXT_OFFSET("-----------"), 64+(16*2), "-----------");
		if((lines.size() -1) > max_line)
		{
			PrintFormat( 0, TEXT_OFFSET("Up    Scroll Up        "), rmode->viHeight-80, "Up    Scroll Up");
			PrintFormat( 0, TEXT_OFFSET("Down  Scroll Down      "), rmode->viHeight-64, "Down  Scroll Down");
		}
		PrintFormat( 0, TEXT_OFFSET("A(A)  Proceed(Download)"), rmode->viHeight-48, "A(A)  Proceed(Download)");
		PrintFormat( 0, TEXT_OFFSET("B(B)  Cancel Update    "), rmode->viHeight-32, "B(B)  Cancel Update    ");
		u32 PAD_Pressed = 0;
		u32 WPAD_Pressed = 0;
		while(1)
		{
			WPAD_ScanPads();
			PAD_ScanPads();

			WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
			PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
			if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
			{
				mem_free(Changelog);
				break;
			}
			if ( WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B || PAD_Pressed & PAD_BUTTON_B )
			{
				mem_free(Changelog);
				ClearScreen();
				return;
			}
			if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
			{
				if ( (min_line+max_line) < lines.size()-1 )
				{
					min_line++;
					redraw = true;
				}
			}
			if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
			{
				if ( min_line > 0 )
				{
					min_line--;
					redraw = true;
				}
			}
			if ( redraw )
			{
				for(u8 i = min_line; i <= (min_line+max_line); i++)
				{
					PrintFormat( 0, 16, 64 + ((line+3)*16), "                                                                ");
					PrintFormat( 0, 16, 64 + ((line+3)*16), "%s", lines[i]);
					if( i < lines.size()-1 && i == (min_line+max_line) )
						PrintFormat( 0, 16, 64 + ((line+4)*16), "...");
					else
						PrintFormat( 0, 16, 64 + ((line+4)*16), "   ");
					line++;
				}
				redraw = false;
				line = 0;
			}
			VIDEO_WaitVSync();
		}
	}
	else if(file_size < 0)
	{
		if(file_size < -9)
			mem_free(Changelog);
		gprintf("CheckForUpdate : failed to get changelog.error %d, HTTP reply %s\n",file_size,Get_Last_reply());
	}
//The choice is made. lets download what the user wanted :)
//--------------------------------------------------------------
	ClearScreen();
	gprintf("downloading %s\n",DownloadedBeta?"beta":"update");
	if(DownloadedBeta)
	{
		PrintFormat( 1, TEXT_OFFSET("downloading   .   beta   ..."), 208, "downloading %d.%d beta %d...",UpdateFile.beta_version >> 8,UpdateFile.beta_version&0xFF, UpdateFile.beta_number);
		file_size = GetHTTPFile("www.dacotaco.com","/priiloader/Priiloader_Beta.dol",Data,0);
		//download beta
	}
	else
	{
		PrintFormat( 1, TEXT_OFFSET("downloading   .  ..."), 208, "downloading %d.%d ...",UpdateFile.version >> 8,UpdateFile.version&0xFF);
		file_size = GetHTTPFile("www.dacotaco.com","/priiloader/Priiloader_Update.dol",Data,0);
		//download Update
	}
	if ( file_size <= 0 )
	{
		if (file_size < 0 && file_size > -4)
		{
			//errors connecting to server
			gprintf("connection failure: error %d\n",file_size);
		}
		else if (file_size == -7)
		{
			gprintf("HTTP Error %s!\n",Get_Last_reply());
		}
		else
		{
			if(file_size < -9)
				mem_free(Data);
			gprintf("getting update error %d\n",file_size);
		}
		PrintFormat( 1, TEXT_OFFSET("error getting file from server"), 224, "error getting file from server");
		sleep(2);
		return;
	}
	else
	{
		SHA1 sha; // SHA-1 class
		u32 FileHash[5];
		sha.Reset();
		sha.Input(Data,file_size);
		if (!sha.Result(FileHash))
		{
			gprintf("sha: could not compute Hash of Update!\nHash : 00 00 00 00 00\n");
		}
		else
		{
			gprintf( "Downloaded Update : %08X %08X %08X %08X %08X\n",
			FileHash[0],
			FileHash[1],
			FileHash[2],
			FileHash[3],
			FileHash[4]);
		}
		gprintf("Online : ");
		if (!DownloadedBeta)
		{
			gprintf("%08X %08X %08X %08X %08X\n"
					,UpdateFile.SHA1_Hash[0]
					,UpdateFile.SHA1_Hash[1]
					,UpdateFile.SHA1_Hash[2]
					,UpdateFile.SHA1_Hash[3]
					,UpdateFile.SHA1_Hash[4]);
		}
		else
		{
			gprintf("%08X %08X %08X %08X %08X\n"
					,UpdateFile.beta_SHA1_Hash[0]
					,UpdateFile.beta_SHA1_Hash[1]
					,UpdateFile.beta_SHA1_Hash[2]
					,UpdateFile.beta_SHA1_Hash[3]
					,UpdateFile.beta_SHA1_Hash[4]);
		}

		if (
			( !DownloadedBeta && (
			UpdateFile.SHA1_Hash[0] == FileHash[0] &&
			UpdateFile.SHA1_Hash[1] == FileHash[1] &&
			UpdateFile.SHA1_Hash[2] == FileHash[2] &&
			UpdateFile.SHA1_Hash[3] == FileHash[3] &&
			UpdateFile.SHA1_Hash[4] == FileHash[4] ) ) ||

			( DownloadedBeta && (
			UpdateFile.beta_SHA1_Hash[0] == FileHash[0] &&
			UpdateFile.beta_SHA1_Hash[1] == FileHash[1] &&
			UpdateFile.beta_SHA1_Hash[2] == FileHash[2] &&
			UpdateFile.beta_SHA1_Hash[3] == FileHash[3] &&
			UpdateFile.beta_SHA1_Hash[4] == FileHash[4] ) ) )
		{
			gprintf("Hash check complete. booting file...\n");
		}
		else
		{
			gprintf("File not the same : hash check failure!\n");
			PrintFormat( 1, TEXT_OFFSET("Error Downloading Update"), 224, "Error Downloading Update");
			sleep(5);
			mem_free(Data);
			return;
		}

//Load the dol
//---------------------------------------------------
		ClearScreen();
		if(DownloadedBeta)
		{
			PrintFormat( 1, TEXT_OFFSET("loading   .   beta   ..."), 208, "loading %d.%d beta %d...",UpdateFile.beta_version >> 8,UpdateFile.beta_version&0xFF, UpdateFile.beta_number);
		}
		else
		{
			PrintFormat( 1, TEXT_OFFSET("loading   .  ..."), 208, "loading %d.%d ...",UpdateFile.version >> 8,UpdateFile.version&0xFF);
		}
		sleep(1);
		//load the fresh installer
		net_deinit();
		BootDolFromMem(Data,1,NULL);
		mem_free(Data);
		PrintFormat( 1, TEXT_OFFSET("Error Booting Update dol"), 224, "Error Booting Update dol");
		sleep(5);
	}
	return;
}
void HandleSTMEvent(u32 event)
{
	if (system_state.InMainMenu == 0) //we have no buisness with the reset/power when we aren't in main menu
		return;
	double ontime;
	switch(event)
	{
		case STM_EVENT_POWER:
			system_state.Shutdown=1;
			system_state.BootSysMenu = 0;
			break;
		case STM_EVENT_RESET:
			if (system_state.BootSysMenu == 0 && WPAD_Probe(0,0) < 0)
			{
				time_t inloop;
				time(&inloop);
				ontime = difftime(inloop, startloop);
				//gprintf("ontime = %4.2fs\n",ontime);
				if (ontime >= 15)
					system_state.BootSysMenu = 1;
			}
		default:
			break;
	}
	return;
}
void HandleWiiMoteEvent(s32 chan)
{
	if(system_state.InMainMenu == 0)//see HandleSTMevent()
		return;
	system_state.Shutdown=1;
	return;
}
void Autoboot_System( void )
{
  	if( SGetSetting(SETTING_PASSCHECKMENU) && SGetSetting(SETTING_AUTBOOT) != AUTOBOOT_DISABLED && SGetSetting(SETTING_AUTBOOT) != AUTOBOOT_ERROR )
 		password_check();

	switch( SGetSetting(SETTING_AUTBOOT) )
	{
		case AUTOBOOT_SYS:
			gprintf("AutoBoot:System Menu\n");
			BootMainSysMenu(0);
			break;
		case AUTOBOOT_HBC:
			gprintf("AutoBoot:Homebrew Channel\n");
			LoadHBC();
			break;
		case AUTOBOOT_BOOTMII_IOS:
			gprintf("AutoBoot:BootMii IOS\n");
			LoadBootMii();
			error=ERROR_BOOT_BOOTMII;
			break;
		case AUTOBOOT_FILE:
			gprintf("AutoBoot:Installed File\n");
			AutoBootDol();
			break;
		case AUTOBOOT_ERROR:
			error=ERROR_BOOT_ERROR;
		case AUTOBOOT_DISABLED:
		default:
			break;
	}
	return;
}
s8 CheckMagicWords( void )
{
	//0x4461636f = "Daco" in hex, 0x50756e65 = "Pune"
	if( *(vu32*)MAGIC_WORD_ADDRESS_1 == 0x4461636f || *(vu32*)MAGIC_WORD_ADDRESS_1 == 0x50756e65 )
		return 1;
	if( *(vu32*)MAGIC_WORD_ADDRESS_2 == 0x4461636f || *(vu32*)MAGIC_WORD_ADDRESS_2 == 0x50756e65 )
		return 1;
	return 0;
}
//a temp main to play around in for the crash <<
int main2(int argc, char **argv)
{
	CheckForGecko();
	time(&startloop);
#ifdef DEBUG
	InitGDBDebug();
#endif
	gprintf("priiloader\n");
	gprintf("Built   : %s %s\n", __DATE__, __TIME__ );
	gprintf("Version : %d.%d (rev %s)\n", VERSION>>16, VERSION&0xFFFF, SVN_REV_STR);
	gprintf("Firmware: %d.%d.%d\n", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

	s32 r = ISFS_Initialize();
	if( r < 0 )
	{
		*(vu32*)0xCD8000C0 |= 0x20;
		error=ERROR_ISFS_INIT;
	}

	gprintf("\"Magic Priiloader word\": %x - %x\n",*(vu32*)MAGIC_WORD_ADDRESS_2 ,*(vu32*)MAGIC_WORD_ADDRESS_1);
	STM_RegisterEventHandler(HandleSTMEvent);
	//memset(&system_state,0,sizeof(wii_state));
	system_state.InMainMenu = 1;
	gprintf("hold reset now\n");
	sleep(5);
	gprintf("lets go\n");
	//that worked

	PollDevices();
	//enable the commented gprintf in HandleSTMEvent for this to crash
	SetDumpDebug(1);
	gprintf("hold reset now\n");
	sleep(5);
	gprintf("lets go\n");
	//above crashed

	s16 Bootstate = 0;//CheckBootState();
	gprintf("BootState:%d\n", Bootstate );

	//init video first so we can see crashes :)
	InitVideo();

	r = PAD_Init();
	gprintf("PAD_Init():%d\n", r );

	s8 cur_off=0;
	s8 redraw=true;
	u32 SysVersion= 512;
#ifdef DEBUG
	gdprintf("priiloader v%d.%d DEBUG (Sys:%d)(IOS:%d)(%s %s)\n", VERSION>>8, VERSION&0xFF, SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
#else
	#if BETAVERSION > 0
		gprintf("priiloader v%d.%d BETA %d (Sys:%d)(IOS:%d)(%s %s)\n", VERSION>>8, VERSION&0xFF,BETAVERSION,SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
	#endif
#endif
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
 
		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			ClearScreen();
			system_state.InMainMenu = 0;
			switch(cur_off)
			{
				case 0:
					BootMainSysMenu(1);
					if(!error)
						error=ERROR_SYSMENU_GENERAL;
					break;
				case 1:		//Load HBC
					LoadHBC();
					break;
				case 2: //Load Bootmii
					LoadBootMii();
					//well that failed...
					error=ERROR_BOOT_BOOTMII;
					break;
				case 3: // show titles list
					LoadListTitles();
					break;
				case 4:		//load main.bin from /title/00000001/00000002/data/ dir
					AutoBootDol();
					break;
				case 5:
					InstallLoadDOL();
					break;
				case 6:
					SysHackHashSettings();
					break;
				case 7:
					CheckForUpdate();
					net_deinit();
					break;
				case 8:
					InstallPassword();
					break;
				case 9:
					SetSettings();
					break;
				default:
					break;

			}
			system_state.InMainMenu = 1;
			ClearScreen();
			redraw=true;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;

			if( error == ERROR_UPDATE )
			{
				if( cur_off >= 11 )
					cur_off = 0;
			}else {

				if( cur_off >= 10 )
					cur_off = 0;
			}
			//gprintf("loool %d-%d-%d\n",*(vu32*)0xCC003000,(*(vu32*)0xCC003000)>>16,((*(vu32*)0xCC003000)>>16)&1);
			redraw=true;
		} else if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;

			if( cur_off < 0 )
			{
				if( error == ERROR_UPDATE )
				{
					cur_off=11-1;
				} else {
					cur_off=10-1;
				}
			}

			redraw=true;
		}

		if( redraw )
		{

			if (error > 0)
			{
				ShowError();
				error = ERROR_NONE;
			}
			redraw = false;
		}
		
		VIDEO_WaitVSync();
	}

	return 0;
}
int main(int argc, char **argv)
{
	CheckForGecko();
#ifdef DEBUG
	InitGDBDebug();
#endif
	gprintf("priiloader\n");
	gprintf("Built   : %s %s\n", __DATE__, __TIME__ );
	gprintf("Version : %d.%d.%d (rev %s)\n", VERSION>>8, (VERSION&0xFF) / 10,(VERSION&0xFF) % 10,SVN_REV_STR);//VERSION>>16, VERSION&0xFFFF, SVN_REV_STR);
	gprintf("Firmware: %d.%d.%d\n", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

	/**(vu32*)0x80000020 = 0x0D15EA5E;				// Magic word (how did the console boot?)
	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed

	*(vu32*)0x80000040 = 0x00000000;				// Debugger present?
	*(vu32*)0x80000044 = 0x00000000;				// Debugger Exception mask
	*(vu32*)0x80000048 = 0x00000000;				// Exception hook destination 
	*(vu32*)0x8000004C = 0x00000000;				// Temp for LR
	*(vu32*)0x80003100 = 0x01800000;				// Physical Mem1 Size
	*(vu32*)0x80003104 = 0x01800000;				// Console Simulated Mem1 Size

	*(vu32*)0x80003118 = 0x04000000;				// Physical Mem2 Size
	*(vu32*)0x8000311C = 0x04000000;				// Console Simulated Mem2 Size

	*(vu32*)0x80003120 = 0x93400000;				// MEM2 end address ?*/

	s32 r = ISFS_Initialize();
	if( r < 0 )
	{
		*(vu32*)0xCD8000C0 |= 0x20;
		error=ERROR_ISFS_INIT;
	}

	AddMem2Area (14*1024*1024, OTHER_AREA);
	LoadHBCStub();
	gprintf("\"Magic Priiloader word\": %x - %x\n",*(vu32*)MAGIC_WORD_ADDRESS_2 ,*(vu32*)MAGIC_WORD_ADDRESS_1);
	LoadSettings();
	if(SGetSetting(SETTING_DUMPGECKOTEXT) == 1)
	{
		PollDevices();
	}

	SetDumpDebug(SGetSetting(SETTING_DUMPGECKOTEXT));
	s16 Bootstate = CheckBootState();
	gprintf("BootState:%d\n", Bootstate );
	memset(&system_state,0,sizeof(wii_state));
	StateFlags temp;
	temp = GetStateFlags();
	gprintf("Bootstate %u detected. DiscState %u ,ReturnTo %u & Flags %u & checksum %u\n",temp.type,temp.discstate,temp.returnto,temp.flags,temp.checksum);
	//Check reset button state
	if( ((*(vu32*)0xCC003000)>>16) == 1 && CheckMagicWords() == 0) //if( ((*(vu32*)0xCC003000)>>16)&1 && !CheckMagicWords())
	{
		//Check autoboot settings
		switch( Bootstate )
		{
			case TYPE_UNKNOWN: //255 or -1, only seen when shutting down from MIOS or booting dol from HBC. it is actually an invalid value
				temp = GetStateFlags();
				gprintf("Bootstate %u detected. DiscState %u ,ReturnTo %u & Flags %u\n",temp.type,temp.discstate,temp.returnto,temp.flags);
				if( temp.flags == 130 ) //&& temp.discstate != 2)
				{
					//if the flag is 130, its probably shutdown from mios. in that case system menu 
					//will handle it perfectly (and i quote from SM's OSreport : "Shutdown system from GC!")
					//it seems to reboot into bootstate 5. but its safer to let SM handle it
					gprintf("255:System Menu\n");
					BootMainSysMenu(0);
				}
				else
				{
					Autoboot_System();
				}
				break;
			case TYPE_SHUTDOWNSYSTEM: // 5 - shutdown
				ClearState();
				//check for valid nandboot shitzle. if its found we need to change bootstate to 4.
				//yellow8 claims system menu reset everything then, but it didn't on my wii (joy). this is why its not activated code.
				//its not fully confirmed system menu does it(or ios while being standby) and if system menu does indeed clear it.
				/*if(VerifyNandBootInfo())
				{
					gprintf("Verifty of NandBootInfo : 1\nbootstate changed to %d\n",CheckBootState());
				}
				else
				{
					gprintf("Verifty of NandBootInfo : 0\n");
				}*/
				if(!SGetSetting(SETTING_SHUTDOWNTOPRELOADER))
				{
					gprintf("Shutting down...\n");
					DVDStopDisc(true);
					ShutdownDevices();
					USB_Deinitialize();
					*(vu32*)0xCD8000C0 &= ~0x20;
					Control_VI_Regs(0);
					while(DvdKilled() < 1);
					if( SGetSetting(SETTING_IGNORESHUTDOWNMODE) )
					{
						STM_ShutdownToStandby();

					} else {
						if( CONF_GetShutdownMode() == CONF_SHUTDOWN_STANDBY )
						{
							//standby = red = off							
							STM_ShutdownToStandby();
						}
						else
						{
							//idle = orange = standby
							s32 ret;
							ret = CONF_GetIdleLedMode();
							if (ret >= 0 && ret <= 2)
								STM_SetLedMode(ret);
							STM_ShutdownToIdle();
						}
					}
				}
				break;
			case RETURN_TO_ARGS: //2 - normal reboot which funny enough doesn't happen very often
			case TYPE_RETURN: //3 - return to system menu
				switch( SGetSetting(SETTING_RETURNTO) )
				{
					case RETURNTO_SYSMENU:
						gprintf("ReturnTo:System Menu\n");
						BootMainSysMenu(0);
					break;

					case RETURNTO_AUTOBOOT:
						Autoboot_System();
					break;

					default:
					break;
				}
				break;
			case TYPE_NANDBOOT: // 4 - nandboot
				//apparently a boot state in which the system menu auto boots a title. read more : http://wiibrew.org/wiki/WiiConnect24#WC24_title_booting
				//as it hardly happens i guess nothing bad can happen if we ignore it and just do autoboot instead :)
			case RETURN_TO_SETTINGS: // 1 - Boot when fully shutdown & wiiconnect24 is off. why its called RETURN_TO_SETTINGS i have no clue...
			case RETURN_TO_MENU: // 0 - boot when wiiconnect24 is on
				Autoboot_System();
				break;
			default :
				if( ClearState() < 0 )
				{
					error = ERROR_STATE_CLEAR;
					gprintf("ClearState failure\n");
				}
				break;

		}
	}
	//remove the "Magic Priiloader word" cause it has done its purpose
	//Plan B address : 0x93FFFFFA
	if(*(vu32*)MAGIC_WORD_ADDRESS_1 == 0x4461636f || *(vu32*)MAGIC_WORD_ADDRESS_2 == 0x4461636f)
 	{
		gprintf("\"Magic Priiloader Word\" 'Daco' found!\n");
		gprintf("clearing memory of the \"Magic Priiloader Word\"\n");
		*(vu32*)MAGIC_WORD_ADDRESS_1 = 0x00000000;
		DCFlushRange((void*)MAGIC_WORD_ADDRESS_1,4);
		*(vu32*)MAGIC_WORD_ADDRESS_2 = 0x00000000;
		DCFlushRange((void*)MAGIC_WORD_ADDRESS_2,4);
	}
	else if(*(vu32*)MAGIC_WORD_ADDRESS_1 == 0x50756e65 || *(vu32*)MAGIC_WORD_ADDRESS_2 == 0x50756e65)
	{
		//detected the force for sys menu
		gprintf("\"Magic Priiloader Word\" 'Pune' found!\n");
		gprintf("clearing memory of the \"Magic Priiloader Word\" and starting system menu...\n");
		*(vu32*)MAGIC_WORD_ADDRESS_1 = 0x00000000;
		DCFlushRange((void*)MAGIC_WORD_ADDRESS_1,4);
		*(vu32*)MAGIC_WORD_ADDRESS_2 = 0x00000000;
		DCFlushRange((void*)MAGIC_WORD_ADDRESS_2,4);
		BootMainSysMenu(0);
	}
	else if( (
			( SGetSetting(SETTING_AUTBOOT) != AUTOBOOT_DISABLED && ( Bootstate < 2 || Bootstate == 4 ) ) 
		 || ( SGetSetting(SETTING_RETURNTO) != RETURNTO_PRIILOADER && Bootstate > 1 && Bootstate != 4 ) 
		 || ( SGetSetting(SETTING_SHUTDOWNTOPRELOADER) == 0 && Bootstate == 5 ) ) 
		 && error == 0 )
	{
		gprintf("Reset Button is held down\n");
	}

	//init video first so we can see crashes :)
	InitVideo();
  	if( SGetSetting(SETTING_PASSCHECKPRII) )
 		password_check();

	r = (s32)PollDevices();
	gprintf("FAT_Init():%d\n", r );

	r = PAD_Init();
	gprintf("PAD_Init():%d\n", r );

	r = WPAD_Init();
	gprintf("WPAD_Init():%d\n", r );

	WPAD_SetPowerButtonCallback(HandleWiiMoteEvent);
	STM_RegisterEventHandler(HandleSTMEvent);

	ClearScreen();

	s8 cur_off=0;
	s8 redraw=true;
	u32 SysVersion= GetSysMenuVersion();

	if( SGetSetting(SETTING_STOPDISC) )
	{
		DVDStopDisc(false);
	}
	_sync();
#ifdef DEBUG
	gdprintf("priiloader v%d.%d DEBUG (Sys:%d)(IOS:%d)(%s %s)\n", VERSION>>8, VERSION&0xFF, SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
#else
	#if BETAVERSION > 0
		gprintf("priiloader v%d.%d BETA %d (Sys:%d)(IOS:%d)(%s %s)\n", VERSION>>8, VERSION&0xFF,BETAVERSION,SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
	#endif
#endif
	time(&startloop);
	system_state.InMainMenu = 1;
	//gprintf("ptr : 0x%08X data of ptr : 0x%08X size : %d\n",&system_state,*((u32*)&system_state),sizeof(system_state));
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
 
		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			ClearScreen();
			system_state.InMainMenu = 0;
			switch(cur_off)
			{
				case 0:
					BootMainSysMenu(1);
					if(!error)
						error=ERROR_SYSMENU_GENERAL;
					break;
				case 1:		//Load HBC
					LoadHBC();
					break;
				case 2: //Load Bootmii
					LoadBootMii();
					//well that failed...
					error=ERROR_BOOT_BOOTMII;
					break;
				case 3: // show titles list
					LoadListTitles();
					break;
				case 4:		//load main.bin from /title/00000001/00000002/data/ dir
					AutoBootDol();
					break;
				case 5:
					InstallLoadDOL();
					break;
				case 6:
					SysHackHashSettings();
					break;
				case 7:
					CheckForUpdate();
					net_deinit();
					break;
				case 8:
					InstallPassword();
					break;
				case 9:
					SetSettings();
					break;
				default:
					break;

			}
			system_state.InMainMenu = 1;
			ClearScreen();
			redraw=true;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;

			if( error == ERROR_UPDATE )
			{
				if( cur_off >= 11 )
					cur_off = 0;
			}else {

				if( cur_off >= 10 )
					cur_off = 0;
			}
			//gprintf("loool %d-%d-%d\n",*(vu32*)0xCC003000,(*(vu32*)0xCC003000)>>16,((*(vu32*)0xCC003000)>>16)&1);
			redraw=true;
		} else if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;

			if( cur_off < 0 )
			{
				if( error == ERROR_UPDATE )
				{
					cur_off=11-1;
				} else {
					cur_off=10-1;
				}
			}

			redraw=true;
		}

		if( redraw )
		{
			//if its 0, it means there is no minor version
			if( (VERSION&0xFF) % 10 == 0 )
			{
#if BETAVERSION > 0
					PrintFormat( 0, 160, rmode->viHeight-96, "Priiloader v%d.%d(beta v%d)", VERSION>>8, (VERSION&0xFF) / 10, BETAVERSION&0xFF );
#else
					PrintFormat( 0, 160, rmode->viHeight-96, "Priiloader v%d.%d (r%d)", VERSION>>8, (VERSION&0xFF) / 10 ,SVN_REV );
#endif
			}
			else
			{
#if BETAVERSION > 0
				PrintFormat( 0, 160, rmode->viHeight-96, "Priiloader v%d.%d.%d(beta v%d)", VERSION>>8, (VERSION&0xFF) / 10,(VERSION&0xFF) % 10, BETAVERSION&0xFF );
#else
				PrintFormat( 0, 160, rmode->viHeight-96, "Priiloader v%d.%d.%d (r%d)", VERSION>>8, (VERSION&0xFF) / 10,(VERSION&0xFF) % 10,SVN_REV );
#endif
			}
			PrintFormat( 0, 16, rmode->viHeight-112, "IOS v%d", (*(vu32*)0x80003140)>>16 );
			PrintFormat( 0, 16, rmode->viHeight-96, "Systemmenu v%d", SysVersion );			
			PrintFormat( 0, 16, rmode->viHeight-64, "Priiloader is a mod of Preloader 0.30");

			PrintFormat( cur_off==0, TEXT_OFFSET("System Menu"), 64, "System Menu");
			PrintFormat( cur_off==1, TEXT_OFFSET("Homebrew Channel"), 80, "Homebrew Channel");
			PrintFormat( cur_off==2, TEXT_OFFSET("BootMii IOS"), 96, "BootMii IOS");
			PrintFormat( cur_off==3, TEXT_OFFSET("Launch Title"), 112, "Launch Title");
			PrintFormat( cur_off==4, TEXT_OFFSET("Installed File"), 144, "Installed File");
			PrintFormat( cur_off==5, TEXT_OFFSET("Load/Install File"), 160, "Load/Install File");
			PrintFormat( cur_off==6, TEXT_OFFSET("System Menu Hacks"), 176, "System Menu Hacks");
			PrintFormat( cur_off==7, TEXT_OFFSET("Check For Update"),192,"Check For Update");
			PrintFormat( cur_off==8, TEXT_OFFSET("Set Password"), 208, "Set Password");
			PrintFormat( cur_off==9, TEXT_OFFSET("Settings"), 224, "Settings");

			if (error > 0)
			{
				ShowError();
				error = ERROR_NONE;
			}
			redraw = false;
		}
		if( system_state.Shutdown )
		{
			*(vu32*)0xCD8000C0 &= ~0x20;
			ClearState();
			VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
			Control_VI_Regs(0);
			DVDStopDisc(false);
			for(u8 i=0;i<WPAD_MAX_WIIMOTES;i++) {
				if(WPAD_Probe(i,0) < 0)
					continue;
				WPAD_Flush(i);
				WPAD_Disconnect(i);
			}
			WPAD_Shutdown();
			ShutdownDevices();
			USB_Deinitialize();
			if( SGetSetting(SETTING_IGNORESHUTDOWNMODE) )
			{
				STM_ShutdownToStandby();
			} 
			else 
			{
				if( CONF_GetShutdownMode() == CONF_SHUTDOWN_STANDBY )
				{
					//standby = red = off
					STM_ShutdownToStandby();
				}
				else
				{
					//idle = orange = standby
					s32 ret;
					ret = CONF_GetIdleLedMode();
					if (ret >= 0 && ret <= 2)
						STM_SetLedMode(ret);
					STM_ShutdownToIdle();
				}
			}

		}
		//boot system menu
		if(system_state.BootSysMenu)
		{
			if ( !SGetSetting(SETTING_USESYSTEMMENUIOS) )
			{
				settings->UseSystemMenuIOS = true;
			}
			ClearScreen();
			BootMainSysMenu(1);
			if(!error)
				error=ERROR_SYSMENU_GENERAL;
			system_state.BootSysMenu = 0;
			redraw=true;
		}
		//check Mounted Devices
		PollDevices();
		
		VIDEO_WaitVSync();
	}

	return 0;
}
