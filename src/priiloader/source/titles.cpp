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
#include <ogc/machine/processor.h>

#include "titles.h"
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
const title_info HBC_Titles[] = {
	{ 0x0001000148415858LL, "HAXX" },
	{ 0x000100014A4F4449LL, "JODI" },
	{ 0x00010001AF1BF516LL, "0.7 - 1.1" },
	{ 0x000100014C554C5ALL, "LULZ" },
	{ 0x000100014F484243LL, "OpenHBC 1.4"}
};
const s32 HBC_Titles_Size = (s32)((sizeof(HBC_Titles) / sizeof(HBC_Titles[0])));

s8 CheckTitleOnSD(u64 id)
{
	if (!HAS_SD_FLAG(GetMountedFlags())) //no SD mounted, lets bail out
	{
		gprintf("CheckTitleOnSD : no SD card inserted");
		return 0;
	}

	char title_ID[5];
	//Check app on SD. it might be there. not that it matters cause we can't boot from SD
	memset(title_ID,0,5);
	u32 title_l = id & 0xFFFFFFFF;
	memcpy(title_ID, &title_l, 4);
	for (s8 f=0; f<4; f++)
	{
		if(title_ID[f] < 0x20)
			title_ID[f] = '.';
		if(title_ID[f] > 0x7E)
			title_ID[f] = '.';
	}
	title_ID[4]='\0';
	std::string filepath = BuildPath("/private/wii/title/%s/content.bin", StorageDevice::SD);
	filepath.replace(filepath.find("%s"), 2, title_ID);
	FILE* SDHandler = fopen(filepath.c_str(),"rb");
	if (SDHandler)
	{
		//content.bin is there meaning its on SD
		fclose(SDHandler);
		gprintf("CheckTitleOnSD : title is saved on SD");
		return 1;
	}
	else
	{
		//title isn't on SD either. ow well...
		gprintf("CheckTitleOnSD : content not found on NAND or SD for %08X\\%08X",(u32)(id >> 32),title_l);
		return 0;
	}
}

s32 GetTitleTMD(u64 titleId, signed_blob* &blob, u32 &blobSize)
{
	if (blob)
		mem_free(blob);

	s32 ret = 0;
	s32 fd = -1;
	try 
	{
		//IOS versions, starting with 28 have ES calls to retrieve the TMD. lets use those
		if (IOS_GetVersion() >= 28)
		{
			//get the TMD. we need the TMD, not views, as SM 1.0's boot index != last content
			ret = ES_GetStoredTMDSize(titleId, &blobSize);
			if (ret < 0 || blobSize > MAX_SIGNED_TMD_SIZE)
				throw ("GetTitleTMD error " + std::to_string(ret));

			blob = (signed_blob*)mem_align(32, MAX_SIGNED_TMD_SIZE);
			if (blob == NULL)
				throw "GetTitleTMD: memalign TMD failure";

			memset(blob, 0, MAX_SIGNED_TMD_SIZE);

			ret = ES_GetStoredTMD(titleId, blob, blobSize);
			if (ret < 0)
				throw ("GetTitleTMD error " + std::to_string(ret));

			return ret;
		}

		//Other IOS calls would need direct NAND Access to load the TMD.
		//so here we go
		gprintf("GetTitleTMD : Load TMD from nand");
		STACK_ALIGN(fstats, TMDStatus, sizeof(fstats), 32);		
		char TMD_Path[ISFS_MAXPATH];
		memset(TMD_Path, 0, 64);
		sprintf(TMD_Path, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleId), TITLE_LOWER(titleId));
		fd = ISFS_Open(TMD_Path, ISFS_OPEN_READ);
		if (fd < 0)
		{
			ret = fd;
			throw ("GetTitleTMD : failed to open Title TMD. ");
		}

		ret = ISFS_GetFileStats(fd, TMDStatus);
		if (ret < 0)
			throw ("GetTitleTMD : Failed to get TMD information. ");

		blob = (signed_blob*)mem_align(32, MAX_SIGNED_TMD_SIZE);
		if (blob == NULL)
			throw "GetTitleTMD: memalign TMD failure";

		memset(blob, 0, MAX_SIGNED_TMD_SIZE);

		ret = ISFS_Read(fd, blob, TMDStatus->file_length);
		if (ret < 0)
			throw ("GetTitleTMD : Failed to read TMD data. ");

		ISFS_Close(fd);
	}
	catch (const std::string& ex)
	{
		gprintf("GetTitleTMD Exception -> %s", ex.c_str());
		if (blob)
			mem_free(blob);

		if (fd >= 0)
			ISFS_Close(fd);
	}
	catch (char const* ex)
	{
		gprintf("GetTitleTMD Exception -> %s", ex);
		if (blob)
			mem_free(blob);

		if (fd >= 0)
			ISFS_Close(fd);
	}
	catch (...)
	{
		gprintf("GetTitleTMD Exception was thrown");
		if (blob)
			mem_free(blob);

		if (fd >= 0)
			ISFS_Close(fd);
	}

	return ret;
}

