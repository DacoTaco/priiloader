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
#include "mem2_manager.h"
#include "dvd.h"
#include "Input.h"

#define USE_DVD_ASYNC

s8 CheckTitleOnSD(u64 id)
{
	//check Mounted Devices so that IF the SD is inserted it also gets mounted
	PollDevices();
	if (!(GetMountedValue() & 2)) //no SD mounted, lets bail out
		return -1;
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
	char file[64] ATTRIBUTE_ALIGN(32);
	sprintf(file, "fat:/private/wii/title/%s/content.bin", title_ID);
	FILE* SDHandler = fopen(file,"rb");
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
s8 GetTitleName(u64 id, u32 app, char* name, u8* _dst_uncode_name) 
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
	int lang = 1;
	s32 r;
	u8 return_unicode_name = (_dst_uncode_name == NULL)? 0 : 1;

    char file[64] ATTRIBUTE_ALIGN(32);
    sprintf(file, "/title/%08x/%08x/content/%08x.app", (u32)(id >> 32), (u32)(id & 0xFFFFFFFF), app);
	gdprintf("GetTitleName : %s",file);
	u32 cnt ATTRIBUTE_ALIGN(32);
	cnt = 0;
	IMET *data = (IMET *)mem_align(32, ALIGN32( sizeof(IMET) ) );
	if(data == NULL)
	{
		gprintf("GetTitleName : IMET header align failure");
		return -1;
	}
	memset(data,0,sizeof(IMET) );
	r = ES_GetNumTicketViews(id, &cnt);
	if(r < 0)
	{
		gprintf("GetTitleName : GetNumTicketViews error %d!",r);
		mem_free(data);
		return -1;
	}
	tikview *views = (tikview *)mem_align( 32, sizeof(tikview)*cnt );
	if(views == NULL)
	{
		mem_free(data);
		return -2;
	}
	r = ES_GetTicketViews(id, views, cnt);
	if (r < 0)
	{
		gprintf("GetTitleName : GetTicketViews error %d ",r);
		mem_free(data);
		mem_free(views);
		return -3;
	}

	//lets get this party started with the right way to call ES_OpenTitleContent. and not like how libogc < 1.8.3 does it. patch was passed on , and is done correctly in 1.8.3
	//the right way is ES_OpenTitleContent(u64 TitleID,tikview* views,u16 Index); note the views >_>
	s32 fh = ES_OpenTitleContent(id, views, 0);
	if (fh == -106)
	{
		CheckTitleOnSD(id);
		mem_free(data);
		mem_free(views);
		return -106;
	}
	else if(fh < 0)
	{
		//ES method failed. remove tikviews from memory and fall back on ISFS method
		gprintf("GetTitleName : ES_OpenTitleContent error %d",fh);
		mem_free(views);
		fh = ISFS_Open(file, ISFS_OPEN_READ);
		// fuck failed. lets check SD & GTFO
		if (fh == -106)
		{
			CheckTitleOnSD(id);
			return -106;
		}
		else if (fh < 0)
		{
			mem_free(data);
			gprintf("open %s error %d",file,fh);
			return -5;
		}
		// read the completed IMET header
		r = ISFS_Read(fh, data, sizeof(IMET));
		if (r < 0) {
			gprintf("IMET read error %d",r);
			ISFS_Close(fh);
			mem_free(data);
			return -6;
		}
		ISFS_Close(fh);
	}
	else
	{
		//ES method
		r = ES_ReadContent(fh,(u8*)data,sizeof(IMET));
		if (r < 0) {
			gprintf("GetTitleName : ES_ReadContent error %d",r);
			ES_CloseContent(fh);
			mem_free(data);
			mem_free(views);
			return -8;
		}
		//free data and let it point to IMET_data so everything else can work just fine
		ES_CloseContent(fh);
		mem_free(views);
	}
	char str[10][84];
	char str_unprocessed[10][84];
	//clear any memory that is in the place of the array cause we dont want any confusion here
	memset(str,0,10*84);
	if(return_unicode_name)
		memset(str_unprocessed,0,10*84);
	if(data->imet == 0x494d4554) // check if its a valid imet header
	{
		for(u8 y =0;y <= 9;y++)
		{
			u8 p = 0;
			u8 up = 0;
			for(u8 j=0;j<83;j++)
			{
				if(data->names[y][j] < 0x20)
					if(return_unicode_name && data->names[y][j] == 0x00)
						str_unprocessed[y][up++] = data->names[y][j];
					else
						continue;
				else if(data->names[y][j] > 0x7E)
					continue;
				else
				{
					str[y][p++] = data->names[y][j];
					str_unprocessed[y][up++] = data->names[y][j];
				}
			}
			str[y][83] = '\0';

		}
		mem_free(data);
	}
	else
	{
		gprintf("invalid IMET header for 0x%08x/0x%08x", (u32)(id >> 32), (u32)(id & 0xFFFFFFFF));
		return -9;
	}
	if(str[lang][0] != '\0')
	{
		gdprintf("GetTitleName : title %s",str[lang]);
		memcpy(name, str[lang], strnlen(str[lang],255));
		if (return_unicode_name && str_unprocessed[lang][1] != '\0')
		{
			memcpy(_dst_uncode_name,&str_unprocessed[lang][0],83);
		}
		else if(return_unicode_name)
			gprintf("WARNING : empty unprocessed string");
	}
	else
		gprintf("GetTitleName: no name found");
	memset(str,0,10*84);
	memset(str_unprocessed,0,10*84);
	return 1;
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
	char temp_name[256];
	char title_ID[5];
	for(u32 i = 0;i < count;i++)
	{	
		//u32 titletype = title_list[i] >> 32;
		switch (title_list[i] >> 32)
		{
			case 0x10001: // Normal channels / VC
			case 0x10002: // "System channels" -- News, Weather, etc.
			{
				u32 tmd_size;
				ret = ES_GetTMDViewSize(title_list[i], &tmd_size);
				if(ret<0)
				{
					gprintf("WARNING : GetTMDViewSize error %d on title %x-%x",ret,(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("WARNING : TMDSize error on 00000000-00000000!"))*13/2))>>1, 208+32, "WARNING : TMDSize error on %08X-%08X",(u32)(title_list[i] >> 32),(u32)(title_list[i] & 0xFFFFFFFF));
					sleep(3);
					if( !SGetSetting(SETTING_BLACKBACKGROUND))
						VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
					else
						VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
					VIDEO_WaitVSync();
					printf("\x1b[5;0H");
					fflush(stdout);
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
					if( !SGetSetting(SETTING_BLACKBACKGROUND))
						VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
					else
						VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
					VIDEO_WaitVSync();
					printf("\x1b[5;0H");
					fflush(stdout);
					PrintFormat( 1, ((rmode->viWidth /2)-((strlen("loading titles..."))*13/2))>>1, 208+16, "loading titles...");
					continue;
				}
				memset(temp_name,0,sizeof(temp_name));
				sprintf(temp_name,"????????");
				title_info temp;
				temp.title_id = 0;
				temp.name_ascii.clear();
				memset(temp.name_unicode,0,84);
				temp.content_id = 0;
				ret = GetTitleName(rTMD->title_id,rTMD->contents[0].cid,temp_name,temp.name_unicode);
				if ( ret != -106 )
				{
					temp.title_id = rTMD->title_id;
					temp.name_ascii = temp_name;
					temp.content_id = rTMD->contents[0].cid;
					//gprintf("0x%02X%02X%02X%02X",temp.name_unicode[0],temp.name_unicode[1],temp.name_unicode[2],temp.name_unicode[3]);
					titles.push_back(temp);
					gprintf("LoadListTitles : added %d , title id %08X/%08X(%s)",titles.size(),TITLE_UPPER(temp.title_id),TITLE_LOWER(temp.title_id),temp.name_ascii.c_str());
				}
				if(rTMD)
				{
					mem_free(rTMD);
				}
				//break;
			}
			case 1: // IOS, MIOS, BC, System Menu
			case 0x10000: // TMD installed by running a disc
			case 0x10004: // "Hidden channels by discs" -- WiiFit channel
			case 0x10008: // "Hidden channels" -- EULA, rgnsel
			case 0x10005: // Downloadable Content for Wiiware
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
	if( !SGetSetting(SETTING_BLACKBACKGROUND))
		VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
	else
		VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	printf("\x1b[5;0H");
	fflush(stdout);
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
	if ((s32)titles.size() < max_pos)
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

			if( !SGetSetting(SETTING_BLACKBACKGROUND))
				VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
			else
				VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
			VIDEO_WaitVSync();
			printf("\x1b[5;0H");
			fflush(stdout);
			PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Loading title..."))*13/2))>>1, 208, "Loading title...");

			//lets start this bitch
#ifdef USE_DVD_ASYNC
			if(DVDCheckCover())
			{
				gprintf("LoadListTitles : excecuting StopDisc Async...");
				DVDStopDisc(true);
			}
			else
			{
				gprintf("LoadListTitles : Skipping StopDisc -> no drive or disc in drive");
			}
#else
			if(DVDCheckCover())
			{
				gprintf("LoadListTitles : excecuting StopDisc...");
				DVDStopDisc(false);
			}
			else
			{
				gprintf("LoadListTitles : Skipping StopDisc -> no drive or disc in drive");
			}
#endif
			
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
				ret = ISFS_Delete(PLAYRECPATH);
				//and create it with the new info :)
				std::string id;
				id.push_back(titles[cur_off].title_id & 0xFFFFFFFF);
				ret = Playlog_Update(id.c_str(), titles[cur_off].name_unicode);
				gdprintf("play_rec ret = %d",ret);
			}
			else
			{
				gprintf("no title name to use in play_rec");
			}
			net_wc24cleanup();
			ClearState();
			SetNandBootInfo();
			Input_Shutdown();
			ShutdownDevices();

#ifdef USE_DVD_ASYNC
			gdprintf("waiting for drive to stop...");
			while(DvdKilled() < 1);
#endif
			VIDEO_SetBlack(1);
			VIDEO_Flush();

			ES_LaunchTitle(titles[cur_off].title_id, &views[0]);
failure:
			VIDEO_SetBlack(0);
			VIDEO_Flush();
			VIDEO_WaitVSync();
			Input_Init();
			PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to Load Title!"))*13/2))>>1, 224, "Failed to Load Title!");
			while(DvdKilled() < 1);
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
					PrintFormat( cur_off==i, 16, 64+(i-min_pos+1)*16, "(%d)%s(%s)                              ",i+1,titles[i].name_ascii.c_str(), title_ID);
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
