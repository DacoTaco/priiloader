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
#include <iomanip>
#include <algorithm>
#include <ogc/machine/processor.h>
#include <ogc/conf.h>

#include "titles.hpp"
#include "Video.h"
#include "font.h"
#include "settings.h"
#include "playlog.h"
#include "state.h"
#include "mem2_manager.h"
#include "dvd.h"
#include "Input.h"
#include "gecko.h"
#include "mount.h"

//The known HBC titles
const std::vector<std::shared_ptr<TitleDescription>> HBCTitles = {
	std::make_shared<TitleDescription>(0x0001000148415858LL, "HAXX"),
	std::make_shared<TitleDescription>(0x000100014A4F4449LL, "JODI"),
	std::make_shared<TitleDescription>(0x00010001AF1BF516LL, "0.7 - 1.1"),
	std::make_shared<TitleDescription>(0x000100014C554C5ALL, "LULZ"),
	std::make_shared<TitleDescription>(0x000100014F484243LL, "OpenHBC 1.4"),
};

#define UNKNOWN_TITLE_NAME		"????????"
#define UNKNOWN_TITLE_NAME_SD	"????[SD]"

#define HW_VI1CFG 0x0d800018

bool VideoRegionMatches(TitleRegion region)
{
	switch (VI_TVMODE_FMT(rmode->viTVMode))
	{
		case VI_NTSC:
		case VI_DEBUG:
			return (region == TitleRegion::NTSC || region == TitleRegion::NTSC_J);
		case VI_PAL:
		case VI_MPAL:
		case VI_DEBUG_PAL:
		case VI_EURGB60:
			return (region == TitleRegion::PAL);
		default:
			return true;
	}
}

void SetVideoInterfaceConfig(std::shared_ptr<TitleInformation> title)
{
	bool isJpRegion = title == NULL
		? CONF_GetRegion() == CONF_REGION_JP
		: title->GetTitleRegion() == TitleRegion::NTSC_J;

	//if the region is JP, we will set the bit, otherwise clear it
	s32 vi1cfg = read32(HW_VI1CFG);
	if (isJpRegion)
		vi1cfg |= (1 << 17); //set the bit
	else
		vi1cfg &= ~(1 << 17); //clear the bit

	write32(HW_VI1CFG, vi1cfg);
}

s8 SetVideoModeForTitle(std::shared_ptr<TitleInformation> title)
{
	//always set video when launching disc
	TitleRegion titleRegion = title->GetTitleRegion();
	bool confProg = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	bool confPAL60 = CONF_GetEuRGB60() > 0;
	GXRModeObj* rmodeNew = rmode;
	s8 videoMode = 0;
	switch (titleRegion)
	{
		case TitleRegion::PAL:
			if (confProg) {          // 480p60
				rmodeNew = &TVEurgb60Hz480Prog;
				gprintf("PAL60 480p");
			} else if (confPAL60) {  // 480i60
				rmodeNew = &TVEurgb60Hz480IntDf;
				gprintf("PAL60 480i");
			} else {                 // 576i50 (incompatible with S-Video cables!)
				rmodeNew = &TVPal528IntDf;
				gprintf("PAL50 576i");
			}
			videoMode = SYS_VIDEO_PAL;
			break;

		case TitleRegion::NTSC_J:
			gprintf("NTSC-J");
			SetVideoInterfaceConfig(title);
			goto region_ntsc;
			break;

		case TitleRegion::NTSC:
			gprintf("NTSC-U");
		region_ntsc:
			if (confProg) {          // 480p60
				rmodeNew = &TVNtsc480Prog;
				gprintf("NTSC 480p");
			} else {                 // 480i60
				rmodeNew = &TVNtsc480IntDf;
				gprintf("NTSC 480i");
			}
			videoMode = SYS_VIDEO_NTSC;
			break;
		default:
			gprintf("unknown titleRegion");
			break;
	}

	//set video mode for the game
	if (rmode != rmodeNew && !VideoRegionMatches(titleRegion))
		ConfigureVideoMode(rmodeNew);

	return videoMode;
}