s8 GetTitleName(u64 id, u32 app, char* name, u8* unicodeName) 
{	
    /*
		languages:
		enum {
			CONF_LANG_JAPANESE = 0,
			CONF_LANG_ENGLISH,
			CONF_LANG_GERMAN,
			CONF_LANG_FRENCH,
			CONF_LANG_SPANISH,
			CONF_LANG_ITALIAN,
			CONF_LANG_DUTCH,
			CONF_LANG_SIMP_CHINESE,
			CONF_LANG_TRAD_CHINESE,
			CONF_LANG_KOREAN
		};
		cause we dont support unicode stuff in font.cpp we will force to use english then(1)
		but what we should be doing otherwise : 
		int lang = CONF_GetLanguage();
    */
	const int languages = 10;
	const int lang = CONF_LANG_ENGLISH;
	s32 ret = 0;
	u32 cnt ATTRIBUTE_ALIGN(32) = 0;
	IMET *imetHeader = NULL;
	tikview *ticketViews = NULL;

	try
	{
		imetHeader = (IMET *)mem_align(32, ALIGN32( sizeof(IMET) ) );
		if(imetHeader == NULL)
			throw "failed to alloc IMET header";
		
		memset(imetHeader,0,sizeof(IMET) );
		ret = ES_GetNumTicketViews(id, &cnt);
		if(ret < 0)
			throw "GetNumTicketViews " + std::to_string(ret);

		ticketViews = (tikview *)mem_align( 32, sizeof(tikview)*cnt );
		if(ticketViews == NULL)
			throw "Failed to alloc ticket views";

		ret = ES_GetTicketViews(id, ticketViews, cnt);
		if(ret < 0)
			throw "ES_GetTicketViews " + std::to_string(ret);

		s32 fh = ES_OpenTitleContent(id, ticketViews, 0);
		if(fh == -106)
		{
			CheckTitleOnSD(id);
			ret = -106;
			goto return_getTitle;
		}

		switch(fh >= 0)
		{
			//ES method
			case true:
			{
				ret = ES_ReadContent(fh, (u8*)imetHeader, sizeof(IMET));
				ES_CloseContent(fh);
				if (ret < 0) 
					throw "IMET ES_ReadContent " + std::to_string(ret);
				break;
			}

			//ES method failed. remove tikviews from memory and fall back on ISFS method
			case false:
			default:
			{
				gprintf("GetTitleName : ES_OpenTitleContent error %d",fh);
				char file[64] ATTRIBUTE_ALIGN(32);
				sprintf(file, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(id), TITLE_LOWER(id), app);
				gdprintf("GetTitleName : %s",file);

				fh = ISFS_Open(file, ISFS_OPEN_READ);
				// fuck failed. lets check SD & GTFO
				if(fh < 0)
				{
					if (fh != -106)
						throw "IMET ISFS_Open " + std::to_string(ret);

					CheckTitleOnSD(id);
					ret = -106;
					goto return_getTitle;
				}

				// read the completed IMET header
				ret = ISFS_Read(fh, imetHeader, sizeof(IMET));
				ISFS_Close(fh);
				if (ret < 0) 
					throw "IMET ISFS_Read " + std::to_string(ret);
				break;
			}
		}

		mem_free(ticketViews);
		
		// check if its a valid imet header
		if(imetHeader->imet != 0x494d4554) 
			throw "Invalid IMET header for " + std::to_string(TITLE_UPPER(id)) + "/" + std::to_string(TITLE_LOWER(id));

		char str[languages][MAX_TITLE_NAME];
		char str_unprocessed[languages][MAX_TITLE_NAME];
		//clear any memory that is in the place of the array cause we dont want any confusion here
		memset(str, 0, sizeof(str));
		memset(str_unprocessed, 0, sizeof(str_unprocessed));

		for(u8 language = 0; language < languages; language++)
		{
			u8 strIndex = 0;
			for(u8 charIndex = 0; charIndex < MAX_TITLE_NAME; charIndex++)
			{
				char titleChar = imetHeader->names[language][charIndex];
				//filter out any non-printable characters
				if(titleChar < 0x20 || titleChar > 0x7E)
					continue;

				str[language][strIndex] = titleChar;
				str_unprocessed[language][strIndex++] = titleChar;
			}
			str[language][MAX_TITLE_NAME-1] = '\0';
		}

		mem_free(imetHeader);

		if(str[lang][0] != '\0')
		{
			gdprintf("GetTitleName : title %s",str[lang]);
			s32 nameLength = strnlen(str[lang], MAX_TITLE_NAME-1);
			memcpy(name, str[lang], nameLength);
			str[lang][nameLength+1] = '\0';

			if(unicodeName != NULL)
			{
				if (str_unprocessed[lang][1] != '\0')
					memcpy(unicodeName, &str_unprocessed[lang][0], MAX_TITLE_NAME);
				else
					gprintf("WARNING : empty unprocessed string");
			}
			
		}
		else
			gprintf("GetTitleName: no name found");
	}
	catch (char const* ex)
	{
		gprintf("GetTitleName Exception : %s",ex);
		ret = -1;
	}
	catch (const std::string& ex)
	{
		gprintf("GetTitleName Exception : %s",ex);
		ret = -1;
	}
	catch(...)
	{	
		gprintf("GetTitleName General Exception");
		ret = -2;
	}

return_getTitle:
	mem_free(imetHeader);
	mem_free(ticketViews);
	return ret;
}

