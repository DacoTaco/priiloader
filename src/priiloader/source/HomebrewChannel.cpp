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
#include "Video.h"
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

//HBC functions
std::shared_ptr<TitleDescription> DetectHBC()
{
	if(HBCTitles.size() <= 0)
		return NULL;
	
	std::shared_ptr<TitleDescription> titleDescription = NULL;
	for(u32 i = 0; i < HBCTitles.size(); i++)
	{
		auto info = std::make_unique<TitleInformation>(HBCTitles[i]->TitleId);
		if(!info->IsInstalled() || info->IsMovedToSD())
			continue;
		
		titleDescription = HBCTitles[i];
	}

	return titleDescription;
}

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
	auto titleDescription = DetectHBC();

	if(titleDescription != NULL)
	{
		u32 lowerTitleId = TITLE_LOWER(titleDescription->TitleId);
		hex[0] = (u16)((lowerTitleId & 0xFFFF0000) >> 16);
		hex[1] = (u16)(lowerTitleId & 0x0000FFFF);
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

void LoadHBC( void )
{
	auto titleDescription = DetectHBC();
	if(titleDescription == NULL)
	{
		error = ERROR_BOOT_HBC;
		return;
	}
	
	try
	{
		gprintf("LoadHBC : %s detected", titleDescription->Name.c_str());
		auto titleInfo = std::make_unique<TitleInformation>(titleDescription->TitleId);
		gprintf("LoadHBC : starting HBC...");
		titleInfo->LaunchTitle();
	}
	catch(...)
	{
		error = ERROR_BOOT_HBC;
	}

	return;
}

void LoadBootMii( void )
{
	//when this was coded on 6th of Oct 2009 Bootmii ios was in IOS slot 254
	if(IsIOSstub(254))
	{
		if(rmode != NULL)
		{
			PrintFormat( 1, TEXT_OFFSET("Bootmii(IOS254) Not found!"), 208, "Bootmii(IOS254) Not found!");
			sleep(5);
		}
		return;
	}

	//only check files if mounted. this allows autoboot to work, which hasn't mounted anything yet
	if(GetMountedFlags() > 0)
	{
		if ( !HAS_SD_FLAG(GetMountedFlags()) )
		{
			PrintFormat( 1, TEXT_OFFSET("Could not mount SD card"), 208, "Could not mount SD card");
			sleep(5);
			return;
		}

		FILE* BootmiiFile = fopen(BuildPath("/bootmii/armboot.bin", StorageDevice::SD).c_str(),"r");
		if (!BootmiiFile)
		{
			gprintf("LoadBootMii: /bootmii/armboot.bin not found");
			PrintFormat( 1, TEXT_OFFSET("Could not find sd:/bootmii/armboot.bin"), 208, "Could not find sd:/bootmii/armboot.bin");
			sleep(5);
			return;
		}
		fclose(BootmiiFile);
			
		/*BootmiiFile = fopen(BuildPath("/bootmii/ppcboot.elf", StorageDevice::Device_SD).c_str(),"r");
		if(!BootmiiFile)
		{
			PrintFormat( 1, TEXT_OFFSET("Could not find sd:/bootmii/ppcboot.elf"), 208, "Could not find sd:/bootmii/ppcboot.elf");
			sleep(5);
			return;
		}
		fclose(BootmiiFile);*/
	}
	
	u8 currentIOS = IOS_GetVersion();
	Input_Shutdown();
	ShutdownMounts();
	IOS_ReloadIOS(254);
	//launching bootmii failed. lets wait a bit for the launch(it could be delayed) and then load the other ios back
	sleep(3);
	IOS_ReloadIOS(currentIOS);
	system_state.ReloadedIOS = 1;
	InitMounts();
	Input_Init();
	return;
}