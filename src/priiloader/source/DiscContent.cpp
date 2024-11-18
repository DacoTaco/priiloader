/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019 DacoTaco

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

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <vector>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#include "DiscContent.h"
#include "Global.h"
#include "Video.h"
#include "state.h"
#include "font.h"
#include "error.h"
#include "Input.h"
#include "mount.h"
#include "IOS.h"
#include "dvd.h"
#include "gecko.h"
#include "mem2_manager.h"
#include "titles.h"

extern "C"
{
	extern void __exception_closeall();
}

void ApploaderInitCallback(const char* fmt, ...)
{
	if (fmt != NULL)
		gdprintf("StubApploaderInitCallback : %s", fmt);
}

void LaunchGamecubeDisc(void)
{
	u32 gameID = *(u32*)0x80000000;
	gprintf("GameID 0x%08X", gameID);

	//gamecube games MUST have this called after reading the ID
	//if any command is done before this, this will error out.
	//see https://wiibrew.org/wiki//dev/di#0xE4_DVDLowAudioBufferConfig
	s32 ret = DVDAudioBufferConfig(*(u8*)0x80000008, *(u8*)0x80000009);
	if (ret < 0)
		throw "Failed to set audio streaming config (" + std::to_string(ret) + ")";

	//Read BI2 from disk. this has no real purpose on the wii, but it sets the DVD drive in a ready state
	//System Menu does this to be sure the DVD is in the correct state for the game to use.
	char bi2[0x2000] [[gnu::aligned(32)]];
	ret = DVDUnencryptedRead(0x440, bi2, sizeof(bi2));
	if (ret < 0)
		throw "Failed to read BI2 (" + std::to_string(ret) + ")";
	DVDCloseHandle();

	s8 oldVideoMode = SYS_GetVideoMode();
	s8 videoMode = SetVideoModeForTitle(gameID);
	gprintf("video mode : 0x%02X -> 0x%02X", oldVideoMode, videoMode);
	if (oldVideoMode != videoMode)
		SYS_SetVideoMode(videoMode); // most gc games tested require this
	*(vu32*)0x800000CC = videoMode > SYS_VIDEO_NTSC ? 0x00000001 : 0x00000000;
	DCFlushRange((void*)0x80000000, 0x3200);

	//set bootstate for when we come back & set reset state for gc mode
	SetBootState(TYPE_UNKNOWN, FLAGS_STARTGCGAME, RETURN_TO_MENU, DISCSTATE_GC);
	*(u32*)0xcc003024 |= 7;

	gprintf("booting BC...");
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	ret = WII_LaunchTitle(BC_Title_Id);
	throw "launching BC failed(" + std::to_string(ret) + ")";
}

