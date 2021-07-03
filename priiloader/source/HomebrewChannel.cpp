//compiler includes
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>

#include <gccore.h>
#include <wiiuse/wpad.h>

//Project includes
#include "Global.h"
#include "mem2_manager.h"
#include "gecko.h"
#include "error.h"
#include "state.h"
#include "IOS.h"
#include "HomebrewChannel.h"
#include "font.h"
#include "Input.h"
#include "mount.h"

//Bin include
#include "stub_bin.h"

//The known HBC titles

const title_info HBC_Titles[] = {
	{ 0x0001000148415858LL, "HAXX" },
	{ 0x000100014A4F4449LL, "JODI" },
	{ 0x00010001AF1BF516LL, "0.7 - 1.1" },
	{ 0x000100014C554C5ALL, "LULZ" },
	{ 0x000100014F484243LL, "OpenHBC 1.4"}
};

//array size retrieval
#define HBC_Titles_Size (s32)((sizeof(HBC_Titles) / sizeof(HBC_Titles[0])))

//stub functions
void LoadHBCStub ( void )
{
	//LoadHBCStub: Load HBC 0.10 reload Stub and change stub to any TitleID if needed. 
	//the first part of the title is at 0x800024CA (first 2 bytes) and 0x800024D2 (last 2 bytes)

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

	u16 hex[2] = { 0x00,0x00};
	title_info title = { 0x00,"None"};
	s32 ret = DetectHBC(&title);

	if(ret >= 0 && title.title_id != 0x00)
	{
		hex[0] = (u16)((TITLE_LOWER(title.title_id) & 0xFFFF0000) >> 16);
		hex[1] = (u16)(TITLE_LOWER(title.title_id) & 0x0000FFFF);
	}
	else
	{
		gprintf("HBC stub : No HBC Detected! 1.1.0 stub loaded by default");
	}

	if(
		(hex[0] != 0x00 && hex[1] != 0x00) &&
		(hex[0] != 0xAF1B && hex[1] != 0xF516)) //these are the default values in the loaded stub
	{
		*(vu16*)0x80001F62 = hex[0];
		*(vu16*)0x80001F6A = hex[1];
		DCFlushRange((void*)0x80001800,stub_bin_size);
	}
	gprintf("HBC stub : Loaded");
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

//HBC functions
s32 DetectHBC(title_info* title)
{
	u64 *list;
    u32 titlecount;
    s32 ret;

	if(HBC_Titles_Size <= 0)
	{
		title = NULL;
		return -1;
	}

    ret = ES_GetNumTitles(&titlecount);
    if(ret < 0)
	{
		gprintf("DetectHBC : ES_GetNumTitles failure");
		title = NULL;
		return -1;
	}

    list = (u64*)mem_align(32, ALIGN32( titlecount * sizeof(u64) ) );

    ret = ES_GetTitles(list, titlecount);
    if(ret < 0) {
		gprintf("DetectHBC :ES_GetTitles failure. error %d",ret);
		mem_free(list);
		title = NULL;
		return -2;
    }
	ret = -3;
	//lets check for all known HBC title id's.
	for(u32 i=0; i<titlecount; i++) 
	{
		for(s32 arrayIndex = (ret < 0)?0:ret; arrayIndex < HBC_Titles_Size;arrayIndex++)
		{
			if(ret < arrayIndex && list[i] == HBC_Titles[arrayIndex].title_id)
			{
				gdprintf("Detected %s",HBC_Titles[arrayIndex].name_ascii.c_str());
				ret = arrayIndex;
			}
		}

		//we have found the latest, supported HBC. no need to proceed checking
		if(ret >= HBC_Titles_Size-1)
		{
			gprintf("latest HBC detected. ret = %d",ret);
			break;
		}
	}
	mem_free(list);
    if(ret < 0 || ret > HBC_Titles_Size -1)
	{
		gprintf("DetectHBC: HBC not found");
		title = NULL;
		return -3;
	}
	*title = HBC_Titles[ret];
	return ret;
}

void LoadHBC( void )
{
	u64 TitleID = 0;

	title_info title = { 0x00,"None"};
	s32 ret = DetectHBC(&title);

	if(ret >= 0 && title.title_id != 0x00)
	{
		gprintf("LoadHBC : %s detected",title.name_ascii.c_str());
		TitleID = title.title_id;
	}
	else
	{
		error = ERROR_BOOT_HBC;
		return;
	}
	
	u32 cnt ATTRIBUTE_ALIGN(32);
	ES_GetNumTicketViews(TitleID, &cnt);
	tikview *views = (tikview *)mem_align( 32, sizeof(tikview)*cnt );
	ES_GetTicketViews(TitleID, views, cnt);
	ClearState();
	Input_Shutdown();
	gprintf("starting HBC");
	ES_LaunchTitle(TitleID, &views[0]);
	//well that went wrong
	Input_Init();
	error = ERROR_BOOT_HBC;
	mem_free(views);
	return;
}
void LoadBootMii( void )
{
	//when this was coded on 6th of Oct 2009 Bootmii ios was in IOS slot 254
	if(isIOSstub(254))
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Bootmii(IOS254) Not found!"), 208, "Bootmii(IOS254) Not found!");
			sleep(5);
		}
		return;
	}
	if ( !HAS_SD_FLAG(GetMountedFlags()) )
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Could not mount SD card"), 208, "Could not mount SD card");
			sleep(5);
		}
		return;
	}
	FILE* BootmiiFile = fopen(BuildPath("/bootmii/armboot.bin", MountDevice::Device_SD).c_str(),"r");
	if (!BootmiiFile)
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Could not find sd:/bootmii/armboot.bin"), 208, "Could not find sd:/bootmii/armboot.bin");
			sleep(5);
		}
		return;
	}
	fclose(BootmiiFile);
		
	/*BootmiiFile = fopen(BuildPath("/bootmii/ppcboot.elf", MountDevice::Device_SD).c_str(),"r");
	if(!BootmiiFile)
	{
		if(rmode != NULL)
		{	
			PrintFormat( 1, TEXT_OFFSET("Could not find sd:/bootmii/ppcboot.elf"), 208, "Could not find sd:/bootmii/ppcboot.elf");
			sleep(5);
		}
		return;
	}
	fclose(BootmiiFile);*/
	u8 currentIOS = IOS_GetVersion();
	Input_Shutdown();
	IOS_ReloadIOS(254);
	//launching bootmii failed. lets wait a bit for the launch(it could be delayed) and then load the other ios back
	sleep(3);
	IOS_ReloadIOS(currentIOS);
	system_state.ReloadedIOS = 1;
	Input_Init();
	return;
}