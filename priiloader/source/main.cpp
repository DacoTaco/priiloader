/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar

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
// To use libELM define libELM in the priiloader project & dont forget to link it in the makefile

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#ifndef libELM
#include <fat.h>
#include <ogc/usbstorage.h>
#else
#include "elm.h"
#endif
#include <sys/dir.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <vector>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <ogc/ios.h>

//Project files
#include "../../Shared/svnrev.h"
#include "settings.h"
#include "state.h"
#include "elf.h"
#include "processor.h"
#include "asm.h"
#include "error.h"
#include "hacks.h"
#include "font.h"
#include "gecko.h"

//Bin includes
#include "certs_bin.h"

//#define DEBUG

extern "C"
{
	extern void _unstub_start(void);
}

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

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

extern Settings *settings;
extern u32 error;
extern std::vector<hack> hacks;
extern u32 *states;

u32 result=0;
u32 Shutdown=0;

s32 __IOS_LoadStartupIOS()
{
        return 0;
}
extern s32 __IOS_ShutdownSubsystems();

bool MountDevices(void)
{
#ifndef libELM
	__io_wiisd.startup();
	__io_usbstorage.startup();
	if (!fatMountSimple("fat",&__io_wiisd))
	{
		//sd mounting failed. lets go usb
		if(!fatMountSimple("fat", &__io_usbstorage))
		{
			//usb failed too :(
			return false;
		}
		else
		{
			//usb worked!
			return true;
		}
	}
	else
	{
		//it was ok. SD GO!
		return true;
	}
#else
	__io_wiisd.startup();
	if ( __io_wiisd.isInserted() )
	{
		if ( ELM_MountDevice(ELM_SD) < 0)
			return false
		else
			return true;
	}
	else
		return false;
#endif
}
void ShutdownDevices()
{
#ifndef libELM
	//unmount device
	fatUnmount("fat:/");
	//shutdown ports
	__io_wiisd.shutdown();
	__io_usbstorage.shutdown();
#else
	//only SD support atm, srry
	ELM_UnmountDevice(ELM_SD);
	__io_wiisd.shutdown();
#endif
}
bool RemountDevices( void )
{
	ShutdownDevices();
	return MountDevices();
}
void ClearScreen()
{
	if( !SGetSetting(SETTING_BLACKBACKGROUND))
		VIDEO_ClearFrameBuffer( rmode, xfb, 0xFF80FF80);
	else
		VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);

	VIDEO_WaitVSync();
	printf("\n");
}
void SysHackSettings( void )
{
	bool DeviceFound = RemountDevices();
	if(!LoadHacks())
		return;

//Count hacks for current sys version

	u32 HackCount=0;
	u32 SysVersion=GetSysMenuVersion();
	for( unsigned int i=0; i<hacks.size(); ++i)
	{
		if( hacks[i].version == SysVersion )
		{
			HackCount++;
		}
	}

	if( HackCount == 0 )
	{
		if(DeviceFound)
		{
			PrintFormat( 1, ((640/2)-((strlen("Couldn't find any hacks for"))*13/2))>>1, 208, "Couldn't find any hacks for");
			PrintFormat( 1, ((640/2)-((strlen("System Menu version:vxxx"))*13/2))>>1, 228, "System Menu version:v%d", SysVersion );
			sleep(5);
			return;
		}
		else
		{
			PrintFormat( 1, ((640/2)-((strlen("Failed to mount fat device"))*13/2))>>1, 208, "Failed to mount fat device");
			sleep(5);
			return;
		}
	}

	u32 DispCount=HackCount;

	if( DispCount > 20 )
		DispCount = 20;

	s16 cur_off=0;
	s32 menu_off=0;
	bool redraw=true;
 
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

#ifdef DEBUG
		if ( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) )
		{
			exit(0);
		}
#endif
		if ( (WPAD_Pressed & WPAD_BUTTON_B) || (PAD_Pressed & PAD_BUTTON_B) )
		{
			break;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_A) || (PAD_Pressed & PAD_BUTTON_A) )
		{
			if( cur_off == DispCount)
			{
				//first try to open the file on the SD card/USB, if we found it copy it, other wise skip
				s16 fail = 0;
				FILE *in = NULL;
				if (RemountDevices())
				{
#ifndef libELM
					in = fopen ("fat:/hacks.ini","rb");
#else
					in = fopen ("elm:/sd/hacks.ini","rb");
#endif
				}
				else
				{
					gprintf("no FAT device found to look for hacks.ini\n");
				}
				if( in != NULL )
				{
					//Read in whole file & create it on nand
					fseek( in, 0, SEEK_END );
					u32 size = ftell(in);
					fseek( in, 0, 0);

					char *buf = (char*)memalign( 32, (size+31)&(~31) );
					memset( buf, 0, (size+31)&(~31) );
					fread( buf, sizeof( char ), size, in );

					fclose(in);

					s32 fd = ISFS_Open("/title/00000001/00000002/data/hacks.ini", 1|2 );
					if( fd >= 0 )
					{
						//File already exists, delete and recreate!
						ISFS_Close( fd );
						if(ISFS_Delete("/title/00000001/00000002/data/hacks.ini") <0)
						{
							gprintf("delete of hacks.ini failed.\n");
							fail=1;
						}
					}
					if(ISFS_CreateFile("/title/00000001/00000002/data/hacks.ini", 0, 3, 3, 3)<0)
					{
						fail=2;
						gprintf("create of hacks.ini failed\n");
					}
					fd = ISFS_Open("/title/00000001/00000002/data/hacks.ini", 1|2 );
					if( fd < 0 )
					{
						gprintf("hacks.ini open failure\n");
						fail=3;
					}

					if(ISFS_Write( fd, buf, size )<0)
					{
						gprintf("hacks.ini writing failure\n");
						fail = 4;
					}
					ISFS_Close( fd );
					free(buf);
				}

				s32 fd = ISFS_Open("/title/00000001/00000002/data/hacks_s.ini", 1|2 );

				if( fd >= 0 )
				{
					//File already exists, delete and recreate!
					ISFS_Close( fd );
					if(ISFS_Delete("/title/00000001/00000002/data/hacks_s.ini")<0)
					{
						gprintf("removal of hacks_s.ini failed.\n");
						fail = 5;
					}
				}

				if(ISFS_CreateFile("/title/00000001/00000002/data/hacks_s.ini", 0, 3, 3, 3)<0)
				{
					gprintf("hacks_s.ini creating failure\n");
					fail = 6;
				}
				fd = ISFS_Open("/title/00000001/00000002/data/hacks_s.ini", 1|2 );
				if( fd < 0 )
				{
					gprintf("hacks_s.ini open failure\n");
					fail=7;
				}
				if(ISFS_Write( fd, states, sizeof( u32 ) * hacks.size() )<0)
				{
					gprintf("hacks_s.ini writing failure\n");
					fail = 8;
				}

				ISFS_Close( fd );

				if( fail )
					PrintFormat( 0, 114, 480-48, "saving failed:%d", fail);
				else
					PrintFormat( 0, 118, 480-48, "settings saved");
			} 
			else 
			{

				s32 j = 0;
				u32 i = 0;
				for(i=0; i<hacks.size(); ++i)
				{
					if( hacks[i].version == SysVersion )
					{
						if( cur_off+menu_off == j++)
							break;
					}
				}

				//printf("\x1b[26;0Hi:%d,%d,%d\n", i, j, states[i] );
				//sleep(5);

				if(states[i])
					states[i]=0;
				else 
					states[i]=1;

				redraw = true;
			}
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_DOWN) || (PAD_Pressed & PAD_BUTTON_DOWN) )
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
		} else if ( (WPAD_Pressed & WPAD_BUTTON_UP) || (PAD_Pressed & PAD_BUTTON_UP) )
		{
			cur_off--;

			if( cur_off < 0 )
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
	
			redraw=true;
		}

		if( redraw )
		{
			//printf("\x1b[2;0HHackCount:%d DispCount:%d cur_off:%d menu_off:%d Hacks:%d   \n", HackCount, DispCount, cur_off, menu_off, hacks.size() );
			u32 j=0;
			u32 skip=menu_off;
			for( u32 i=0; i<hacks.size(); ++i)
			{
				if( hacks[i].version == SysVersion )
				{
					if( skip > 0 )
					{
						skip--;
					} else {
						//clear line
						for( u32 c=0; c<40; ++c)
							PrintFormat( 0, 16+c*6, 64+j*16, " ");

						PrintFormat( cur_off==j, 16, 64+j*16, "%s", hacks[i].desc );

						if( states[i] )
							PrintFormat( cur_off==j, 256, 64+j*16, "enabled ", hacks[i].desc);
						else
							PrintFormat( cur_off==j, 256, 64+j*16, "disabled", hacks[i].desc);
						
						j++;
					}
				}
				if( j >= 20 ) 
					break;
			}

			PrintFormat( cur_off==DispCount, 118, 480-64, "save settings");

			PrintFormat( 0, 114, 480-32, "                 ");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}

	return;
}
void SetSettings( void )
{
	
	//Load Setting
	LoadSettings();
	
	//get a list of all installed IOSs
	u32 TitleCount = 0;
	ES_GetNumTitles(&TitleCount);
	u64 *TitleIDs=(u64*)memalign(32, TitleCount * sizeof(u64) );
	ES_GetTitles(TitleIDs, TitleCount);

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

#ifdef DEBUG
		if ( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) )
		{
			exit(0);
		}