void LaunchWiiDisc(void)
{
	DVDTableOfContent* tableOfContent = NULL;
	DVDPartitionInfo* partitionsInfo = NULL;
	u8* tmd_buf = NULL;
	u8 partitionOpened = 0;
	u32 gameID = 0;
	s32 ret = 0;
	apploader_entry app_entry = NULL;
	apploader_init app_init = NULL;
	apploader_main app_main = NULL;
	apploader_final app_final = NULL;
	dvd_main dvd_entry [[gnu::noreturn]] = NULL;

	//get game ID
	gameID = *(u32*)0x80000000;
	gprintf("GameID 0x%08X", gameID);

	//Read game name. offset 0x20 - 0x400 
	char gameName[0x80] [[gnu::aligned(32)]];
	memset(gameName, 0, sizeof(gameName));
	ret = DVDUnencryptedRead(0x20, gameName, 0x40);
	if (ret <= 0)
		throw "Failed to Read Game name (" + std::to_string(ret) + ")";
	PrintFormat(1, TEXT_OFFSET(gameName), 224, gameName);
	gprintf("loading %s...", gameName);

	tableOfContent = (DVDTableOfContent*)mem_align(32, 0x20);
	partitionsInfo = (DVDPartitionInfo*)mem_align(32, 0x20);
	//tmd has a max size of 0x49E4(MAX_SIGNED_TMD_SIZE) + align up for extra space
	u32 tmdsize = ((MAX_SIGNED_TMD_SIZE + 0x100 - 1) & ~(0x100 - 1));
	tmd_buf = (u8*)mem_align(32, tmdsize);

	if (tableOfContent == NULL || partitionsInfo == NULL || tmd_buf == NULL)
		throw "Failed to allocate memory";

	memset(tableOfContent, 0, 0x20);
	memset(partitionsInfo, 0, 0x20);
	memset(tmd_buf, 0, tmdsize);

	ret = DVDUnencryptedRead(WIIDVD_TOC_OFFSET, tableOfContent, 0x20);
	if (ret <= 0)
		throw "Failed to Read TableOfContent (" + std::to_string(ret) + ")";

	//shift offset by 2 bits to compensate for the bits needed for the UnencryptedRead
	ret = DVDUnencryptedRead(tableOfContent->partitionInfoOffset << 2, partitionsInfo, 0x20);
	if (ret <= 0)
		throw "Failed to Read Partition Info (" + std::to_string(ret) + ")";

	DVDPartitionInfo* bootGameInfo = NULL;
	for (u32 index = 0; index < tableOfContent->bootInfoCount; index++)
	{
		if (partitionsInfo[index].len != 0)
			continue;

		bootGameInfo = &(partitionsInfo[index]);
		break;
	}

	if (bootGameInfo == NULL)
		throw "could not find game info partition";

	//attempt to patch IOS to keep ahbprot when loading a TMD, because opening the partition kinda resets it
	PatchIOS({ AhbProtPatcher });
	signed_blob* mTMD = (signed_blob*)tmd_buf;
	ret = DVDOpenPartition(bootGameInfo->offset, NULL, NULL, 0, mTMD);
	if (ret <= 0)
		throw "Failed to open Partition (" + std::to_string(ret) + ")";

	partitionOpened = 1;
	tmd* rTMD = (tmd*)SIGNATURE_PAYLOAD(mTMD);
	u8 requiredIOS = rTMD->sys_version & 0xFF;
	gprintf("game title: 0x%16llX", rTMD->title_id);
	gprintf("ios : %d", requiredIOS);

	memset(rTMD, 0, 0x190);

	//reloading IOS not right yet, DI stuff fail afterwards :/
	if (IOS_GetVersion() != requiredIOS)
	{
		gprintf("reloading ios");

		ret = DVDClosePartition();
		partitionOpened = 0;
		if (ret <= 0)
			throw "Failed to close Partition (" + std::to_string(ret) + ")";

		DVDCloseHandle();

		//our AHBPROT patch from before should make us keep access
		ret = ReloadIOS(requiredIOS, 0);
		if (ret <= 0)
			throw "Failed to reload IOS (" + std::to_string(ret) + ")";

		//attempt to patch IOS to keep ahbprot when loading a TMD, because opening the partition kinda resets it
		PatchIOS({ AhbProtPatcher });

		ret = DVDInit();
		if (ret <= 0)
			throw "Failed to init dvd drive (" + std::to_string(ret) + ")";

		//this is required. many commands don't work before having called ReadID (like opening the partition)
		ret = DVDReadGameID((u8*)0x80000000, 0x20);
		if (ret <= 0)
			throw "Failed to re-read Game info (" + std::to_string(ret) + ")";

		ret = DVDOpenPartition(bootGameInfo->offset, NULL, NULL, 0, mTMD);
		if (ret <= 0)
			throw "Failed to open Partition (" + std::to_string(ret) + ")";
		partitionOpened = 1;
	}

	ret = DVDRead(0x00, (void*)0x80000000, 0x20);
	if (ret <= 0)
		throw "Failed to read partition header(" + std::to_string(ret) + ")";

	apploader_hdr appldr_header [[gnu::aligned(32)]];
	memset(&appldr_header, 0, sizeof(apploader_hdr));
	ret = DVDRead(APPLOADER_HDR_OFFSET, &appldr_header, sizeof(apploader_hdr));
	if (ret <= 0)
		throw "Failed to read apploader header(" + std::to_string(ret) + ")";

	DCFlushRange(&appldr_header, sizeof(apploader_hdr));

	//Continue reading the apploader now that we have the header
	//TODO (?) : replace this address with a mem2 address if possible?
	ret = DVDRead(APPLOADER_HDR_OFFSET + sizeof(apploader_hdr), (void*)0x81200000, appldr_header.size + appldr_header.trailersize);
	if (ret <= 0)
		throw "Failed to read start of apploader (" + std::to_string(ret) + ")";
	DCFlushRange((void*)0x81200000, appldr_header.size + appldr_header.trailersize);
	ICInvalidateRange((void*)0x81200000, appldr_header.size + appldr_header.trailersize);

	ICSync();
	app_entry = (apploader_entry)appldr_header.entry;
	app_entry(&app_init, &app_main, &app_final);
	app_init(ApploaderInitCallback);

	void* data;
	int size;
	int offset;
	while (app_main(&data, &size, &offset) == 1)
	{
		gprintf("reading 0x%08X bytes from 0x%08X to 0x%08X", size, offset, data);
		ret = DVDRead(offset << 2, data, size);
		if (ret < 0)
			throw "Failed to read apploader data @ offset " + std::to_string(offset) + "(" + std::to_string(ret) + ")";
		DCFlushRange(data, size);
	}

	dvd_entry = (dvd_main)app_final();

	if (dvd_entry == NULL || 0x80003000 > (u32)(dvd_entry))
		throw "failed to get entrypoint";

	DVDCloseHandle();
	//i don't know why this is done. all loaders do it, but dolphin doesnt. 
	//what is even the purpose of this?
	settime(secs_to_ticks(time(NULL) - 946684800));

	s8 videoMode = SetVideoModeForTitle(gameID);

	//disc related pokes to finish it off
	//see memory map @ https://wiibrew.org/w/index.php?title=Memory_Map
	//and @ https://www.gc-forever.com/yagcd/chap4.html#sec4
	*(vu32*)0x80000020 = 0x0D15EA5E;				// Boot from DVD
	*(vu32*)0x80000024 = 0x00000001;				// Version
	*(vu32*)0x80000028 = 0x01800000;				// Memory Size (Physical) 24MB
	*(vu32*)0x8000002C = 0x00000023;				// Production Board Model
	*(vu32*)0x800000CC = videoMode > SYS_VIDEO_NTSC ? 0x00000001 : 0x00000000;	// Video Mode
	*(vu32*)0x800000E4 = 0x8008f7b8;				// Thread Pointer
	*(vu32*)0x800000F0 = 0x01800000;				// Dev Debugger Monitor Address
	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed
	*(vu32*)0x800030C0 = 0x00000000;				// EXI
	*(vu32*)0x800030C4 = 0x00000000;				// EXI
	*(vu32*)0x800030DC = 0x00000000;				// Time
	*(vu32*)0x800030D8 = 0XFFFFFFFF;				// Time
	*(vu32*)0x800030E0 = 0x00000000;				// PADInit
	*(vu32*)0x800030E4 = 0x00008201;				// Console type
	*(vu32*)0x800030F0 = 0x00000000;				// Apploader parameters
	*(vu32*)0x8000315C = 0x80800113;				// DI Legacy mode ? OSInit/apploader?
	*(vu32*)0x80003180 = *(vu32*)0x80000000;		// Enable WC24 by having the game id's the same
	*(vu32*)0x80003184 = 0x80000000;				// Application Type, 0x80 = Disc, 0x81 = NAND
	*(vu32*)0x8000318C = 0x00000000;				// Title Booted from NAND
	*(vu32*)0x80003190 = 0x00000000;				// Title Booted from NAND

	//Extra pokes that could have a use, but we can do without
	//They aren't found in dolphin, nor GeckoOS so.... meh
	/**(vu32*)0x80000038 = 0x817FEC60;				// FST Start - get from DVD
	*(vu32*)0x80000060 = 0x38A00040;				// Debugger hook
	*(vu32*)0x80003100 = 0x01800000;				// BAT
	*(vu32*)0x80003104 = 0x01800000;				// BAT
	*(vu32*)0x8000310C = 0x00000000;				// Init
	*(vu32*)0x80003110 = 0x00000000;				// Init
	*(vu32*)0x80003118 = 0x04000000;				//
	*(vu32*)0x8000311C = 0x04000000;				// BAT
	*(vu32*)0x80003120 = 0x93400000;				// BAT
	*(vu32*)0x80003138 = 0x00000011;				// Console type*/

	DCFlushRange((void*)0x80000000, 0x3200);

	gprintf("booting binary (0x%08X)...", dvd_entry);
	u32 level;
	__IOS_ShutdownSubsystems();
	ISFS_Deinitialize();
	__exception_closeall();
	_CPU_ISR_Disable(level);
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	ICSync();
	dvd_entry();

	_CPU_ISR_Restore(level);
	gprintf("Failed");

	//cleanup everything
	if (partitionOpened)
		DVDClosePartition();

	DVDStopDriveAsync();

	mem_free(tableOfContent);
	mem_free(partitionsInfo);
	mem_free(tmd_buf);

	while (DVDAsyncBusy());
	DVDCloseHandle();
}