std::string GetTitleLongString(u64 titleId)
{
	const s32 MAX_TITLE_STRING = 19;
	std::string titlestring;
    titlestring.resize(MAX_TITLE_STRING);

    snprintf(&titlestring[0], MAX_TITLE_STRING,"%08X\\%08X",TITLE_UPPER(titleId),TITLE_LOWER(titleId));
    
    return titlestring; 
}

TitleInformation::~TitleInformation()
{
	if(_titleTMD != NULL)
		mem_free(_titleTMD);
}

TitleInformation::TitleInformation(u64 titleId, std::string name)
{
	_titleId = titleId;

	_titleName = {
		.Name = name
	};
}

TitleInformation::TitleInformation(u64 titleId)
{
	_titleId = titleId;

	//if the title id isn't an essential title, we will check if its following the wii/nintendo rules
	//the only exception is the HBC titles, as there was a version that said fuck it lol
	if(TITLE_UPPER(titleId) == TITLE_TYPE_ESSENTIAL)
		return;
	
	auto iterator = std::find_if(HBCTitles.begin(), HBCTitles.end(), [titleId](const std::shared_ptr<TitleDescription>& hbcTitle)
	{
		return hbcTitle->TitleId == titleId;
	});

	if(iterator != HBCTitles.end())
		return;

	u32 lowerTitleId = TITLE_LOWER(titleId);
	for(s8 i = 0; i < 4; i++)
	{
		s8 byte = lowerTitleId & 0xFF;
		//does it have a non-printable character?
		if(byte < 0x20 || byte > 0x7E)
			throw "Invalid TitleId " + std::to_string(TITLE_UPPER(titleId)) + std::to_string(TITLE_LOWER(titleId)) + ", found " + std::to_string(byte);

		lowerTitleId >>= 8;
	}
}

u64 TitleInformation::GetTitleId(){ return _titleId; }

std::string TitleInformation::GetTitleIdString()
{
	//convert the titleId to string, converting non-printable characters to dots
	char titleStr[5] = {0};
	u32 titleId = TITLE_LOWER(_titleId);

	for(s8 i = 0; i < 4; i++)
	{
		char titleCharacter = titleId >> 24;
		if(titleCharacter < 0x20 || titleCharacter > 0x7E)
			titleStr[i] = '.';
		else
			titleStr[i] = titleCharacter;
		
		titleId = titleId << 8;
	}

	return std::string(titleStr, 4);
}