#endif
		if ( (WPAD_Pressed & WPAD_BUTTON_B) || (PAD_Pressed & PAD_BUTTON_B) )
			break;
		
		switch( cur_off )
		{
			case 0:
			{
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT) || (PAD_Pressed & PAD_BUTTON_LEFT) )
				{
					if( settings->autoboot == AUTOBOOT_DISABLED )
						settings->autoboot = AUTOBOOT_FILE;
					else
						settings->autoboot--;
					redraw=true;
				}else if ( (WPAD_Pressed & WPAD_BUTTON_RIGHT) || (PAD_Pressed & PAD_BUTTON_RIGHT) )
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
				if ( (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
					)
				{
					settings->ReturnTo++;
					if( settings->ReturnTo > RETURNTO_AUTOBOOT )
						settings->ReturnTo = RETURNTO_SYSMENU;

					redraw=true;
				} else if ( (WPAD_Pressed & WPAD_BUTTON_LEFT) || (PAD_Pressed & PAD_BUTTON_LEFT) ) {

					if( settings->ReturnTo == RETURNTO_SYSMENU )
						settings->ReturnTo = RETURNTO_AUTOBOOT;
					else
						settings->ReturnTo--;

					redraw=true;
				}


			} break;
			case 2:
			{
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
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
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
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
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
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
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
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
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
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
			case 7: //ignore ios reloading for system menu?
			{
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT)	||
					 (PAD_Pressed & PAD_BUTTON_LEFT)	||
					 (WPAD_Pressed & WPAD_BUTTON_RIGHT)	||
					 (PAD_Pressed & PAD_BUTTON_RIGHT)	||
					 (WPAD_Pressed & WPAD_BUTTON_A)		||
					 (PAD_Pressed & PAD_BUTTON_A)
					)
				{
					if( settings->UseSystemMenuIOS )
					{
						settings->UseSystemMenuIOS = false;
					}
					else
					{
						settings->UseSystemMenuIOS = true;
					}
					redraw=true;
				}
			}
			break;
			case 8:		//	System Menu IOS
			{
				if ( (WPAD_Pressed & WPAD_BUTTON_LEFT) || (PAD_Pressed & PAD_BUTTON_LEFT) )
				{
					while(1)
					{
						IOS_off++;
						if( IOS_off >= TitleCount )
							IOS_off = 0;
						if( (u32)(TitleIDs[IOS_off]>>32) == 0x00000001 && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 2  )
							break;
					}

					settings->SystemMenuIOS = (u32)(TitleIDs[IOS_off]&0xFFFFFFFF);

					redraw=true;
				} else if( (WPAD_Pressed & WPAD_BUTTON_RIGHT) || (PAD_Pressed & PAD_BUTTON_RIGHT) ) 
				{
					while(1)
					{
						IOS_off--;
						if( (signed)IOS_off <= 0 )
							IOS_off = TitleCount;
						if( (u32)(TitleIDs[IOS_off]>>32) == 0x00000001 && (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) > 2  )
							break;
					}

					settings->SystemMenuIOS = (u32)(TitleIDs[IOS_off]&0xFFFFFFFF);

					redraw=true;
				}

			} break;
			case 9:
			{
				if ( (WPAD_Pressed & WPAD_BUTTON_A) || (PAD_Pressed & PAD_BUTTON_A) )
				{
					if( SaveSettings() )
						PrintFormat( 0, 114, 256+96, "settings saved");
					else
						PrintFormat( 0, 118, 256+96, "saving failed");
				}
			} break;

			default:
				cur_off = 0;
				break;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_DOWN) || (PAD_Pressed & PAD_BUTTON_DOWN) )
		{
			cur_off++;
			if( (settings->UseSystemMenuIOS) && (cur_off == 8))
				cur_off++;
			if( cur_off >= 10)
				cur_off = 0;
			
			redraw=true;
		} else if ( (WPAD_Pressed & WPAD_BUTTON_UP) || (PAD_Pressed & PAD_BUTTON_UP) )
		{
			cur_off--;
			if( (settings->UseSystemMenuIOS) && (cur_off == 8))
				cur_off--;
			if( cur_off < 0 )
				cur_off = 9;
			
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
				case RETURNTO_PRELOADER:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Preloader  ");
				break;
				case RETURNTO_AUTOBOOT:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Autoboot   ");
				break;
			}
			
			//PrintFormat( 0, 16, 64, "Pos:%d", ((640/2)-(strlen("settings saved")*13/2))>>1);

			PrintFormat( cur_off==2, 0, 128+16, "           Shutdown to:          %s", settings->ShutdownToPreloader?"Preloader":"off      ");
			PrintFormat( cur_off==3, 0, 128+32, "             Stop disc:          %s", settings->StopDisc?"on ":"off");
			PrintFormat( cur_off==4, 0, 128+48, "   Light slot on error:          %s", settings->LidSlotOnError?"on ":"off");
			PrintFormat( cur_off==5, 0, 128+64, "Ignore standby setting:          %s", settings->IgnoreShutDownMode?"on ":"off");
			PrintFormat( cur_off==6, 0, 128+80, "      Background Color:          %s", settings->BlackBackground?"Black":"White");
			PrintFormat( cur_off==7, 0, 128+96, "   Use System Menu IOS:          %s", settings->UseSystemMenuIOS?"on ":"off");
			if(!settings->UseSystemMenuIOS)
			{
				PrintFormat( cur_off==8, 0, 128+112, "     IOS to use for SM:          %d  ", (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) );
			}
			else
			{
				PrintFormat( cur_off==8, 0, 128+112,	"                                        ");
			}
			PrintFormat( cur_off==9, 118, 128+144, "save settings");
			PrintFormat( 0, 114, 256+96, "                 ");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}
	free(TitleIDs);
	return;
}
void LoadHBC( void )
{
	//Note By DacoTaco : try and boot new (0x000100014A4F4449 - JODI) HBC id
	//if failed, boot old one(0x0001000148415858 - HAXX)
	u64 TitleID = 0x000100014A4F4449LL;
	u32 cnt ATTRIBUTE_ALIGN(32);
	ES_GetNumTicketViews(TitleID, &cnt);
	tikview *views = (tikview *)memalign( 32, sizeof(tikview)*cnt );
	if (ES_GetTicketViews(TitleID, views, cnt) < 0)
	{
		//title isnt there D: lets retry everything but with the old id...
		free(views);
		TitleID = 0x0001000148415858LL;
		ES_GetNumTicketViews(TitleID, &cnt);
		tikview *views = (tikview *)memalign( 32, sizeof(tikview)*cnt );
		ES_GetTicketViews(TitleID, views, cnt);
		ES_LaunchTitle(TitleID, &views[0]);
		free(views);
	}
	else
	{
		//new title found apparently :P
		ES_LaunchTitle(TitleID, &views[0]);
	}
	free(views);
}
void LoadBootMii( void )
{
	if (!RemountDevices())
	{
		PrintFormat( 1, ((640/2)-((strlen("Could not mount any FAT device"))*13/2))>>1, 208, "Could not mount any FAT device");
		sleep(5);
		return;
	}
	//when this was coded on 6th of Oct 2009 Bootmii was IOS 254
#ifndef libELM
	FILE* BootmiiFile = fopen("fat:/bootmii/armboot.bin","r");
#else
	FILE* BootmiiFile = fopen("elm:/sd/bootmii/armboot.bin","r");
#endif
	if (!BootmiiFile)
	{
		PrintFormat( 1, ((640/2)-((strlen("Could not find fat:/bootmii/armboot.bin"))*13/2))>>1, 208, "Could not find fat:/bootmii/armboot.bin");
		sleep(5);
		return;
	}
	else
	{
		fclose(BootmiiFile);
#ifndef libELM
		BootmiiFile = fopen("fat:/bootmii/ppcboot.elf","r");
#else
		BootmiiFile = fopen("elm:/sd/bootmii/ppcboot.elf","r");
#endif
		if(!BootmiiFile)
		{
			PrintFormat( 1, ((640/2)-((strlen("Could not find fat:/bootmii/ppcboot.elf"))*13/2))>>1, 208, "Could not find fat:/bootmii/ppcboot.elf");
			sleep(5);
			return;
		}
	}
	fclose(BootmiiFile);
	u16 currentIOS = IOS_GetVersion();
	__IOS_LaunchNewIOS(254);
	//launching bootmii failed. lets wait a bit for the launch and then load the other ios back
	sleep(5);
	__IOS_LaunchNewIOS(currentIOS);
}
void BootMainSysMenu( void )
{
	static u64 TitleID ATTRIBUTE_ALIGN(32)=0x0000000100000002LL;
	static u32 tmd_size ATTRIBUTE_ALIGN(32);
	static u32 tempKeyID ATTRIBUTE_ALIGN(32);
	s32 r = 0;

	ISFS_Deinitialize();
	if( ISFS_Initialize() < 0 )
	{
		//printf("ISFS_Initialize() failed!\n");
		PrintFormat( 1, ((640/2)-((strlen("ISFS_Initialize() failed!"))*13/2))>>1, 208, "ISFS_Initialize() failed!" );
		sleep( 5 );
		return;
	}

	//read ticket from FS
	s32 fd = ISFS_Open("/title/00000001/00000002/content/ticket", 1 );
	if( fd < 0 )
	{
		error = ERROR_SYSMENU_TIKNOTFOUND;
		return;
	}

	//get size
	fstats * tstatus = (fstats*)memalign( 32, sizeof( fstats ) );
	r = ISFS_GetFileStats( fd, tstatus );
	if( r < 0 )
	{
		ISFS_Close( fd );
		error = ERROR_SYSMENU_TIKSIZEGETFAILED;
		return;
	}

	//create buffer
	char * buf = (char*)memalign( 32, (tstatus->file_length+32)&(~31) );
	if( buf == NULL )
	{
		ISFS_Close( fd );
		error = ERROR_MALLOC;
		return;
	}
	memset(buf, 0, (tstatus->file_length+32)&(~31) );

	//read file
	r = ISFS_Read( fd, buf, tstatus->file_length );
	if( r < 0 )
	{
		free( buf );
		ISFS_Close( fd );
		error = ERROR_SYSMENU_TIKREADFAILED;
		return;
	}

	ISFS_Close( fd );

	r=ES_GetStoredTMDSize(TitleID, &tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMDSize(%llX, %08X):%d\n", TitleID, (u32)(&tmd_size), r );
	sleep(1);
#endif
	if(r<0)
	{
		free( buf );
		error = ERROR_SYSMENU_GETTMDSIZEFAILED;
		return;
	}

	signed_blob *TMD = (signed_blob *)memalign( 32, (tmd_size+32)&(~31) );
	if( TMD == NULL )
	{
		free( buf );
		error = ERROR_MALLOC;
		return;
	}
	memset(TMD, 0, tmd_size);

	r=ES_GetStoredTMD(TitleID, TMD, tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMD(%llX, %08X, %d):%d\n", TitleID, (u32)(TMD), tmd_size, r );
	sleep(1);
#endif
	if(r<0)
	{
		free( TMD );
		free( buf );
		error = ERROR_SYSMENU_GETTMDFAILED;
		return;
	}
	
	tmd *rTMD = (tmd *)(TMD+(0x140/sizeof(tmd *)));
#ifdef DEBUG
	printf("num_contents:%08X\n", rTMD->num_contents );
#endif

	//get main.dol filename

	u32 fileID = 0;
	for(u32 z=0; z < rTMD->num_contents; ++z)
	{
		if( rTMD->contents[z].index == rTMD->boot_index )
		{
#ifdef DEBUG
			printf("%d:%d\n", rTMD->contents[z].index, rTMD->contents[z].cid);
#endif
			fileID = rTMD->contents[z].cid;
			break;
		}
	}

	if( fileID == 0 )
	{
		free( TMD );
		free( buf );
		error = ERROR_SYSMENU_BOOTNOTFOUND;
		return;
	}


	char * file = (char*)memalign( 32, 256 );
	if( file == NULL )
	{
		free( TMD );
		free( buf );
		error = ERROR_MALLOC;
		return;
	}

	memset(file, 0, 256 );

	sprintf( file, "/title/00000001/00000002/content/%08x.app", fileID );
	//small fix that Phpgeek didn't forget but i did
	file[33] = '1'; // installing preloader renamed system menu so we change the app file to have the right name

	fd = ISFS_Open( file, 1 );
#ifdef DEBUG
	printf("IOS_Open(%s, %d):%d\n", file, 1, fd );
	sleep(1);
#endif
	if( fd < 0 )
	{
		free( TMD );
		free( buf );
		free( file );
		ISFS_Close( fd );
		error = ERROR_SYSMENU_BOOTOPENFAILED;
		return;
	}

	fstats *status = (fstats *)memalign(32, sizeof( fstats ) );
	if( status == NULL )
	{
		free( TMD );
		free( buf );
		free( file );
		ISFS_Close( fd );
		error = ERROR_MALLOC;
		return;
	}

	r = ISFS_GetFileStats( fd, status);
#ifdef DEBUG
	printf("ISFS_GetFileStats(%d, %08X):%d\n", fd, status, r );
	sleep(1);
#endif
	if( fd < 0 )
	{
		free( TMD );
		free( buf );
		free( file );
		ISFS_Close( fd );
		error = ERROR_SYSMENU_BOOTGETSTATS;
		return;
	}
#ifdef DEBUG
	printf("size:%d\n", status->file_length);
#endif
	dolhdr *hdr = (dolhdr *)memalign(32, (sizeof( dolhdr )+32)&(~31) );
	memset( hdr, 0, (sizeof( dolhdr )+32)&(~31) );
	
	ISFS_Seek( fd, 0, SEEK_SET );
	r = ISFS_Read( fd, hdr, sizeof(dolhdr) );
#ifdef DEBUG
	printf("ISFS_Read(%d, %08X, %d):%d\n", fd, hdr, sizeof(dolhdr), r );
	sleep(1);
#endif
	if( r < 0 )
		return;

	if( hdr->entrypoint != 0x3400 )
	{
#ifdef DEBUG
		printf("BOGUS entrypoint:%08X\n", hdr->entrypoint );
		sleep(5);
#endif
		ISFS_Close( fd );
		free(hdr);
		return;
	}

	void	(*entrypoint)();

	int i=0;
	for (i = 0; i < 6; i++)
	{
		if( hdr->sizeText[i] && hdr->addressText[i] && hdr->offsetText[i] )
		{
			ICInvalidateRange((void*)(hdr->addressText[i]), hdr->sizeText[i]);
#ifdef DEBUG
			printf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetText[i]), hdr->addressText[i], hdr->sizeText[i]);
#endif
			if( (((hdr->addressText[i])&0xF0000000) != 0x80000000) || (hdr->sizeText[i]>(10*1024*1024)) )
			{
#ifdef DEBUG
				printf("BOGUS offsets!\n");
				sleep(5);
#endif
				return;
			}

			ISFS_Seek( fd, hdr->offsetText[i], SEEK_SET );
			ISFS_Read( fd, (void*)(hdr->addressText[i]), hdr->sizeText[i] );

			DCFlushRange((void*)(hdr->addressText[i]), hdr->sizeText[i]);
		}
	}

	// data sections
	for (i = 0; i <= 10; i++)
	{
		if( hdr->sizeData[i] && hdr->addressData[i] && hdr->offsetData[i] )
		{
			ICInvalidateRange((void*)(hdr->addressData[i]), hdr->sizeData[i]);
#ifdef DEBUG
			printf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetData[i]), hdr->addressData[i], hdr->sizeData[i]);
#endif
			if( (((hdr->addressData[i])&0xF0000000) != 0x80000000) || (hdr->sizeData[i]>(10*1024*1024)) )
			{
#ifdef DEBUG
				printf("BOGUS offsets!\n");
				sleep(5);
#endif
				return ;
			}

			ISFS_Seek( fd, hdr->offsetData[i], SEEK_SET );
			ISFS_Read( fd, (void*)(hdr->addressData[i]), hdr->sizeData[i] );

			DCFlushRange((void*)(hdr->addressData[i]), hdr->sizeData[i]);
		}

	}

	entrypoint = (void (*)())(hdr->entrypoint);
	gprintf("entrypoint: %08X\n", entrypoint );

	LoadHacks();
	WPAD_Shutdown();
	if( !SGetSetting( SETTING_USESYSTEMMENUIOS ) )
	{
		if ((s32)SGetSetting(SETTING_SYSTEMMENUIOS) != IOS_GetVersion())
		{
			__ES_Close();
			__ES_Init();
			__IOS_LaunchNewIOS(SGetSetting(SETTING_SYSTEMMENUIOS));
			gprintf("launched ios %d for system menu\n",IOS_GetVersion());
			//__IOS_LaunchNewIOS(rTMD->sys_version);
			//__IOS_LaunchNewIOS(249);
			__IOS_InitializeSubsystems();
			r = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)TMD, tmd_size, (signed_blob *)buf, tstatus->file_length, &tempKeyID);
			if( r < 0 )
			{	error=ERROR_SYSMENU_ESDIVERFIY_FAILED;
				__IOS_ShutdownSubsystems();
				__IOS_LaunchNewIOS(rTMD->sys_version);
				__IOS_InitializeSubsystems();
				WPAD_Init();
				return;
			}
		}
		else
		{
			gprintf("set to use the same ios as system ios. skipping reload...\n");
		}
	}
	//ES_SetUID(TitleID);
	free(TMD);
	free( status );
	free( tstatus );
	free( buf );

	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed

	gprintf("Hacks:%d\n", hacks.size() );
	//Apply patches
	for( u32 i=0; i<hacks.size(); ++i)
	{
#ifdef DEBUG
		printf("i:%d state:%d version:%d\n", i, states[i], hacks[i].version);
#endif
		if( states[i] == 1 )
		{
			if( hacks[i].version != rTMD->title_version )
				continue;

			for( u32 z=0; z < hacks[i].value.size(); ++z )
			{
#ifdef DEBUG
				printf("%08X:%08X\n", hacks[i].offset[z], hacks[i].value[z] );
#endif
				*(vu32*)(hacks[i].offset[z]) = hacks[i].value[z];
				DCFlushRange((void*)(hacks[i].offset[z]), 4);
			}
		}
	}