void BootDiscContent(void)
{
	ClearScreen();
	PrintFormat(1, TEXT_OFFSET("Loading DVD..."), 208, "Loading DVD...");
	gprintf("reading dvd...");

	try
	{
		DVDInit();
		s32 ret = DVDResetDrive();
		if (ret <= 0)
			throw "Failed to Reset Drive (" + std::to_string(ret) + ")";

		memset((char*)0x80000000, 0, 0x20);
		ICInvalidateRange((u32*)0x80000000, 0x20);
		DCFlushRange((u32*)0x80000000, 0x20);

		ret = DVDInquiry();
		if (ret <= 0)
			throw "Failed to Identify (" + std::to_string(ret) + ")";

		//Read Game ID. required to do certain commands, also needed to check disc type.
		ret = DVDReadGameID((u8*)0x80000000, 0x20);
		if (ret <= 0)
			throw "Failed to Read Game info (" + std::to_string(ret) + ")";

		ICInvalidateRange((u32*)0x80000000, 0x20);
		DCFlushRange((u32*)0x80000000, 0x20);

		//verify disc type
		if (*((u32*)0x8000001C) == GCDVD_MAGIC_VALUE)
		{
			gprintf("GC disc detected");
			LaunchGamecubeDisc();
		}

		if (*((u32*)0x80000018) != WIIDVD_MAGIC_VALUE)
			throw "Unknown Disc inserted.";

		LaunchWiiDisc();
	}
	catch (const std::string& ex)
	{
		gprintf("BootDvdDrive Exception -> %s", ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("BootDvdDrive Exception -> %s", ex);
	}
	catch (...)
	{
		gprintf("BootDvdDrive Exception was thrown");
	}

	return;
}