u8 GetTitleRegion(u32 lowerTitleId)
{
	//Taken from Dolphin
	switch (lowerTitleId & 0xFF)
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
			return TITLE_PAL;

			//NTSC-J
		case 'J':
		case 'K':
		case 'Q':
		case 'T':
			return TITLE_NTSC_J;

			//NTSC-U
		default:
			gprintf("unknown Region");
		case 'B':
		case 'N':
		case 'E':
			return TITLE_NTSC;
	}
}

s8 VideoRegionMatches(s8 titleRegion)
{
	switch (rmode->viTVMode)
	{
		case VI_NTSC:
		case VI_DEBUG:
			return (titleRegion == TITLE_NTSC || titleRegion == TITLE_NTSC_J);
		case VI_PAL:
		case VI_MPAL:
		case VI_DEBUG_PAL:
		case VI_EURGB60:
			return (titleRegion == TITLE_PAL);
		default:
			return 1;
	}
}

s8 SetVideoModeForTitle(u32 lowerTitleId)
{
	//always set video, 
	s8 titleRegion = GetTitleRegion(lowerTitleId);
	GXRModeObj* vidmode = rmode;
	s8 videoMode = 0;
	switch (titleRegion)
	{
		//PAL
		case TITLE_PAL:
			gprintf("PAL50");
			// set 50Hz mode - incompatible with S-Video cables!
			vidmode = &TVPal528IntDf;
			videoMode = SYS_VIDEO_PAL;
			break;

		case TITLE_NTSC_J:
			gprintf("NTSC-J");
			goto region_ntsc;
			break;

		case TITLE_NTSC:
			gprintf("NTSC-U");
		region_ntsc:
			videoMode = SYS_VIDEO_NTSC;
			vidmode = &TVNtsc480IntDf;
			break;
		default:
			gprintf("unknown titleRegion");
			break;
	}

	//set video mode for the game
	if (rmode != vidmode && !VideoRegionMatches(titleRegion))
		ConfigureVideoMode(vidmode);

	return videoMode;
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

	u64* title_list = (u64*)mem_align( 32, ALIGN32(sizeof(u64)*count) );
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
	std::vector<title_info> titles;
	titles.clear();
	title_info GoBackInfo;
	GoBackInfo.name_ascii = "<-- Go Back";
	titles.push_back(GoBackInfo);
	tmd_view *rTMD;
	char temp_name[MAX_TITLE_NAME];
	char title_ID[5];
	for(u32 i = 0;i < count;i++)
	{	
		//u32 titletype = title_list[i] >> 32;
		switch (title_list[i] >> 32)
		{
			case TITLE_TYPE_GAMECHANNEL:// Channels installed by disc games -- WiiFit channel, etc
			case TITLE_TYPE_DOWNLOAD:	// Normal channels / VC
			case TITLE_TYPE_SYSTEM:		// "System channels" -- News, Weather, etc.
			{
				u32 tmd_size;
				ret = ES_GetTMDViewSize(title_list[i], &tmd_size);
				if(ret<0)
				{
					gprintf("WARNING : GetTMDViewSize error %d on title %x-%x",ret,(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("WARNING : TMDSize error on 00000000-00000000!"))*13/2))>>1, 208+32, "WARNING : TMDSize error on %08X-%08X",(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					sleep(3);

					ClearScreen();
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("loading titles..."))*13/2))>>1, 208+16, "loading titles...");
					continue;
				}
				rTMD = (tmd_view*)mem_align( 32, ALIGN32(tmd_size) );
				if( rTMD == NULL )
				{
					mem_free(title_list);
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to MemAlign TMD!"))*13/2))>>1, 208+32, "Failed to MemAlign TMD!");
					sleep(3);
					return 0;
				}
				memset(rTMD,0, tmd_size );
				ret = ES_GetTMDView(title_list[i], (u8*)rTMD, tmd_size);
				if(ret<0)
				{
					gprintf("WARNING : GetTMDView error %d on title %x-%x",ret,(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("WARNING : TMD error on 00000000-00000000!"))*13/2))>>1, 208+32, "WARNING : TMD error on %08X-%08X!",(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					sleep(3);
					mem_free(rTMD);
					
					ClearScreen();
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("loading titles..."))*13/2))>>1, 208+16, "loading titles...");
					continue;
				}
				memset(temp_name, 0, sizeof(temp_name));
				sprintf(temp_name,"????????");
				title_info temp;
				temp.title_id = 0;
				temp.name_ascii.clear();
				memset(temp.name_unicode, 0, MAX_TITLE_NAME);
				temp.content_id = 0;
				ret = GetTitleName(rTMD->title_id,rTMD->contents[0].cid,temp_name,temp.name_unicode);
				if ( ret != -106 )
				{
					temp.title_id = rTMD->title_id;
					temp.name_ascii = temp_name;
					temp.content_id = rTMD->contents[0].cid;
					titles.push_back(temp);
					gprintf("LoadListTitles : added %d , title id %08X/%08X(%s)",titles.size()-1,TITLE_UPPER(temp.title_id),TITLE_LOWER(temp.title_id),temp.name_ascii.c_str());
				}
				
				mem_free(rTMD);
				//break;
			}
			case TITLE_TYPE_ESSENTIAL:	// IOS, MIOS, BC, System Menu
			case TITLE_TYPE_DISC:		// TMD installed by running a disc
			case TITLE_TYPE_HIDDEN:		// "Hidden channels" -- EULA, rgnsel
			case TITLE_TYPE_DLC:		// Downloadable Content for Wiiware
			default:
				break;
		}
	}
	mem_free(title_list);
	//done detecting titles. lets list them
	if(titles.size() == 0)
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
	if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
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

			//lets start this bitch
			if(DVDDiscAvailable())
			{
				gprintf("LoadListTitles : excecuting StopDisc Async...");
				DVDStopDriveAsync();
			}
			else
			{
				gprintf("LoadListTitles : Skipping StopDisc -> no drive or disc in drive");
			}
			
			s8 regionMatch = 1;
			s8 titleRegion = 0;
			u32 cnt ATTRIBUTE_ALIGN(32) = 0;
			STACK_ALIGN(tikview,views,4,32);

			if (ES_GetNumTicketViews(titles[cur_off].title_id, &cnt) < 0)
			{
				gprintf("GetNumTicketViews failure");
				goto failure;
			}
			if (ES_GetTicketViews(titles[cur_off].title_id, views, cnt) < 0 )
			{
				gprintf("ES_GetTicketViews failure!");
				goto failure;
			}
			if(wcslen((wchar_t*)titles[cur_off].name_unicode))
			{
				//kill play_rec.dat if its already there...
				ISFS_Delete(PLAYRECPATH);
				//and create it with the new info :)
				std::string id;
				id.push_back(titles[cur_off].title_id & 0xFFFFFFFF);
				Playlog_Update(id.c_str(), titles[cur_off].name_unicode);
			}
			else
			{
				gprintf("no title name to use in play_rec");
			}
			net_wc24cleanup();
			ClearState();
			SetNandBootInfo();
			Input_Shutdown();
			ShutdownMounts();

			gdprintf("waiting for drive to stop...");
			while(DVDAsyncBusy());
			if(system_state.Init)
			{
				VIDEO_SetBlack(true);
				VIDEO_Flush();
				VIDEO_WaitVSync();
			}

			titleRegion = GetTitleRegion(TITLE_LOWER(titles[cur_off].title_id));
			regionMatch = VideoRegionMatches(titleRegion);

			//if our region mismatched, we need to also verify against our list of known HBC channels
			if (!regionMatch)
			{
				for (s32 hbcIndex = 0; hbcIndex < HBC_Titles_Size; hbcIndex++)
				{
					if (HBC_Titles[hbcIndex].title_id == titles[cur_off].title_id)
					{
						regionMatch = 1;
						break;
					}
				}
			}

			//only shutdown video if:
			// * region mismatched
			// * not (known) HBC
			// * titleType == TITLE_TYPE_DOWNLOAD 
			// * TITLE_GAMEID_TYPE(gameId) != H,W or O
			if (!regionMatch && TITLE_UPPER(titles[cur_off].title_id) == TITLE_TYPE_DOWNLOAD)
			{
				switch (TITLE_GAMEID_TYPE(TITLE_LOWER(titles[cur_off].title_id)))
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
						gprintf("LoadListTitles : Region Mismatch ! %d -> %d", rmode->viTVMode, titleRegion);
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

			ES_LaunchTitle(titles[cur_off].title_id, &views[0]);
failure:
			InitVideo();
			if(system_state.Init)
			{
				VIDEO_SetBlack(false);
				VIDEO_Flush();
				VIDEO_WaitVSync();
			}
			Input_Init();
			PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to Load Title!"))*13/2))>>1, 224, "Failed to Load Title!");
			while(DVDAsyncBusy());
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
				if(i == 0)
				{
					PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "%s                             ",titles[i].name_ascii.c_str());
				}
				else
				{
					memset(title_ID,0,5);
					u32 title_l = titles[i].title_id & 0xFFFFFFFF;
					memcpy(title_ID, &title_l, 4);
					for (s8 f=0; f<4; f++)
					{
						if(title_ID[f] < 0x20)
							title_ID[f] = '.';
						if(title_ID[f] > 0x7E)
							title_ID[f] = '.';
					}
					title_ID[4]='\0';
					PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "(%d)%s(%s)                              ",i,titles[i].name_ascii.c_str(), title_ID);
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