#ifdef DEBUG
	sleep(20);
#endif
	__io_wiisd.shutdown();
	__STM_Close();
	__IOS_ShutdownSubsystems();
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	_unstub_start();

}
void InstallLoadDOL( void )
{
	char filename[MAXPATHLEN],filepath[MAXPATHLEN];
	std::vector<char*> names;
	
	struct stat st;
	DIR_ITER* dir;

	if (!RemountDevices() )
	{
		PrintFormat( 1, ((640/2)-((strlen("NO fat device found found!"))*13/2))>>1, 208, "NO fat device found found!");
		sleep(5);
		return;
	}
#ifndef libELM
	dir = diropen ("fat:/");
#else
	dir = diropen("elm:/sd/");
#endif
	if( dir == NULL )
	{
		PrintFormat( 1, ((640/2)-((strlen("Failed to open root of Device!"))*13/2))>>1, 208, "Failed to open root of Device!");
		sleep(5);
		return;
	}
	//get all files names
	while( dirnext (dir, filename, &st) != -1 )
	{
		if( (strstr( filename, ".dol") != NULL) ||
			(strstr( filename, ".DOL") != NULL) ||
			(strstr( filename, ".elf") != NULL) ||
			(strstr( filename, ".ELF") != NULL) )
		{
			names.resize( names.size() + 1 );
			names[names.size()-1] = new char[strlen(filename)+1];
			memcpy( names[names.size()-1], filename, strlen( filename ) + 1 );
		}
	}

	dirclose( dir );

	if( names.size() == 0 )
	{
		PrintFormat( 1, ((640/2)-((strlen("Couldn't find any executable files"))*13/2))>>1, 208, "Couldn't find any executable files");
		PrintFormat( 1, ((640/2)-((strlen("in the root of the FAT device!"))*13/2))>>1, 228, "in the root of the FAT device!");
		sleep(5);
		return;
	}

	u32 redraw = 1;
	s32 cur_off= 0;

	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
 
#ifdef DEBUG
		if ( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) )
			exit(0);