bool TitleInformation::IsInstalled() 
{ 	
	/*
		steps to check if its installed, based on what SM was doing: 
			* Get TMD
			* Get Content Count
			* if content count matches -> installed
			* if content count == 1 -> installed but moved to SD

		if any error happened we will assume it is installed just in case, unless we got a not found error
	*/
	auto isInstalled = true;
	try
	{
		auto tmd = GetTMD();
		if(tmd == NULL)
			return false;
		
		STACK_ALIGN(u32, contentCount, 1, 32);
		*contentCount = 0;
		auto ret = ES_GetTitleContentsCount(_titleId, contentCount);
		switch(ret)
		{
			case -106:
			{
				isInstalled = false;
				break;
			}
			default:
			{
				if(ret < 0)
					throw "Failed to retrieve content count, error " + std::to_string(ret);
				
				isInstalled = *contentCount == 1 || *contentCount == tmd->num_contents;
				break;
			}
		}

		if(!isInstalled)
			gdprintf("IsInstalled: %s not installed", GetTitleLongString(_titleId).c_str());
	}
	catch (const std::string& ex)
	{
		gprintf("IsInstalled: %s", ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("IsInstalled: %s", ex);
	}
	catch(...)
	{
		gprintf("IsInstalled: Unknown Error Occurred");
	}

	return isInstalled;
}

bool TitleInformation::IsMovedToSD()
{
	if (!HAS_SD_FLAG(GetMountedFlags())) //no SD mounted, lets bail out
	{
		gdprintf("IsMovedToSD : no SD card inserted");
		return false;
	}

	std::string path = "/private/wii/title/" + GetTitleIdString() + "/content.bin";
	std::string filepath = BuildPath(path.c_str(), StorageDevice::SD);
	FILE* SDHandler = fopen(filepath.c_str(),"rb");
	if (SDHandler)
	{
		//content.bin is there meaning its on SD
		fclose(SDHandler);
		gprintf("IsMovedToSD : %s is saved on SD", GetTitleLongString(_titleId).c_str());
		return true;
	}

	//title isn't on SD
	gprintf("IsMovedToSD : %s not found on SD", GetTitleLongString(_titleId).c_str());
	return false;
}

tmd* TitleInformation::GetTMD()
{
	//return already fetched title TMD if we retrieved it
	if(_titleTMD != NULL)
		return (tmd*)SIGNATURE_PAYLOAD(_titleTMD);
	
	s32 ret;
	u32 blobSize = 0;
	signed_blob* blob = NULL;
	s32 fd = -1;

	try
	{
		//IOS versions, starting with 28 have ES calls to retrieve the TMD. lets use those
		if (IOS_GetVersion() >= 28)
		{
			ret = ES_GetStoredTMDSize(_titleId, &blobSize);
			if(ret == -106)
			{
				gdprintf("GetTMD: title %s TMD not found", GetTitleLongString(_titleId));
				return NULL;
			}
			
			if (ret < 0 || blobSize > MAX_SIGNED_TMD_SIZE)
				throw "GetTMD: failed to retrieve TMD Size via IOS of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);

			blob = static_cast<signed_blob*>(mem_align(32, MAX_SIGNED_TMD_SIZE));
			if (blob == NULL)
				throw "GetTMD: failed to allocate TMD of " + GetTitleLongString(_titleId);

			memset(blob, 0, MAX_SIGNED_TMD_SIZE);
			ret = ES_GetStoredTMD(_titleId, blob, blobSize);
			if (ret < 0)
				throw "GetTMD: failed to retrieve TMD via IOS of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);
		}
		else
		{
			//Other IOS calls would need direct NAND Access to load the TMD.
			//so here we go
			gprintf("GetTMD : Load TMD from nand");
			STACK_ALIGN(fstats, TmdStats, 1, 32);
			char TmdPath[ISFS_MAXPATH];
			memset(TmdPath, 0, 64);
			sprintf(TmdPath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(_titleId), TITLE_LOWER(_titleId));
			fd = ISFS_Open(TmdPath, ISFS_OPEN_READ);
			if (fd < 0)
				throw "GetTMD: failed to open TMD of " + GetTitleLongString(_titleId) + ",error " + std::to_string(fd);

			ret = ISFS_GetFileStats(fd, TmdStats);
			if (ret < 0)
				throw "GetTMD: failed to file info of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);

			blob = static_cast<signed_blob*>(mem_align(32, MAX_SIGNED_TMD_SIZE));
			if (blob == NULL)
				throw "GetTMD: failed to allocate TMD of " + GetTitleLongString(_titleId);

			memset(blob, 0, MAX_SIGNED_TMD_SIZE);
			ret = ISFS_Read(fd, blob, TmdStats->file_length);
			if (ret < 0)
				throw "GetTMD: failed read TMD of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);

			ISFS_Close(fd);
		}

		_titleTMD = blob;
		return (tmd*)SIGNATURE_PAYLOAD(_titleTMD);
	}
	catch(...)
	{
		if(blob)
			mem_free(blob);

		if(fd >= 0)
			ISFS_Close(fd);
		
		throw;
	}
}

signed_blob* TitleInformation::GetRawTMD()
{
	//return already fetched title TMD if we retrieved it
	if(_titleTMD != NULL)
		return _titleTMD;

	this->GetTMD();
	return _titleTMD;
}

TitleRegion TitleInformation::GetTitleRegion()
{
	//Taken from Dolphin
	switch (TITLE_LOWER(_titleId) & 0xFF)
	{
		//PAL
		case 'D':
		case 'F':
		case 'H':
		case 'I':
		case 'L':
		case 'M':
		case 'P':
		case 'R':
		case 'S':
		case 'U':
		case 'V':
			return TitleRegion::PAL;

			//NTSC-J
		case 'J':
		case 'K':
		case 'Q':
		case 'T':
			return TitleRegion::NTSC_J;

			//NTSC-U
		default:
			gdprintf("unknown Region");
		case 'B':
		case 'N':
		case 'E':
			return TitleRegion::NTSC;
	}
}

TitleName TitleInformation::GetTitleName()
{
	if(_titleName.Name[0] != '\0')
		return _titleName;
	
	void* openingBanner = NULL;
	tikview *ticketViews = NULL;
	try
	{
		//we allocate the NandIMET size since it is the bigger of the 2 versions
	openingBanner = static_cast<void*>(mem_align(32, ALIGN32( sizeof(NandOpeningBanner) ) ));
		if(openingBanner == NULL)
			throw "failed to alloc IMET header of title" + GetTitleLongString(_titleId);
		
		memset(openingBanner, 0, sizeof(NandOpeningBanner));
		u32 cnt ATTRIBUTE_ALIGN(32) = 0;
		s32 ret = ES_GetNumTicketViews(_titleId, &cnt);
		if(ret < 0)
			throw "failed to retrieve Tickets Size of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);

	ticketViews = static_cast<tikview *>(mem_align( 32, sizeof(tikview)*cnt ));
		if(ticketViews == NULL)
			throw "failed to allocate ticket views of " + GetTitleLongString(_titleId);

		ret = ES_GetTicketViews(_titleId, ticketViews, cnt);
		if(ret < 0)
			throw "failed to retrieve Ticket views of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);

		s32 fh = ES_OpenTitleContent(_titleId, ticketViews, 0);
		//if its not there, we will look on the SD and throw exception.
		//maybe its moved to SD, idk.
		if(fh == -106)
		{
			if(this->IsMovedToSD())
			{
				_titleName = {
					.Name = UNKNOWN_TITLE_NAME_SD,
					.UnicodeName = {0}
				};

				mem_free(openingBanner);
				return _titleName;				
			}
			throw "failed to retrieve title content of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);
		}

		//no longer need the ticket views
		mem_free(ticketViews);

		if (fh >= 0)
		{
			//ES method
			ret = ES_ReadContent(fh, static_cast<u8*>(openingBanner), sizeof(NandOpeningBanner));
			ES_CloseContent(fh);
			if (ret < 0) 
				throw "failed to read(ES) title content of " + GetTitleLongString(_titleId) + ",error " + std::to_string(ret);
		}
		else
		{
			//ES method failed, lets fallback to direct access
			gprintf("GetTitleName: ES_OpenTitleContent error %d", fh);
			const u32 contentId = this->GetTMD()->contents[0].cid;
			char file[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
			sprintf(file, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(_titleId), TITLE_LOWER(_titleId), contentId);

			fh = ISFS_Open(file, ISFS_OPEN_READ);
			// fuck failed. lets check SD & GTFO
			if(fh < 0)
			{
				if(fh == -106 && this->IsMovedToSD())
				{
					_titleName = { 
						.Name = UNKNOWN_TITLE_NAME_SD, 
						.UnicodeName = {0}
					};

					return _titleName;
				}

				throw "failed to open title content of " + GetTitleLongString(_titleId) + ",error " + std::to_string(fh);
			}

			// read the completed IMET header
			ret = ISFS_Read(fh, openingBanner, sizeof(NandOpeningBanner));
			ISFS_Close(fh);
			if (ret < 0) 
				throw "failed to read title content of " + GetTitleLongString(_titleId) + ",error " + std::to_string(fh);
		}

		auto imetHeader = static_cast<NandOpeningBanner*>(openingBanner)->Header;
		if(imetHeader.Signature != IMET_HEADER_ID)
			imetHeader = static_cast<DiscOpeningBanner*>(openingBanner)->Header;

		// check if its a valid imet header
		if(imetHeader.Signature != IMET_HEADER_ID) 
			throw "Invalid IMET header for " + GetTitleLongString(_titleId);

		/*
			because we dont support unicode stuff in font.cpp we will force to use english.
			but what we should be doing otherwise : 
			int lang = CONF_GetLanguage();
			languages is currently set to the last language (korean) + 1 to get the max value
		*/
		const int lang = CONF_LANG_ENGLISH;
		char str[MAX_TITLE_NAME] = {0};
		char str_unprocessed[MAX_TITLE_NAME] = {0};

		u8 index = 0;
		for(u8 charIndex = 0; charIndex < MAX_TITLE_NAME; charIndex++)
		{
			char titleChar = imetHeader.Names[lang][charIndex];
			//filter out any non-printable characters
			if(titleChar >= 0x20 && titleChar <= 0x7E)
				str[index++] = titleChar;
			
			str_unprocessed[charIndex] = titleChar;
		}

		mem_free(openingBanner);
		str[MAX_TITLE_NAME-1] = '\0';
		str_unprocessed[MAX_TITLE_NAME-1] = '\0';
		gdprintf("GetTitleName : title %s", str);
		if(str[0] == '\0')
			throw "no name found";

		_titleName = {
			.Name = str
		};

		memcpy(_titleName.UnicodeName, str_unprocessed, MAX_TITLE_NAME);
	}
	catch (const std::string& ex)
	{
		gprintf("GetTitleName: %s", ex.c_str());
		_titleName = { 
			.Name = UNKNOWN_TITLE_NAME, 
			.UnicodeName = {0}
		};
	}
	catch (char const* ex)
	{
		gprintf("GetTitleName: %s", ex);
		_titleName = { 
			.Name = UNKNOWN_TITLE_NAME, 
			.UnicodeName = {0}
		};
	}
	catch(...)
	{
		gprintf("GetTitleName: Unknown Error Occurred");
		_titleName = { 
			.Name = UNKNOWN_TITLE_NAME, 
			.UnicodeName = {0}
		};
	}

	if(openingBanner)
		mem_free(openingBanner);
	if(ticketViews)
		mem_free(ticketViews);

	return _titleName;
}

void TitleInformation::LaunchTitle()
{
	//lets start this bitch
	if(DVDDiscAvailable())
	{
		gprintf("LaunchTitle : excecuting StopDisc Async...");
		DVDStopDriveAsync();
	}
	else
		gprintf("LaunchTitle : Skipping StopDisc -> no drive or disc in drive");

	try
	{
		u32 cnt ATTRIBUTE_ALIGN(32) = 0;
		if (ES_GetNumTicketViews(_titleId, &cnt) < 0)
			throw "GetNumTicketViews failure";

		STACK_ALIGN(tikview, views, cnt, 32);
		if (ES_GetTicketViews(_titleId, views, cnt) < 0 )
			throw "ES_GetTicketViews failure!";

		auto titleName = this->GetTitleName();
		if(titleName.UnicodeName[0] != '\0' && wcslen(reinterpret_cast<wchar_t*>(titleName.UnicodeName)))
		{
			//kill play_rec.dat if its already there...
			ISFS_Delete(PLAYRECPATH);
			//and create it with the new info :)
			std::string id;
			id.push_back(TITLE_LOWER(_titleId));
			Playlog_Update(id.c_str(), titleName.UnicodeName);
		}
		else
		{
			gprintf("no title name to use in play_rec");
		}

		net_wc24cleanup();
		ClearState();
		SetNandBootInfo();
		Input_Shutdown(false);
		ShutdownMounts();

		gdprintf("waiting for drive to stop...");
		while(DVDAsyncBusy());
		if(system_state.Init)
		{
			VIDEO_SetBlack(true);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}

		auto titleRegion = this->GetTitleRegion();
		auto regionMatches = VideoRegionMatches(titleRegion);

		//if our region mismatched, we need to also verify against our list of known HBC channels
		if (!regionMatches)
		{
			for (u32 hbcIndex = 0; hbcIndex < HBCTitles.size(); hbcIndex++)
			{
				if (HBCTitles[hbcIndex]->TitleId == _titleId)
				{
					regionMatches = true;
					break;
				}
			}
		}

		//only shutdown video if:
		// * region mismatched
		// * not (known) HBC
		// * titleType == TITLE_TYPE_DOWNLOAD 
		// * TITLE_GAMEID_TYPE(gameId) != H,W or O
		if (!regionMatches && TITLE_UPPER(_titleId) == TITLE_TYPE_DOWNLOAD)
		{
			switch (TITLE_GAMEID_TYPE(TITLE_LOWER(_titleId)))
			{
				case 'C':
				case 'E':
				case 'F':
				case 'J':
				case 'L':
				case 'M':
				case 'N':
				case 'P':
				case 'Q':
					gprintf("LaunchTitle : (%08X\\%08X) Region Mismatch ! %d -> %d", _titleId, VI_TVMODE_FMT(rmode->viTVMode), titleRegion);
					//calling SetVideoModeForTitle to force video mode here would inexplicably revert (correct) 480p to 480i sometimes; see issue #376
					//instead we call SetupVideoInterfaceConfig later on to setup the VICFG bits and just shut down video				
					ShutdownVideo();
					break;
				case 'H':
				case 'O':
				case 'W':
				case 'X':
				default:
					break;
			}
		}

		SetVideoInterfaceConfig(std::shared_ptr<TitleInformation>(this));
		ES_LaunchTitle(_titleId, &views[0]);

		//failed to launch title
		InitVideo();
		if(system_state.Init)
		{
			VIDEO_SetBlack(false);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
		Input_Init();

		throw "failed to launch";
	}
	catch (char const* ex)
	{
		gprintf("LaunchTitle (%s) Exception : %s", GetTitleLongString(_titleId).c_str(), ex);
	}
	catch (const std::string& ex)
	{
		gprintf("LaunchTitle (%s) Exception : %s", GetTitleLongString(_titleId).c_str(), ex);
	}
	catch(...)
	{	
		gprintf("LaunchTitle (%s) General Exception", GetTitleLongString(_titleId).c_str());
	}

	while(DVDAsyncBusy());
}

s32 LoadListTitles( void )
{
	PrintFormat( 1, ((rmode->viWidth /2)-((strlen("loading titles..."))*13/2))>>1, 208+16, "loading titles...");
	s32 ret;
	u32 count = 0;
	ret = ES_GetNumTitles(&count);
	if (ret < 0)
	{
		gprintf("GetNumTitles error %x",ret);
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to get the amount of installed titles!"))*13/2))>>1, 208+16, "Failed to get the amount of installed titles!");
		sleep(3);
		return ret;
	}
	if(count == 0)
	{
		gprintf("count == 0");
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to get the titles list!"))*13/2))>>1, 208+32, "Failed to get the titles list!");
		sleep(3);
		return ret;
	}
	gdprintf("%u titles",count);

	u64* title_list = static_cast<u64*>(mem_align( 32, ALIGN32(sizeof(u64)*count) ));
	if(title_list == NULL)
	{
		gprintf("LoadListTitles : fail to memalign list");
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to get the titles list!"))*13/2))>>1, 208+32, "Failed to get the titles list!");
		sleep(3);
		return ret;
	}
	memset(title_list,0,sizeof(u64)*count);
	ret = ES_GetTitles(title_list, count);
	if (ret < 0) {
		mem_free(title_list);
		gprintf("ES_GetTitles error %x",-ret);
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to get the titles list!"))*13/2))>>1, 208+32, "Failed to get the titles list!");
		sleep(3);
		return ret;
	}

	std::vector<std::unique_ptr<TitleInformation>> titles;
	titles.emplace_back(std::make_unique<TitleInformation>(0, "<-- Go Back"));

	for(u32 i = 0;i < count;i++)
	{	
		switch (TITLE_UPPER(title_list[i]))
		{
			case TITLE_TYPE_HIDDEN:		// "Hidden channels" -- EULA, rgnsel
			case TITLE_TYPE_GAMECHANNEL:// Channels installed by disc games -- WiiFit channel, etc
			case TITLE_TYPE_DOWNLOAD:	// Normal channels / VC
			case TITLE_TYPE_SYSTEM:		// "System channels" -- News, Weather, etc.
			{
				auto titleid = title_list[i];
				try
				{
					auto titleInformation = std::make_unique<TitleInformation>(titleid);
					if(!titleInformation.get()->IsInstalled())
						break;
					
					auto name = titleInformation.get()->GetTitleName();
					titles.emplace_back(std::move (titleInformation));
					gprintf("LoadListTitles : added %d , title id %08X/%08X(%s)", titles.size()-1, TITLE_UPPER(titleid), TITLE_LOWER(titleid), name.Name.c_str());
				}
				catch(...)
				{
					gprintf("LoadListTitles: failed to load title %08X/%08X", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
				}

				break;
			}
			case TITLE_TYPE_ESSENTIAL:	// IOS, MIOS, BC, System Menu
			case TITLE_TYPE_DISC:		// TMD installed by running a disc
			case TITLE_TYPE_DLC:		// Downloadable Content for Wiiware
			default:
				break;
		}
	}
	mem_free(title_list);

	//done detecting titles. lets list them
	if(titles.size() <= 1)
	{
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("ERROR : No VC/Wiiware channels found"))*13/2))>>1, 208+32, "ERROR : No VC/Wiiware channels found");
		sleep(3);
		return 0;
	}
	
	ClearScreen();

	s8 redraw = true;
	s16 cur_off = 0;
	//eventho normally a tv would be able to show 23 titles; some TV's do 60hz in a horrible mannor 
	//making title 23 out of the screen just like the main menu
	s16 max_pos;
	if( VI_TVMODE_ISFMT(rmode->viTVMode, VI_NTSC) || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
	{
		//ye, those tv's want a special treatment again >_>
		max_pos = 14;
	}
	else
	{
		max_pos = 19;
	}
	s16 min_pos = 0;

	if ((s32)titles.size() <= max_pos)
		max_pos = titles.size() -1;
	while(1)
	{
		Input_ScanPads();

		u32 pressed  = Input_ButtonsDown();
		if ( pressed & INPUT_BUTTON_B )
		{
			if(titles.size())
				titles.clear();
			break;
		}
		if ( pressed & INPUT_BUTTON_UP )
		{
			cur_off--;
			if (cur_off < min_pos)
			{
				min_pos = cur_off;
				if(titles.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                        ");
						PrintFormat( 0,((rmode->viWidth /2)-((strlen("               "))*13/2))>>1,64+(max_pos+2)*16,"               ");
						PrintFormat( 0,((rmode->viWidth /2)-((strlen("               "))*13/2))>>1,64,"               ");
					}
				}
			}
			if (cur_off < 0)
			{
				cur_off = titles.size() - 1;
				min_pos = titles.size() - max_pos - 1;
			}
			redraw = true;
		}
		if ( pressed & INPUT_BUTTON_DOWN )
		{
			cur_off++;
			if (cur_off > (max_pos + min_pos))
			{
				min_pos = cur_off - max_pos;
				if(titles.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                        ");
						PrintFormat( 0,((rmode->viWidth /2)-((strlen("               "))*13/2))>>1,64+(max_pos+2)*16,"               ");
						PrintFormat( 0,((rmode->viWidth /2)-((strlen("               "))*13/2))>>1,64,"               ");
					}
				}
			}
			if (cur_off >= (s32)titles.size())
			{
				cur_off = 0;
				min_pos = 0;
			}
			redraw = true;
		}
		if ( pressed & INPUT_BUTTON_A )
		{
			if(cur_off == 0)
			{
				titles.clear();
				break;
			}

			ClearScreen();
			PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Loading title..."))*13/2))>>1, 208, "Loading title...");

			titles[cur_off].get()->LaunchTitle();

			PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to Load Title!"))*13/2))>>1, 224, "Failed to Load Title!");
			sleep(3);
			redraw = true;
		}			
		if(redraw)
		{
			s16 i= min_pos;
			if((s32)titles.size() -1 > max_pos && (min_pos != (s32)titles.size() - max_pos - 1))
			{
				PrintFormat( 0,((rmode->viWidth /2)-((strlen("-----More-----"))*13/2))>>1,64+(max_pos+2)*16,"-----More-----");
			}
			if(min_pos > 0)
			{
				PrintFormat( 0,((rmode->viWidth /2)-((strlen("-----Less-----"))*13/2))>>1,64,"-----Less-----");
			}
			for(; i<=(min_pos + max_pos); i++ )
			{
				try
				{
					auto titleName = titles[i]->GetTitleName();
					if(i == 0)
					{
						PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "%s                             ", titleName.Name.c_str());
					}
					else
					{
						PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "(%d)%s(%s)                              ", i, titleName.Name.c_str(), titles[i]->GetTitleIdString().c_str());
					}
				}
				catch(...)
				{
					gprintf("oh ow, exception in printing title %d", i);
				}
				//gprintf("lolid : %s - %x & %x ",title_ID,titles[i].title_id,(titles[i].title_id & 0x00000000FFFFFFFF) << 32);			
			}
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("A(A) Load Title       "))*13/2))>>1, rmode->viHeight-32, "A(A) Load Title");
			redraw = false;
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
