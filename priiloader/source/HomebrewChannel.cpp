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

//Bin include
#include "stub_bin.h"

//variables
extern u8 error;

//stub functions
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

	u16 hex[2] = { 0x00,0x00};
    switch(DetectHBC())
	{
		default: //not good, no HBC was detected >_> lets keep the stub anyway
			gprintf("HBC stub : No HBC Detected! 1.1.0 stub loaded by default\n");
			break;
		case 5:
			hex[0] = 0x4F48;
			hex[1] = 0x4243;
			break;
		case 4:
			hex[0] = 0x4C55;
			hex[1] = 0x4C5A;
			break;
		case 3:
			/*hex[0] = 0xAF1B;
			hex[1] = 0xF516;*/
			break;
		case 1: //HAXX
			hex[0] = 0x4841;//"HA";
			hex[1] = 0x5858;//"XX";
			DCFlushRange((void*)0x80001800,stub_bin_size);
			break;
		case 2: //JODI
			hex[0] = 0x4A4F; //"JO";
			hex[1] = 0x4449; //"DI";
			DCFlushRange((void*)0x80001800,stub_bin_size);
			break;
	}
	if(hex[0] != 0x00 && hex[1] != 0x00)
	{
		*(vu16*)0x80001F62 = hex[0];
		*(vu16*)0x80001F6A = hex[1];
		DCFlushRange((void*)0x80001800,stub_bin_size);
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

//HBC functions
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
		if (list[i] == 0x000100014F484243LL)
		{
			if(ret < 5)
				ret = 5;
		}
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
		case 5: //OpenHBC
			gprintf("LoadHBC : OpenHBC detected\n");
			TitleID = 0x000100014F484243LL;
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