#endif
		if ( (WPAD_Pressed & WPAD_BUTTON_B) || (PAD_Pressed & PAD_BUTTON_B) )
		{
			break;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_A) || (PAD_Pressed & PAD_BUTTON_A) )
		{
			ClearScreen();
			//Install file
#ifdef libELM
			sprintf(filepath, "elm:/sd/%s",names[cur_off]);
			FILE *dol = fopen(filepath, "rb" );
#else
			sprintf(filepath, "fat:/%s",names[cur_off]);
			FILE *dol = fopen(filepath, "rb" );
#endif
			if( dol == NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Could not open:\"%s\" for reading")+strlen(names[cur_off]))*13/2))>>1, 208, "Could not open:\"%s\" for reading", names[cur_off]);
				sleep(5);
				break;
			}
			PrintFormat( 0, ((640/2)-((strlen("Installing \"%s\"...")+strlen(names[cur_off]))*13/2))>>1, 208, "Installing \"%s\"...", names[cur_off]);

			//get size
			fseek( dol, 0, SEEK_END );
			unsigned int size = ftell( dol );
			fseek( dol, 0, 0 );

			char *buf = (char*)memalign( 32, sizeof( char ) * size );
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

			if( ISFS_Write( fd, buf, sizeof( char ) * size ) != sizeof( char ) * size )
			{
				PrintFormat( 1, ((640/2)-((strlen("Writing file failed!"))*13/2))>>1, 240, "Writing file failed!");
			} else {
				PrintFormat( 0, ((640/2)-((strlen("\"%s\" installed")+strlen(names[cur_off]))*13/2))>>1, 240, "\"%s\" installed", names[cur_off]);
			}

			sleep(5);
			ClearScreen();
			redraw=true;
			ISFS_Close( fd );
			free( buf );

		}

		if ( (WPAD_Pressed & WPAD_BUTTON_1) || (PAD_Pressed & PAD_TRIGGER_Z) )
		{
			ClearScreen();

			//Load dol
#ifdef libELM
			sprintf(filepath, "elm:/sd/%s", names[cur_off]);
			FILE *dol = fopen(filepath, "rb" );
#else
			sprintf(filepath, "fat:/%s", names[cur_off]);
			FILE *dol = fopen(filepath,"rb");
#endif
			gprintf("laoding %s\n",names[cur_off]);
			if( dol == NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Could not open:\"%s\" for reading")+strlen(names[cur_off]))*13/2))>>1, 208, "Could not open:\"%s\" for reading", names[cur_off]);
				sleep(5);
				break;
			}
			PrintFormat( 0, ((640/2)-((strlen("Loading file...")+strlen(names[cur_off]))*13/2))>>1, 208, "Loading file...", names[cur_off]);
			void	(*entrypoint)();

			Elf32_Ehdr ElfHdr;

			fread( &ElfHdr, sizeof( ElfHdr ), 1, dol );

			if( ElfHdr.e_ident[EI_MAG0] == 0x7F ||
				ElfHdr.e_ident[EI_MAG1] == 'E' ||
				ElfHdr.e_ident[EI_MAG2] == 'L' ||
				ElfHdr.e_ident[EI_MAG3] == 'F' )
			{
				gprintf("ELF Found\n");
#ifdef DEBUG
				gprintf("Type:      \t%04X\n", ElfHdr.e_type );
				gprintf("Machine:   \t%04X\n", ElfHdr.e_machine );
				gprintf("Version:  %08X\n", ElfHdr.e_version );
				gprintf("Entry:    %08X\n", ElfHdr.e_entry );
				gprintf("Flags:    %08X\n", ElfHdr.e_flags );
				gprintf("EHsize:    \t%04X\n\n", ElfHdr.e_ehsize );

				gprintf("PHoff:    %08X\n",	ElfHdr.e_phoff );
				gprintf("PHentsize: \t%04X\n",	ElfHdr.e_phentsize );
				gprintf("PHnum:     \t%04X\n\n",ElfHdr.e_phnum );

				gprintf("SHoff:    %08X\n",	ElfHdr.e_shoff );
				gprintf("SHentsize: \t%04X\n",	ElfHdr.e_shentsize );
				gprintf("SHnum:     \t%04X\n",	ElfHdr.e_shnum );
				gprintf("SHstrndx:  \t%04X\n\n",ElfHdr.e_shstrndx );
#endif

				if( ElfHdr.e_phnum == 0 )
				{
#ifdef DEBUG
					printf("Warning program header entries are zero!\n");
#endif
				} else {

					for( int i=0; i < ElfHdr.e_phnum; ++i )
					{
						fseek( dol, ElfHdr.e_phoff + sizeof( Elf32_Phdr ) * i, SEEK_SET );

						Elf32_Phdr phdr;
						fread( &phdr, sizeof( phdr ), 1, dol );

#ifdef DEBUG
						printf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz );
#endif
						fseek( dol, phdr.p_offset, 0 );
						fread( (void*)(phdr.p_vaddr | 0x80000000), sizeof( u8 ), phdr.p_filesz, dol);
					}
				}

				if( ElfHdr.e_shnum == 0 )
				{
#ifdef DEBUG
					printf("Warning section header entries are zero!\n");
#endif
				} else {

					for( int i=0; i < ElfHdr.e_shnum; ++i )
					{
						fseek( dol, ElfHdr.e_shoff + sizeof( Elf32_Shdr ) * i, SEEK_SET );

						Elf32_Shdr shdr;
						fread( &shdr, sizeof( shdr ), 1, dol );

						if( shdr.sh_type == 0 )
							continue;

#ifdef DEBUG
						if( shdr.sh_type > 17 )
							printf("Warning the type: %08X could be invalid!\n", shdr.sh_type );

						if( shdr.sh_flags & ~0xF0000007 )
							printf("Warning the flag: %08X is invalid!\n", shdr.sh_flags );

						printf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr.sh_type, shdr.sh_offset, shdr.sh_name, shdr.sh_addr, shdr.sh_size );
#endif
						fseek( dol, shdr.sh_offset, 0 );
						fread( (void*)(shdr.sh_addr | 0x80000000), sizeof( u8 ), shdr.sh_size, dol);
					}
				}

				entrypoint = (void (*)())(ElfHdr.e_entry | 0x80000000);

				//sleep(20);
				//return;

			} else {
			//	printf("DOL found\n");

				//Load the dol!, TODO: maybe add sanity checks?
				//read the header
				dolhdr hdr;
				fseek( dol, 0, 0);
				fread( &hdr, sizeof( dolhdr ), 1, dol );

				//printf("\nText Sections:\n");

				int i=0;
				for (i = 0; i < 6; i++)
				{
					if( hdr.sizeText[i] && hdr.addressText[i] && hdr.offsetText[i] )
					{
						DCInvalidateRange( (void*)(hdr.addressText[i]), hdr.sizeText[i] );

						fseek( dol, hdr.offsetText[i], SEEK_SET );
						fread( (void*)(hdr.addressText[i]), sizeof( char ), hdr.sizeText[i], dol );

						//printf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr.offsetText[i]), hdr.addressText[i], hdr.sizeText[i]);
					}
				}

				//printf("\nData Sections:\n");

				// data sections
				for (i = 0; i <= 10; i++)
				{
					if( hdr.sizeData[i] && hdr.addressData[i] && hdr.offsetData[i] )
					{
						fseek( dol, hdr.offsetData[i], SEEK_SET );
						fread( (void*)(hdr.addressData[i]), sizeof( char ), hdr.sizeData[i], dol );

						DCFlushRangeNoSync( (void*)(hdr.addressData[i]), hdr.sizeData[i] );
						//printf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr.offsetData[i]), hdr.addressData[i], hdr.sizeData[i]);
					}
				}

				memset ((void *) hdr.addressBSS, 0, hdr.sizeBSS);
				DCFlushRange((void *) hdr.addressBSS, hdr.sizeBSS);
				fclose( dol );
				entrypoint = (void (*)())(hdr.entrypoint);
			}

			__STM_Close();
			__io_wiisd.shutdown();
			__IOS_ShutdownSubsystems();
			WPAD_Shutdown();
			mtmsr(mfmsr() & ~0x8000);
			mtmsr(mfmsr() | 0x2002);
			ICSync();
			entrypoint();		

			//u32 level;
			//__IOS_ShutdownSubsystems();
			//_CPU_ISR_Disable (level);
			//mtmsr(mfmsr() & ~0x8000);
			//mtmsr(mfmsr() | 0x2002);
			//entrypoint();
			//_CPU_ISR_Restore (level);


			//Shutdown everything
			__STM_Close();
			ISFS_Deinitialize();
			__io_wiisd.shutdown();
			WPAD_Shutdown();
			__IOS_ShutdownSubsystems();
			//IOS_ReloadIOS(IOS_GetPreferredVersion());
			//__ES_Init();
			//__IOS_LaunchNewIOS(30);
			//__IOS_InitializeSubsystems();
			//__ES_Close();
			mtmsr(mfmsr() & ~0x8000);
			mtmsr(mfmsr() | 0x2002);
			//sleep(5);
			ICSync();
			gprintf("Entrypoint: %08X\n", (u32)(entrypoint) );
			//sleep(1);
			entrypoint();
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_DOWN) || (PAD_Pressed & PAD_BUTTON_DOWN) )
		{
			cur_off++;

			if( cur_off >= names.size())
				cur_off = 0;
			
			redraw=true;
		} else if ( (WPAD_Pressed & WPAD_BUTTON_UP) || (PAD_Pressed & PAD_BUTTON_UP) )
		{
			cur_off--;

			if( cur_off < 0 )
				cur_off=names.size()-1;
			
			redraw=true;
		}

		if( redraw )
		{
			for( u32 i=0; i<names.size(); ++i )
				PrintFormat( cur_off==i, 16, 64+i*16, "%s", names[i]);

			PrintFormat( 0, 33, 480-64, "press A to install, 1(Z) to load a file");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}

	//free memory
	for( u32 i=0; i<names.size(); ++i )
		delete names[i];
	names.clear();

	return;
}
void AutoBootDol( void )
{
	s32 fd = ISFS_Open("/title/00000001/00000002/data/main.bin", 1 );
	if( fd < 0 )
	{
		error = ERROR_BOOT_DOL_OPEN;
		return;
	}

	void	(*entrypoint)();

	Elf32_Ehdr *ElfHdr = (Elf32_Ehdr *)memalign( 32, (sizeof( Elf32_Ehdr )+32)&(~31) );
	if( ElfHdr == NULL )
	{
		error = ERROR_MALLOC;
		return;
	}

	s32 r = ISFS_Read( fd, ElfHdr, sizeof( Elf32_Ehdr ) );
	if( r < 0 || r != sizeof( Elf32_Ehdr ) )
	{
#ifdef DEBUG
		sleep(10);
#endif
		error = ERROR_BOOT_DOL_READ;
		return;
	}

	if( ElfHdr->e_ident[EI_MAG0] == 0x7F ||
		ElfHdr->e_ident[EI_MAG1] == 'E' ||
		ElfHdr->e_ident[EI_MAG2] == 'L' ||
		ElfHdr->e_ident[EI_MAG3] == 'F' )
	{
		gprintf("ELF Found\n");
#ifdef DEBUG
		gprintf("Type:      \t%04X\n", ElfHdr->e_type );
		gprintf("Machine:   \t%04X\n", ElfHdr->e_machine );
		gprintf("Version:  %08X\n", ElfHdr->e_version );
		gprintf("Entry:    %08X\n", ElfHdr->e_entry );
		gprintf("Flags:    %08X\n", ElfHdr->e_flags );
		gprintf("EHsize:    \t%04X\n\n", ElfHdr->e_ehsize );

		gprintf("PHoff:    %08X\n",	ElfHdr->e_phoff );
		gprintf("PHentsize: \t%04X\n",	ElfHdr->e_phentsize );
		gprintf("PHnum:     \t%04X\n\n",ElfHdr->e_phnum );

		gprintf("SHoff:    %08X\n",	ElfHdr->e_shoff );
		gprintf("SHentsize: \t%04X\n",	ElfHdr->e_shentsize );
		gprintf("SHnum:     \t%04X\n",	ElfHdr->e_shnum );
		gprintf("SHstrndx:  \t%04X\n\n",ElfHdr->e_shstrndx );
#endif

		if( ElfHdr->e_phnum == 0 )
		{
#ifdef DEBUG
			printf("Warning program header entries are zero!\n");
#endif
		} else {

			for( int i=0; i < ElfHdr->e_phnum; ++i )
			{
				r = ISFS_Seek( fd, ElfHdr->e_phoff + sizeof( Elf32_Phdr ) * i, SEEK_SET );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}

				Elf32_Phdr *phdr = (Elf32_Phdr *)memalign( 32, (sizeof( Elf32_Phdr )+32)&(~31) );
				r = ISFS_Read( fd, phdr, sizeof( Elf32_Phdr ) );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_READ;
					return;
				}
#ifdef DEBUG
				gprintf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz );
#endif
				r = ISFS_Seek( fd, phdr->p_offset, 0 );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}

				//DacoTaco : hacky check, i know
				if ( (phdr->p_vaddr != 0) && (phdr->p_filesz != 0) )
				{
					//Check if target address is aligned by 32, otherwise create a temp buffer and load it from there!
					if( phdr->p_vaddr&(~31))
					{
						u8 *tbuf = (u8*)memalign(32, (phdr->p_filesz+32)&(~31) );

						r = ISFS_Read( fd, tbuf, phdr->p_filesz);
						if( r < 0 )
						{
#ifdef DEBUG
							sleep(10);
#endif
							gprintf("read failed of the program section addr(%u). error 1.%d.%u\n",phdr->p_vaddr,r,phdr->p_filesz);
							error = ERROR_BOOT_DOL_READ;
							return;
						}

						memcpy( (void*)(phdr->p_vaddr | 0x80000000), tbuf, phdr->p_filesz );

						free( tbuf);
					} else {

						r = ISFS_Read( fd, (void*)(phdr->p_vaddr | 0x80000000), phdr->p_filesz);
						if( r < 0 )
						{
#ifdef DEBUG
							sleep(10);
#endif
							gprintf("read failed of the program section addr(%u). error 2.%d.%u\n",phdr->p_vaddr,r,phdr->p_filesz);
							error = ERROR_BOOT_DOL_READ;
							return;
						}
					}
				}
				else
				{
					gprintf("warning! program section nr %d address is 0!(%u - %u)\n",i,phdr->p_vaddr, phdr->p_filesz);
				}

				free( phdr );
			}
		}
		if( ElfHdr->e_shnum == 0 )
		{
#ifdef DEBUG
			printf("Warning section header entries are zero!\n");
#endif
		} else {

			Elf32_Shdr *shdr = (Elf32_Shdr *)memalign( 32, (sizeof( Elf32_Shdr )+32)&(~31) );

			for( int i=0; i < ElfHdr->e_shnum; ++i )
			{
				r = ISFS_Seek( fd, ElfHdr->e_shoff + sizeof( Elf32_Shdr ) * i, SEEK_SET );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}

				r = ISFS_Read( fd, shdr, sizeof( Elf32_Shdr ) );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_READ;
					return;
				}

				if( shdr->sh_type == 0 || shdr->sh_size == 0 )
					continue;

#ifdef DEBUG
				if( shdr->sh_type > 17 )
					printf("Warning the type: %08X could be invalid!\n", shdr->sh_type );

				if( shdr->sh_flags & ~0xF0000007 )
					printf("Warning the flag: %08X is invalid!\n", shdr->sh_flags );

				printf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr->sh_type, shdr->sh_offset, shdr->sh_name, shdr->sh_addr, shdr->sh_size );
#endif
				r = ISFS_Seek( fd, shdr->sh_offset, 0 );
				if( r < 0 )
				{
#ifdef DEBUG
					sleep(10);
#endif
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}


				//Check if target address is aligned by 32, otherwise create a temp buffer and load it from there!
				if( (shdr->sh_addr == 0) || shdr->sh_addr&(~31) )
				{
					u8 *tbuf = (u8*)memalign(32, (shdr->sh_size+32)&(~31) );

					r = ISFS_Read( fd, tbuf, shdr->sh_size);
					if( r < 0 )
					{
#ifdef DEBUG
						sleep(10);
#endif
						gprintf("error reading file, error code 5.%d\n",r);
						error = ERROR_BOOT_DOL_READ;
						return;
					}

					memcpy( (void*)(shdr->sh_addr | 0x80000000), tbuf, shdr->sh_size );

					free( tbuf);
				} else {

					r = ISFS_Read( fd, (void*)(shdr->sh_addr | 0x80000000), shdr->sh_size);
					if( r < 0 )
					{
#ifdef DEBUG
						sleep(10);
#endif
						gprintf("error reading file, error code 6.%d\n",r);
						error = ERROR_BOOT_DOL_READ;
						return;
					}
				}

			}
			free( shdr );
		}

		ISFS_Close( fd );
		entrypoint = (void (*)())(ElfHdr->e_entry | 0x80000000);

		//sleep(20);
		//return;

	} else {
	//	printf("DOL found\n");

		//Load the dol!, TODO: maybe add sanity checks?

		//read the header
		dolhdr *hdr = (dolhdr *)memalign(32, (sizeof( dolhdr )+32)&(~31) );
		if( hdr == NULL )
		{
			error = ERROR_MALLOC;
			return;
		}

		s32 r = ISFS_Seek( fd, 0, 0);
		if( r < 0 )
		{
			gprintf("ISFS_Read failed:%d\n", r);
			//sleep(5);
			//exit(0);
			error = ERROR_BOOT_DOL_SEEK;
			return;
		}

		r = ISFS_Read( fd, hdr, sizeof(dolhdr) );

		if( r < 0 || r != sizeof(dolhdr) )
		{
			gprintf("ISFS_Read failed:%d\n", r);
			//sleep(5);
			//exit(0);
			error = ERROR_BOOT_DOL_READ;
			return;
		}

		//printf("read:%d\n", r );

		gprintf("\nText Sections:\n");

		int i=0;
		for (i = 0; i < 6; i++)
		{
			if( hdr->sizeText[i] && hdr->addressText[i] && hdr->offsetText[i] )
			{
				if(ISFS_Seek( fd, hdr->offsetText[i], SEEK_SET )<0)
				{
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}
				
				//if( hdr->addressText[i] & (~31) )
				//{
				//	u8 *tbuf = (u8*)memalign(32, (hdr->sizeText[i]+32)&(~31) );

				//	ISFS_Read( fd, tbuf, hdr->sizeText[i]);

				//	memcpy( (void*)(hdr->addressText[i]), tbuf, hdr->sizeText[i] );

				//	free( tbuf);

				//} else {
					if(ISFS_Read( fd, (void*)(hdr->addressText[i]), hdr->sizeText[i] )<0)
					{
						error = ERROR_BOOT_DOL_READ;
						return;
					}
				//}
				DCInvalidateRange( (void*)(hdr->addressText[i]), hdr->sizeText[i] );

				gprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetText[i]), hdr->addressText[i], hdr->sizeText[i]);
			}
		}

		gprintf("\nData Sections:\n");

		// data sections
		for (i = 0; i <= 10; i++)
		{
			if( hdr->sizeData[i] && hdr->addressData[i] && hdr->offsetData[i] )
			{
				if(ISFS_Seek( fd, hdr->offsetData[i], SEEK_SET )<0)
				{
					error = ERROR_BOOT_DOL_SEEK;
					return;
				}
				
				//if( hdr->addressData[i] & (~31) )
				//{
				//	u8 *tbuf = (u8*)memalign(32, (hdr->sizeData[i]+32)&(~31) );

				//	ISFS_Read( fd, tbuf, hdr->sizeData[i]);

				//	memcpy( (void*)(hdr->addressData[i]), tbuf, hdr->sizeData[i] );

				//	free( tbuf);

				//} else {
					if( ISFS_Read( fd, (void*)(hdr->addressData[i]), hdr->sizeData[i] )<0)
					{
						error = ERROR_BOOT_DOL_READ;
						return;
					}
				//}

				DCInvalidateRange( (void*)(hdr->addressData[i]), hdr->sizeData[i] );

				gprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetData[i]), hdr->addressData[i], hdr->sizeData[i]);
			}
		}

		entrypoint = (void (*)())(hdr->entrypoint);

	}
	if( entrypoint == 0x00000000 )
	{
		error = ERROR_BOOT_DOL_ENTRYPOINT;
		return;
	}
	for(int i=0;i<WPAD_MAX_WIIMOTES;i++) {
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}
	WPAD_Shutdown();
	gprintf("Entrypoint: %08X\n", (u32)(entrypoint) );
	//sleep(1);
	//Shutdown everything
	__STM_Close();
	ISFS_Deinitialize();
	__io_wiisd.shutdown();
	__IOS_ShutdownSubsystems();
	//IOS_ReloadIOS(IOS_GetPreferredVersion());
	//__ES_Init();
	//__IOS_LaunchNewIOS(30);
	//__IOS_InitializeSubsystems();
	//__ES_Close();
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	//sleep(5);
	ICSync();
	//printf("Entrypoint: %08X\n", (u32)(entrypoint) );
	//sleep(1);
	entrypoint();
	
	return;
}
void HandleWiiMoteEvent(s32 chan)
{
	Shutdown=1;
}
void HandleSTMEvent(u32 event)
{
	switch(event)
	{
		default:
		case STM_EVENT_RESET:
			break;
		case STM_EVENT_POWER:
			Shutdown=1;
			break;
	}
}
void DVDStopDisc( void )
{
	s32 di_fd = IOS_Open("/dev/di",0);

	u8 *inbuf = (u8*)memalign( 32, 0x20 );
	u8 *outbuf = (u8*)memalign( 32, 0x20 );

	memset(inbuf, 0, 0x20 );
	memset(outbuf, 0, 0x20 );

	((u32*)inbuf)[0x00] = 0xE3000000;
	((u32*)inbuf)[0x01] = 0;
	((u32*)inbuf)[0x02] = 0;

	DCFlushRange(inbuf, 0x20);
	IOS_IoctlAsync( di_fd, 0xE3, inbuf, 0x20, outbuf, 0x20, NULL, NULL);
	IOS_Close(di_fd);

	free( outbuf );
	free( inbuf );
}
int main(int argc, char **argv)
{
	CheckForGecko();
	gprintf("priiloader\n");
	gprintf("Built   : %s %s\n", __DATE__, __TIME__ );
	gprintf("Version : %d.%d (rev %s)\n", VERSION>>16, VERSION&0xFFFF, SVN_REV_STR);
	gprintf("Firmware: %d.%d.%d\n", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

	*(vu32*)0x80000020 = 0x0D15EA5E;				// Magic word (how did the console boot?)
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

	*(vu32*)0x80003120 = 0x93400000;				// MEM2 end address ?

	s32 r = ISFS_Initialize();
	if( r < 0 )
	{
		*(vu32*)0xCD8000C0 |= 0x20;
		error=ERROR_ISFS_INIT;
	}
	//do video init first so we can see the bloody crash screens
	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth*VI_DISPLAY_PIX_SZ );

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	LoadSettings();
	s16 Bootstate = CheckBootState();
	gprintf("BootState:%d\n", Bootstate );
	if( ClearState() < 0 )
	{
		gprintf("failed to clear state\n");
		error = ERROR_STATE_CLEAR;
	}
	gprintf("\"Magic Priiloader word\": %x\n",*(vu32*)0x8132FFFB);
	//Check reset button state or magic word
	if( ((*(vu32*)0xCC003000)>>16)&1 && *(vu32*)0x8132FFFB != 0x4461636f) //0x4461636f = "Daco" in hex
	{
		//Check autoboot settings
		switch( Bootstate )
		{
			case 5:
				ClearState();
				if(!SGetSetting(SETTING_SHUTDOWNTOPRELOADER))
				{
					*(vu32*)0xCD8000C0 &= ~0x20;
					DVDStopDisc();
        			WPAD_Shutdown();
					ShutdownDevices();

					if( SGetSetting(SETTING_IGNORESHUTDOWNMODE) )
					{
						STM_ShutdownToStandby();

					} else {
						if( CONF_GetShutdownMode() == CONF_SHUTDOWN_STANDBY )
							STM_ShutdownToStandby();
						else
							STM_ShutdownToIdle();
					}
				}
				break;
			case 3:
				if( SGetSetting(SETTING_RETURNTO) == RETURNTO_SYSMENU )
					MountDevices();
					gprintf("ReturnTo:System Menu\n");
					BootMainSysMenu();
				if( SGetSetting(SETTING_RETURNTO) == RETURNTO_AUTOBOOT )
				{
					switch( SGetSetting(SETTING_AUTBOOT) )
					{
						case AUTOBOOT_SYS:
							MountDevices();
							gprintf("AutoBoot:System Menu\n");
							BootMainSysMenu();
							break;
						case AUTOBOOT_HBC:
							gprintf("AutoBoot:Homebrew Channel\n");
							LoadHBC();
							error=ERROR_BOOT_HBC;
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
							break;

						case AUTOBOOT_DISABLED:
						default:
							break;
					}
				}
				break;
			default :
				if( ClearState() < 0 )
					error = ERROR_STATE_CLEAR;
			case 0: 
				switch( SGetSetting(SETTING_AUTBOOT) )
				{
					case AUTOBOOT_SYS:
						MountDevices();
						gprintf("AutoBoot:System Menu\n");
						BootMainSysMenu();
						break;
					case AUTOBOOT_HBC:
						gprintf("AutoBoot:Homebrew Channel\n");
						LoadHBC();
						error=ERROR_BOOT_HBC;
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
						break;

					case AUTOBOOT_DISABLED:
					default:
						break;
				}
				break;

		}
	}
	//remove the "Magic Priiloader word" cause it has done its purpose
	if(*(vu32*)0x8132FFFB == 0x4461636f)
	{
		gprintf("\"Magic Priiloader Word\" found!\n");
		gprintf("clearing memory of the \"Magic Priiloader Word\"\n");
		*(vu32*)0x8132FFFB = 0x00000000;
		DCFlushRange((void*)0x8132FFFB,4);
	}
	else
	{
		gprintf("Reset Button is hold down\n");
	}
	AUDIO_Init (NULL);
	DSP_Init ();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	r = (s32)MountDevices();
	gprintf("FAT_Init():%d\n", r );

	r = PAD_Init();
	gprintf("PAD_Init():%d\n", r );

	r = WPAD_Init();
	gprintf("WPAD_Init():%d\n", r );

	WPAD_SetPowerButtonCallback(HandleWiiMoteEvent);
	STM_RegisterEventHandler(HandleSTMEvent);

	ClearScreen();

	s32 cur_off=0;
	u32 redraw=true;
	u32 SysVersion=GetSysMenuVersion();

	if( SGetSetting(SETTING_STOPDISC) )
	{
		DVDStopDisc();
	}
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
 
#ifdef DEBUG
		if ( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) )
		{
			
			
			LoadHacks();
			//u64 TitleID = 0x0001000030303032LL;

			//u32 cnt ATTRIBUTE_ALIGN(32);
			//ES_GetNumTicketViews(TitleID, &cnt);
			//tikview *views = (tikview *)memalign( 32, sizeof(tikview)*cnt );
			//ES_GetTicketViews(TitleID, views, cnt);
			//ES_LaunchTitle(TitleID, &views[0]);	
		}
#endif

		if ( (WPAD_Pressed & WPAD_BUTTON_A) || (PAD_Pressed & PAD_BUTTON_A) )
		{
			ClearScreen();
			switch(cur_off)
			{
				case 0:
					RemountDevices();
					BootMainSysMenu();
				break;
				case 1:		//Load HBC
				{
					LoadHBC();
					//oops, error'd
					error=ERROR_BOOT_HBC;
				} break;
				case 2: //Load Bootmii
				{
					LoadBootMii();
					//well that failed...
					error=ERROR_BOOT_BOOTMII;
					break;
				}
				case 3:		//load main.dol from /shared2 dir
					AutoBootDol();
				break;
				case 4:
					InstallLoadDOL();
				break;
				case 5:
					SysHackSettings();
				break;
				case 6:
					SetSettings();
				break;
				default:
				break;

			}

			ClearScreen();
			redraw=true;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_DOWN) || (PAD_Pressed & PAD_BUTTON_DOWN) )
		{
			cur_off++;

			if( error == ERROR_UPDATE )
			{
				if( cur_off >= 8)
					cur_off = 0;
			}else {

				if( cur_off >=7)
					cur_off = 0;
			}

			redraw=true;
		} else if ( (WPAD_Pressed & WPAD_BUTTON_UP) || (PAD_Pressed & PAD_BUTTON_UP) )
		{
			cur_off--;

			if( cur_off < 0 )
			{
				if( error == ERROR_UPDATE )
				{
					cur_off=8-1; //phpgeek said 11-1 why 11?
				} else {
					cur_off=7-1; //phpgeek said 10-1?
				}
			}

			redraw=true;
		}

		if( redraw )
		{
#ifdef DEBUG
			printf("\x1b[2;0Hpreloader v%d.%d DEBUG (Sys:%d)(IOS:%d)(%s %s)\n", VERSION>>8, VERSION&0xFF, SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
#else
			if( BETAVERSION > 0 )
			{
				PrintFormat( 0, 160, 480-48, "preloader v%d.%d(beta v%d)", (BETAVERSION>>16)&0xFF, BETAVERSION>>8, BETAVERSION&0xFF );
			} else {
				PrintFormat( 0, 160, 480-48, "priiloader v%d.%d (r%s)", VERSION>>8, VERSION&0xFF,SVN_REV_STR );
			}
			PrintFormat( 0, 16, 480-64, "IOS v%d", (*(vu32*)0x80003140)>>16 );
			PrintFormat( 0, 16, 480-48, "Systemmenu v%d", SysVersion );			
			PrintFormat( 0, 16, 480-16, "priiloader is a mod of Preloader 0.30");
#endif
			// ((640/2)-(strlen("Systemmenu")*13/2))>>1
			
			//PrintFormat( 0, 16, 64, "Pos:%d", ((640/2)-(strlen("Update")*13/2))>>1);

			PrintFormat( cur_off==0, 127, 64, "System Menu");
			PrintFormat( cur_off==1, 108, 80, "Homebrew Channel");
			PrintFormat( cur_off==2, 127, 96, "BootMii IOS");
			PrintFormat( cur_off==3, 114, 128, "Installed File");
			PrintFormat( cur_off==4, 105, 144, "Load/Install File");
			PrintFormat( cur_off==5, 108, 160, "System Menu Hacks");
			PrintFormat( cur_off==6, 134, 176, "Settings");

			if (error > 0)
			{
				ShowError();
				error = ERROR_NONE;
			}
			redraw = false;
		}

		if( Shutdown )
		{
			*(vu32*)0xCD8000C0 &= ~0x20;
			//when we are in preloader itself we should make the video black before the user thinks its not shutting down...
			//TODO : fade to black if possible without a gfx lib?
			//STM_SetVIForceDimming ?
			VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
			DVDStopDisc();
			WPAD_Shutdown();
			ShutdownDevices();

			if( SGetSetting(SETTING_IGNORESHUTDOWNMODE) )
			{
				STM_ShutdownToStandby();
			} 
			else 
			{
				if( CONF_GetShutdownMode() == CONF_SHUTDOWN_STANDBY )
					STM_ShutdownToStandby();
				else
					STM_ShutdownToIdle();
			}

		}


		VIDEO_WaitVSync();
	}

	return 0;
}
