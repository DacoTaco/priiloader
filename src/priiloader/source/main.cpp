/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar/DacoTaco

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
#define MAGIC_WORD_DACO 1
#define MAGIC_WORD_PUNE 2
#define MAGIC_WORD_ABRA 3
#define ARGUMENT_NAND_PATH "/title/00000001/00000002/data/main.nfo"
#define BINARY_NAND_PATH "/title/00000001/00000002/data/main.bin"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string>
#include <string_view>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/sha.h>
#include <ogc/usb.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>
#include <ogc/lwp_watchdog.h>

#include <sys/dir.h>
#include <vector>
#include <algorithm>
#include <time.h>


//Project files
#include "Input.h"
#include "gitrev.h"
#include "Global.h"
#include "Video.h"
#include "settings.h"
#include "state.h"
#include "error.h"
#include "hacks.h"
#include "font.h"
#include "gecko.h"
#include "password.h"
#include "HTTP_Parser.h"
#include "dvd.h"
#include "titles.hpp"
#include "DiscContent.h"
#include "mem2_manager.h"
#include "HomebrewChannel.h"
#include "IOS.h"
#include "mount.h"
#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "SystemMenu.h"
#include "vWii.h"

//loader files
#include "patches.h"
#include "loader.h"

//Bin includes
#include "loader_bin.h"

typedef struct _dol_settings 
{
	s8 HW_AHBPROT_bit;
	u8 argument_count;
	s32 arg_cli_length;
	char* arg_command_line;
}__attribute((packed)) _dol_settings;

typedef struct {
	std::string app_name;
	std::string app_path;
	u8 HW_AHBPROT_ENABLED;
	std::vector<std::string> args;
}Binary_struct;

extern "C"
{
	extern void __exception_closeall();
}

//overwrite the weak variable in libogc that enables malloc to use mem2. this disables it
u32 MALLOC_MEM2 = 0;

static u8 _mountChanged = 0;

void _mountCallback(bool sd_mounted, bool usb_mounted)
{
	//the callback only gets fired if the mount actually changed value, so we just set the variable hehe
	_mountChanged = 1;
}

s32 __IOS_LoadStartupIOS( void )
{
	return 0;
}

void SysHackHashSettings( void )
{
	u32 SysVersion=GetSysMenuVersion();
	struct hack_index{
		std::string desc;
		u32 index;
	};
	std::vector<hack_index> _hacks;
	s16 cur_off=0;
	u16 max_pos = 0;
	u16 min_pos = 0;
	s32 fd = 0;
	bool redraw=true;
	bool reload=true;
	u8 mountedFlags = GetMountedFlags();
	StorageDevice device = StorageDevice::Auto;
	if (!HAS_SD_FLAG(mountedFlags) && !HAS_USB_FLAG(mountedFlags))
		device = StorageDevice::NAND;

	while(1)
	{
		if (_mountChanged)
		{
			gprintf("mount changed");
			ClearScreen();
			PrintFormat(1, TEXT_OFFSET("Reloading Hacks..."), 208, "Reloading Hacks...");
			sleep(1);
			mountedFlags = GetMountedFlags();
			device = StorageDevice::Auto;
			if (!HAS_SD_FLAG(mountedFlags) && !HAS_USB_FLAG(mountedFlags))
				device = StorageDevice::NAND;
			reload = true;
			min_pos = 0;
			max_pos = 0;
			cur_off = 0;
			_mountChanged = 0;
			redraw = true;
		}

		if (reload)
		{
			gprintf("reloading...");
			if (!LoadSystemHacks(device))
			{
				if (GetMountedFlags() == 0)
				{
					PrintFormat(1, TEXT_OFFSET("Failed to mount FAT device"), 208 + 16, "Failed to mount FAT device");
				}
				else
				{
					PrintFormat(1, TEXT_OFFSET("Can't find device:/apps/priiloader/hacks_hash.ini"), 208 + 16, "Can't find device:/apps/priiloader/hacks_hash.ini");
				}
				PrintFormat(1, TEXT_OFFSET("Can't find hacks_hash.ini on NAND"), 208 + 16 + 16, "Can't find hacks_hash.ini on NAND");
				sleep(5);
				return;
			}
			reload = false;
			gprintf("loaded hacks");

			//loop hacks file and see which one we show
			_hacks.clear();
			for (unsigned int i = 0; i < system_hacks.size(); ++i)
			{
				if (system_hacks[i].max_version >= SysVersion && system_hacks[i].min_version <= SysVersion
					&& system_hacks[i].masterID.length() == 0)
				{
					hack_index hack;
					hack.desc.assign(system_hacks[i].desc, 0, 39);
					while (hack.desc.size() < 40)
						hack.desc += " ";
					hack.index = i;
					_hacks.push_back(hack);
				}
			}

			if (_hacks.size() == 0)
			{
				mountedFlags = GetMountedFlags();
				if (device == StorageDevice::Auto && HAS_SD_FLAG(mountedFlags) && HAS_USB_FLAG(mountedFlags))
				{
					device = settings->PreferredMountPoint == PreferredMountPoint::MOUNT_USB
						? StorageDevice::SD
						: StorageDevice::USB;
					gprintf("switching to 2nd device...");
					reload = 1;
					continue;
				}
				else if (device != StorageDevice::NAND)
				{
					device = StorageDevice::NAND;
					gprintf("switching to NAND...");
					reload = 1;
					continue;
				}

				PrintFormat(1, TEXT_OFFSET("Couldn't find any hacks for"), 208, "Couldn't find any hacks for");
				PrintFormat(1, TEXT_OFFSET("System Menu version:vxxx"), 228, "System Menu version:v%d", SysVersion);
				sleep(5);
				return;
			}

			//ye, those tv's want a special treatment again >_>
			if (VI_TVMODE_ISFMT(rmode->viTVMode, VI_NTSC) || CONF_GetEuRGB60() || CONF_GetProgressiveScan())
				max_pos = 14;
			else
				max_pos = 17;

			if (_hacks.size() <= max_pos)
				max_pos = _hacks.size() - 1;

			ClearScreen();
		}

		Input_ScanPads();

		u32 pressed = Input_ButtonsDown();

		if ( pressed & INPUT_BUTTON_B )
		{
			break;
		}

		if ( pressed & INPUT_BUTTON_A )
		{
			if( (u16)cur_off == _hacks.size())
			{
				//first try to open the file on the SD card/USB, if we found it copy it, other wise skip
				s8 fail = 0;
				s32 ret;
				FILE *in = NULL;
				if (GetMountedFlags() != 0)
				{
					in = fopen (BuildPath("/apps/priiloader/hacks_hash.ini").c_str(),"rb");
				}
				else
				{
					gprintf("no FAT device found");
				}
				if (( HAS_SD_FLAG(GetMountedFlags()) && !__io_wiisd.isInserted() ) || ( HAS_USB_FLAG(GetMountedFlags()) && !__io_usbstorage.isInserted() ) )
				{
					PrintFormat( 0, 103, 64+(max_pos+5)*16, "saving failed : SD/USB error");
					continue;
				}
				if( in != NULL )
				{
					//Read in whole file & create it on nand
					fseek( in, 0, SEEK_END );
					u32 size = ftell(in);
					fseek( in, 0, 0);

					//always have an aligned buffer/file
					char *buf = (char*)mem_align( 32, ALIGN32(size) );
					memset( buf, 0, ALIGN32(size) );
					fread( buf, sizeof( char ), size, in );

					fclose(in);

					fd = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1|2 );
					if( fd >= 0 )
					{
						//File already exists, delete and recreate!
						ISFS_Close( fd );
						ret = ISFS_Delete("/title/00000001/00000002/data/hackshas.ini");
						if( ret < 0)
						{
							fail=1;
							mem_free(buf);
							goto handle_hacks_fail;
						}
					}
					ret = ISFS_CreateFile("/title/00000001/00000002/data/hackshas.ini", 0, 3, 3, 3);
					if(ret < 0)
					{
						fail=2;
						mem_free(buf);
						goto handle_hacks_fail;
					}
					fd = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1|2 );
					if( fd < 0 )
					{
						ret = fd;
						fail=3;
						ISFS_Close( fd );
						mem_free(buf);
						goto handle_hacks_fail;
					}
					//write the file content, aligned by 32 so there are no issues having to read less then 32 bytes 
					//(something something, isfs having to be 32 aligned)
					ret = ISFS_Write( fd, buf, ALIGN32(size) );
					if( ret < 0)
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
					gprintf("hacks_hash.ini save error %d(%d)",ret,fail);
				}
				
				ret = 0;
				fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );

				if( fd >= 0 )
				{
					//File already exists, delete and recreate!
					ISFS_Close( fd );
					ret = ISFS_Delete("/title/00000001/00000002/data/hacksh_s.ini");
					if(ret <0)
					{
						fail = 5;
						goto handle_hacks_s_fail;
					}
				}
				ret = ISFS_CreateFile("/title/00000001/00000002/data/hacksh_s.ini", 0, 3, 3, 3);
				if(ret < 0)
				{
					fail = 6;
					goto handle_hacks_s_fail;
				}
				fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );
				if( fd < 0 )
				{
					ret = fd;
					fail=7;
					goto handle_hacks_s_fail;
				}
				ret = ISFS_Write( fd, &states_hash[0], states_hash.size() );
				if(ret <0)
				{
					fail = 8;
					ISFS_Close(fd);
					goto handle_hacks_s_fail;
				}

				ISFS_Close( fd );
handle_hacks_s_fail:
				if(fail > 0)
				{
					gprintf("hacksh_s.ini save error %d(%d)",ret,fail);
				}

				if( fail > 0 )
					PrintFormat( 0, 118, 64+(max_pos+5)*16, "saving failed");
				else
					PrintFormat( 0, 118, 64+(max_pos+5)*16, "settings saved");
			} 
			else 
			{
				s32 j = 0;
				u32 i = 0;
				for(i=0; i<system_hacks.size(); ++i)
				{
					if( system_hacks[i].max_version >= SysVersion && system_hacks[i].min_version <= SysVersion
							&& system_hacks[i].masterID.length() == 0)
					{
						if( cur_off == j++)
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

		if ( pressed & INPUT_BUTTON_DOWN )
		{
			cur_off++;
			if ((u16)cur_off > _hacks.size())
			{
				cur_off = 0;
				min_pos = 0;
			}
			else if (cur_off > (max_pos + min_pos) && (u16) cur_off != _hacks.size())
			{
				min_pos = cur_off - max_pos;
			}
			
			redraw = true;

		} 
		else if ( pressed & INPUT_BUTTON_UP )
		{
			cur_off--;
			if (cur_off < 0)
			{
				cur_off = _hacks.size();
				min_pos = _hacks.size() - max_pos - 1;
			}
			else if (cur_off < min_pos)
			{
				min_pos = cur_off;
			}
			
			redraw = true;
		}
		if( redraw )
		{
			if(_hacks.size() -1 > max_pos && (min_pos != (s32)_hacks.size() - max_pos - 1) )
				PrintFormat( 0,TEXT_OFFSET("-----More-----"),64+(max_pos+2)*16,"-----More-----");
			else
				PrintFormat( 0,TEXT_OFFSET("               "),64+(max_pos+2)*16,"               ");
			
			if(min_pos > 0 )
				PrintFormat( 0,TEXT_OFFSET("-----Less-----"),64,"-----Less-----");
			else
				PrintFormat( 0,TEXT_OFFSET("               "),64,"               ");

			u8 j = 0;
			for( s32 i=min_pos; i <= (min_pos + max_pos); ++i)
			{
				PrintFormat( (u16)cur_off==i, 16, 64+(j+1)*16, "%s", _hacks[i].desc.c_str() );

				if( states_hash[_hacks[i].index] )
				{
					PrintFormat( (u16)cur_off==i, 256, 64+(j+1)*16, "enabled ");
				}
				else
				{
					PrintFormat( (u16)cur_off==i, 256, 64+(j+1)*16, "disabled");
				}
				j++;
			}

			PrintFormat( (u32)cur_off==_hacks.size(), 118, 64+(max_pos+4)*16, "save settings");
			PrintFormat( 0, 103, 64+(max_pos+5)*16, "                            ");

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
		Input_ScanPads();
		u32 pressed = Input_ButtonsDown();

		if ( pressed & INPUT_BUTTON_B )
		{
			LoadSettings();
			SetDumpDebug(SGetSetting(SETTING_DUMPGECKOTEXT));
			break;
		}
		switch( cur_off )
		{
			case 0:
			{
				if ( pressed & INPUT_BUTTON_LEFT )
				{
					if( settings->autoboot == AUTOBOOT_DISABLED )
						settings->autoboot = AUTOBOOT_FILE;
					else
						settings->autoboot--;
					redraw=true;
				}else if ( pressed & INPUT_BUTTON_RIGHT )
				{
					if( settings->autoboot == AUTOBOOT_FILE )
						settings->autoboot = AUTOBOOT_DISABLED;
					else
						settings->autoboot++;
					redraw=true;
				}
				break;
			} 
			case 1:
			{
				if ( pressed & INPUT_BUTTON_RIGHT				||
					 pressed & INPUT_BUTTON_A
					)
				{
					settings->ReturnTo++;
					if( settings->ReturnTo > RETURNTO_FILE )
						settings->ReturnTo = RETURNTO_SYSMENU;

					redraw=true;
				} else if ( pressed & INPUT_BUTTON_LEFT ) {

					if( settings->ReturnTo == RETURNTO_SYSMENU )
						settings->ReturnTo = RETURNTO_FILE;
					else
						settings->ReturnTo--;

					redraw=true;
				}

				break;
			} 			
			case 2:
			{
				if ( pressed & INPUT_BUTTON_RIGHT				||
					 pressed & INPUT_BUTTON_A
					)
				{
					settings->ShutdownTo++;

					if(settings->ShutdownTo > SHUTDOWNTO_AUTOBOOT )
						settings->ShutdownTo = SHUTDOWNTO_NONE;

					redraw=true;
				}
				else if ( pressed & INPUT_BUTTON_LEFT ) 
				{

					if( settings->ShutdownTo == SHUTDOWNTO_NONE )
						settings->ShutdownTo = SHUTDOWNTO_AUTOBOOT;
					else
						settings->ShutdownTo--;

					redraw=true;
				}
				break;
			} 		
			case 3:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
					)
				{
					if( settings->StopDisc )
						settings->StopDisc = 0;
					else 
						settings->StopDisc = 1;

					redraw=true;
				}
				break;
			} 
			case 4:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
					)
				{
					if( settings->LidSlotOnError )
						settings->LidSlotOnError = 0;
					else 
						settings->LidSlotOnError = 1;
				
					redraw=true;
				}
				break;
			} 
			case 5:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
					)
				{
					if( settings->IgnoreShutDownMode )
						settings->IgnoreShutDownMode = 0;
					else 
						settings->IgnoreShutDownMode = 1;
				
					redraw=true;
				}
				break;
			} 
			case 6:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
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
				break;
			}		
			case 7:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
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
							Input_ScanPads();
							u32 input = Input_ButtonsDown(true);
							if(input & INPUT_BUTTON_A)
							{
								settings->PasscheckPriiloader = true;
								break;
							}
							else if(input & INPUT_BUTTON_B)
							{
								break;
							}
							VIDEO_WaitVSync();
						}
						ClearScreen();
						
					}
					redraw=true;
				}
				break;
			}		
			case 8:
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
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
							Input_ScanPads();
							u32 input = Input_ButtonsDown(true);
							if(input & INPUT_BUTTON_A)
							{
								settings->PasscheckMenu = true;
								break;
							}
							else if(input & INPUT_BUTTON_B)
							{
								break;
							}
							VIDEO_WaitVSync();
						}
						ClearScreen();
					}
					redraw=true;
				}
				break;
			}		
			case 9: //show Debug Info
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
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
			}
			case 10: //download rc updates
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
					)
				{
					if ( settings->ShowRCUpdates )
						settings->ShowRCUpdates = 0;
					else
						settings->ShowRCUpdates = 1;
					redraw=true;
				}
				break;		
			}
			case 11: // Preferred mount point
			{
				if (pressed & INPUT_BUTTON_LEFT ||
					pressed & INPUT_BUTTON_RIGHT)
				{
					switch (settings->PreferredMountPoint)
					{
						case PreferredMountPoint::MOUNT_USB:
							settings->PreferredMountPoint = (pressed & INPUT_BUTTON_LEFT)
								? PreferredMountPoint::MOUNT_SD
								: PreferredMountPoint::MOUNT_AUTO;
							break;
						case PreferredMountPoint::MOUNT_SD:
							settings->PreferredMountPoint = (pressed & INPUT_BUTTON_LEFT)
								? PreferredMountPoint::MOUNT_AUTO
								: PreferredMountPoint::MOUNT_USB;
							break;
						case PreferredMountPoint::MOUNT_AUTO:
						default:
							settings->PreferredMountPoint = (pressed & INPUT_BUTTON_LEFT)
								? PreferredMountPoint::MOUNT_USB
								: PreferredMountPoint::MOUNT_SD;
							break;
					}
					redraw = true;
				}
				break;
			}
			case 12: //ignore ios reloading for system menu?
			{
				if ( pressed & INPUT_BUTTON_LEFT				|| 
					 pressed & INPUT_BUTTON_RIGHT				|| 
					 pressed & INPUT_BUTTON_A
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
				break;
			}
			case 13:		//	System Menu IOS
			{
				if ( pressed & INPUT_BUTTON_LEFT )
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
					IsIOSstub(settings->SystemMenuIOS);
#endif

					redraw=true;
				} else if( pressed & INPUT_BUTTON_RIGHT ) 
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
					IsIOSstub(settings->SystemMenuIOS);
#endif
					redraw=true;
				}
				break;
			} 
			case 14:
			{
				if ( pressed & INPUT_BUTTON_A )
				{
					if( SaveSettings() )
						PrintFormat( 1, 114, 128+(16*15), "save settings : done  ");
					else
						PrintFormat( 1, 114, 128+(16*15), "save settings : failed");
				}
				break;
			} 
			case 15:
			{
				if ( pressed & INPUT_BUTTON_A )
				{
					goto _exit;
				}
				break;
			} 
			default:
				cur_off = 0;
				break;
		}

		if ( pressed & INPUT_BUTTON_DOWN )
		{
			cur_off++;
			if( (settings->UseSystemMenuIOS) && (cur_off == 13))
				cur_off++;
			if( cur_off >= 16)
				cur_off = 0;
			
			redraw=true;
		} else if ( pressed & INPUT_BUTTON_UP )
		{
			cur_off--;
			if( (settings->UseSystemMenuIOS) && (cur_off == 13))
				cur_off--;
			if( cur_off < 0 )
				cur_off = 15;
			
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
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          System Menu    ");
				break;
				case RETURNTO_PRIILOADER:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Priiloader     ");
				break;
				case RETURNTO_AUTOBOOT:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Autoboot       ");
				break;
				case RETURNTO_FILE:
					PrintFormat( cur_off==1, 0, 128,    "             Return to:          Installed File ");
				break;
				default:
					gdprintf("SetSettings : unknown return to value %d",settings->ReturnTo);
					settings->ReturnTo = RETURNTO_PRIILOADER;
					break;
			}

			switch( settings->ShutdownTo )
			{
				case SHUTDOWNTO_NONE:
					PrintFormat( cur_off==2, 0, 128+(16*1), "           Shutdown to:          Power Off ");
				break;
				case SHUTDOWNTO_PRIILOADER:
					PrintFormat( cur_off==2, 0, 128+(16*1), "           Shutdown to:          Priiloader");
				break;
				case SHUTDOWNTO_AUTOBOOT:
					PrintFormat( cur_off==2, 0, 128+(16*1), "           Shutdown to:          Autoboot  ");
				break;
				default:
					gdprintf("SetSettings : unknown shutdown to value %d",settings->ShutdownTo);
					settings->ShutdownTo = SHUTDOWNTO_NONE;
					break;
			}
			
			//PrintFormat( 0, 16, 64, "Pos:%d", ((rmode->viWidth /2)-(strlen("settings saved")*13/2))>>1);

			std::string fatDevice;
			switch (settings->PreferredMountPoint)
			{
				case PreferredMountPoint::MOUNT_USB:
					fatDevice = "USB ";
					break;
				case PreferredMountPoint::MOUNT_SD:
					fatDevice = "SD  ";
					break;
				case PreferredMountPoint::MOUNT_AUTO:
				default:
					fatDevice = "Auto";
					break;
			}
			PrintFormat( cur_off==3, 0, 128+(16*2), "  Stop disc on startup:          %s", settings->StopDisc?"on ":"off");
			PrintFormat( cur_off==4, 0, 128+(16*3), "   Light slot on error:          %s", settings->LidSlotOnError?"on ":"off");
			PrintFormat( cur_off==5, 0, 128+(16*4), "        Ignore standby:          %s", settings->IgnoreShutDownMode?"on ":"off");
			PrintFormat( cur_off==6, 0, 128+(16*5), "      Background Color:          %s", settings->BlackBackground?"Black":"White");
			PrintFormat( cur_off==7, 0, 128+(16*6), "    Protect Priiloader:          %s", settings->PasscheckPriiloader?"on ":"off");
			PrintFormat( cur_off==8, 0, 128+(16*7), "      Protect Autoboot:          %s", settings->PasscheckMenu?"on ":"off");
			PrintFormat( cur_off==9, 0, 128+(16*8), "     Dump Gecko output:          %s", settings->DumpGeckoText?"on ":"off");
			PrintFormat( cur_off==10,0, 128+(16*9), "       Show RC Updates:          %s", settings->ShowRCUpdates?"on ":"off");
			PrintFormat( cur_off==11,0, 128+(16*10),"    Default fat device:          %s", fatDevice.c_str());
			PrintFormat( cur_off==12,0, 128+(16*11),"   Use System Menu IOS:          %s", settings->UseSystemMenuIOS?"on ":"off");
			if(!settings->UseSystemMenuIOS)
			{
				PrintFormat( cur_off==13, 0, 128+(16*12), "     IOS to use for SM:          %d  ", (u32)(TitleIDs[IOS_off]&0xFFFFFFFF) );
			}
			else
			{
				PrintFormat( cur_off==13, 0, 128+(16*12),	"                                        ");
			}
			PrintFormat( cur_off==14, 114, 128+(16*15), "save settings         ");
			PrintFormat( cur_off==15, 114, 128+(16*16), "  Exit Menu");

			redraw = false;
		}

		VIDEO_WaitVSync();
	}
_exit: 
	LoadSettings();
	SetDumpDebug(SGetSetting(SETTING_DUMPGECKOTEXT));
	mem_free(TitleIDs);
	return;
}
s8 BootDolFromMem(void* binary , u8 HW_AHBPROT_ENABLED, struct __argv *args )
{
	if(binary == NULL)
		return -1;

	void* loader_addr = NULL;
	loader_t loader = NULL;
	u8 ret = 1;

	try
	{
		//its an elf; lets start killing DVD :')
		if(DVDDiscAvailable())
		{
			gprintf("BootDolFromMem : excecuting StopDisc Async...");
			DVDStopDriveAsync();
		}
		else
		{
			gprintf("BootDolFromMem : Skipping StopDisc -> no drive or disc in drive");
		}

		//prepare loader
		loader_addr = (void*)mem_align(32,loader_bin_size);
		if(!loader_addr)
			throw "failed to alloc the loader";

		memcpy(loader_addr,loader_bin,loader_bin_size);	
		DCFlushRange(loader_addr, loader_bin_size);
		ICInvalidateRange(loader_addr, loader_bin_size);
		loader = (loader_t)loader_addr;


		gprintf("BootDolFromMem : shutting down...");

		ClearState();
		Input_Shutdown();

		if(DVDAsyncBusy())
		{
			gprintf("checking DVD drive...");
			while(DVDAsyncBusy());
		}
		DVDCloseHandle();

		s8 bAHBPROT = 0;
		s32 Ios_to_load = 0;
		if(read32(0x0d800064) == 0xFFFFFFFF )
			bAHBPROT = HW_AHBPROT_ENABLED;
		if( !IsIOSstub(58) )
		{
			Ios_to_load = 58;
		}
		else if( !IsIOSstub(61) )
		{
			Ios_to_load = 61;
		}
		else if( !IsIOSstub(IOS_GetPreferredVersion()) )
		{
			Ios_to_load = IOS_GetPreferredVersion();
		}
		else
		{
			PrintFormat( 1, TEXT_OFFSET("failed to reload ios for homebrew! ios is a stub!"), 208, "failed to reload ios for homebrew! ios is a stub!");
			sleep(2);	
		}

		ISFS_Deinitialize();
		ShutdownMounts();
		USB_Deinitialize();

		if (Ios_to_load > 2 && Ios_to_load < 255)
		{
			ReloadIOS(Ios_to_load, bAHBPROT);
			system_state.ReloadedIOS = 1;
		}

		gprintf("IOS state : ios %d - ahbprot : %d ", IOS_GetVersion(), (read32(0x0d800064) == 0xFFFFFFFF));
		__IOS_ShutdownSubsystems();
		if(system_state.Init)
		{
			VIDEO_SetBlack(true);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
		__exception_closeall();

		gprintf("BootDolFromMem : starting binary... 0x%08X",loader_addr);	
		ICSync();
		loader(binary, args, args != NULL, BINARY_TYPE_DEFAULT);

		//it failed. FAIL!
		gprintf("this ain't good");
		if(system_state.Init)
		{
			VIDEO_SetBlack(false);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
		__IOS_InitializeSubsystems();
		InitMounts(_mountCallback);
		ISFS_Initialize();
		Input_Init();
		PAD_Init();
		ret = -1;

	}
	catch (const std::string& ex)
	{
		gprintf("BootDolFromMem Exception -> %s",ex.c_str());
		ret = -7;
	}
	catch (char const* ex)
	{
		gprintf("BootDolFromMem Exception -> %s",ex);
		ret = -7;
	}
	catch(...)
	{
		gprintf("BootDolFromMem Exception was thrown");
		ret = -7;
	}

	if(loader_addr)
		mem_free(loader_addr);

	return ret;
}

s8 BootDolFromFile( const char* Dir , u8 HW_AHBPROT_ENABLED,const std::vector<std::string> &args_list)
{
	struct __argv* args = NULL;
	u8 *binary = NULL;
	s8 ret = 1;
	FILE* dol = NULL;

	try
	{
		if (GetMountedFlags() == 0)
			return -5;

		std::string _path = Dir;
		gprintf("going to boot %s",_path.c_str());

		args = (struct __argv*)mem_align(32,sizeof(__argv));
		if(!args)
			throw "arg malloc failed";

		memset(args,0,sizeof(__argv));

		args->argvMagic = 0;
		args->length = 0;
		args->commandLine = NULL;
		args->argc = 0;
		args->argv = &args->commandLine;
		args->endARGV = args->argv + 1;
		
		//calculate the char lenght of the arguments
		args->length = _path.size() +1;
		//loading args
		for(u32 i = 0; i < args_list.size(); i++)
		{
			if(args_list[i].c_str())
				args->length += strnlen(args_list[i].c_str(),128)+1;
		}

		//allocate memory for the arguments
		args->commandLine = (char*) mem_malloc(args->length);
		if(args->commandLine == NULL)
		{
			args->commandLine = 0;
			args->length = 0;
		}
		else
		{
			//assign arguments and the rest
			strcpy(&args->commandLine[0], _path.c_str());

			//add the other arguments
			args->argc = 1;

			if(args_list.size() > 0)
			{
				u32 pos = _path.size() +1;
				for(u32 i = 0; i < args_list.size(); i++)
				{
					if(args_list[i].c_str())
					{
						strcpy(&args->commandLine[pos], args_list[i].c_str());
						pos += strlen(args_list[i].c_str())+1;
					}
				}
				args->argc += args_list.size();
			}
			
			args->commandLine[args->length - 1] = '\0';
			args->argv = &args->commandLine;
			args->endARGV = args->argv + 1;
			args->argvMagic = ARGV_MAGIC; //everything is set so the magic is set so it becomes valid*/
		}
		dol = fopen(_path.c_str(), "rb");
		if(!dol)
			throw "failed to open file";

		fseek( dol, 0, SEEK_END );
		u32 size = ftell(dol);
		fseek( dol, 0, 0);

		binary = (u8*)mem_align( 32, ALIGN32(size) );
		if(!binary)
			throw "failed to alloc the binary";

		memset( binary, 0, size );
		fread( binary, sizeof( u8 ), size, dol );
		fclose(dol);

		BootDolFromMem(binary, HW_AHBPROT_ENABLED, args);
		//we didn't boot, that ain't good
		ret = -8;
	}
	catch (const std::string& ex)
	{
		gprintf("BootDolFromFile Exception -> %s",ex.c_str());
		ret = -7;
	}
	catch (char const* ex)
	{
		gprintf("BootDolFromFile Exception -> %s",ex);
		ret = -7;
	}
	catch(...)
	{
		gprintf("BootDolFromFile Exception was thrown");
		ret = -7;
	}

	if(dol)
		fclose(dol);	
	if(binary)
		mem_free(binary);
	if(args != NULL && args->commandLine != NULL)
		mem_free(args->commandLine);
	if(args)
		mem_free(args);

	return ret;
}
void BootDvdDrive(void)
{
	Input_Shutdown();
	ShutdownMounts();
	USB_Deinitialize();

	BootDiscContent();

	//well that failed
	error = ERROR_DVD_BOOT_FAILURE;
	InitMounts(_mountCallback);
	ISFS_Initialize();
	Input_Init();
}

void InstallLoadDOL( void )
{
	char filename[NAME_MAX+1],filepath[MAXPATHLEN+NAME_MAX+1];
	memset(filename,0,NAME_MAX+1);
	memset(filepath,0,MAXPATHLEN+NAME_MAX+1);
	std::vector<Binary_struct> app_list;
	StorageDevice device = StorageDevice::Auto;
	DIR* dir;
	s8 reload = 1;
	s8 redraw = 1;
	s16 cur_off = 0;
	s16 max_pos = 0;
	s16 min_pos = 0;
	u32 ret = 0;
	while(1)
	{
		if (_mountChanged)
		{
			if (GetMountedFlags() == 0)
			{
				ClearScreen();
				PrintFormat(1, TEXT_OFFSET("NO fat device found!"), 208, "NO fat device found!");
				sleep(5);
				return;
			}

			ClearScreen();
			PrintFormat( 1, TEXT_OFFSET("Reloading Binaries..."), 208, "Reloading Binaries...");
			sleep(1);
			app_list.clear();
			reload = 1;
			min_pos = 0;
			max_pos = 0;
			cur_off = 0;
			_mountChanged = 0;
			redraw=1;
		}

		if(reload)
		{
			gprintf("loading binaries...");
			PrintFormat(1, ((rmode->viWidth / 2) - ((strlen("loading binaries...")) * 13 / 2)) >> 1, 208 + 16, "loading binaries...");
			reload = 0;
			dir = opendir (BuildPath("/apps/", device).c_str());
			if( dir != NULL )
			{
				//get all files names
				while( readdir(dir) != NULL )
				{
					memset(filename,0,NAME_MAX);
					memcpy(filename, dir->fileData.d_name, strnlen(dir->fileData.d_name,NAME_MAX) );
					if(strncmp(filename,".",1) == 0 || strncmp(filename,"..",2) == 0 )
					{
						//we dont want the root or the dirup stuff. so lets filter them
						continue;
					}
					sprintf(filepath, BuildPath("/apps/%s/boot.dol", device).c_str(), filename);
					FILE* app_bin;
					app_bin = fopen(filepath,"rb");
					if(!app_bin)
					{
						sprintf(filepath, BuildPath("/apps/%s/boot.elf", device).c_str(), filename);
						app_bin = fopen(filepath,"rb");
						if(!app_bin)
							continue;
					}
					fclose(app_bin);
					Binary_struct temp;
					temp.HW_AHBPROT_ENABLED = 0;
					temp.app_path = filepath;
					temp.args.clear();
					sprintf(filepath, BuildPath("/apps/%s/meta.xml", device).c_str(), filename);
					app_bin = fopen(filepath,"rb");
					if(!app_bin)
					{
						gdprintf("failed to open meta.xml of %s",filename);
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
						gdprintf("buf == NULL");
						fclose(app_bin);
						temp.app_name = filename;
						app_list.push_back(temp);
						continue;
					}
					memset(buf,0,size+1);
					ret = fread(buf,1,size,app_bin);
					fclose(app_bin);
					if(ret != (u32)size)
					{
						mem_free(buf);						
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("failed to read data error %d",ret);
						continue;
					}

					try
					{
						rapidxml::xml_document<> doc;
						doc.parse<0>(buf);
						rapidxml::xml_node<>* appNode = doc.first_node("app");
						rapidxml::xml_node<>* node = NULL;
						if (appNode == NULL)
							throw "No app node found";
						
						node = appNode->first_node("name");
						if (node != NULL)
							temp.app_name = (std::string)node->value();

						if (appNode->first_node("no_ios_reload") != NULL || appNode->first_node("ahb_access") != NULL)
							temp.HW_AHBPROT_ENABLED = 1;

						node = appNode->first_node("arguments");
						if (node != NULL)
						{
							rapidxml::xml_node<>* arg = node->first_node("arg");
							while (arg != NULL)
							{
								temp.args.push_back((std::string)arg->value());
								arg = arg->next_sibling("arg");
							}
						}
					}
					catch (const std::string& str)
					{
						gprintf("InstallLoadDol Exception: %s", str.c_str());
					}
					catch (char const* str)
					{
						gprintf("InstallLoadDol Exception: %s", str);
					}
					catch(...)
					{	
						gprintf("InstallLoadDol Exception: General xml exception");
					}

					mem_free(buf);
					if(temp.app_name.size())
					{
						gdprintf("added %s to list",temp.app_name.c_str());
						app_list.push_back(temp);
						continue;
					}
					else
					{
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("no name found in xml D:<");
						continue;
					}
				}
				closedir( dir );
			}
			else
			{
				gprintf("WARNING: could not open device:/apps/ for binaries");
			}
			dir = opendir (BuildPath("/", device).c_str());
			if(dir == NULL)
				gprintf("WARNING: could not open device:/ for binaries");

			while( dir != NULL && readdir(dir) != NULL )
			{
				memset(filename,0,NAME_MAX);
				memcpy(filename, dir->fileData.d_name, strnlen(dir->fileData.d_name,NAME_MAX) );
				if( (strstr( filename, ".dol") != NULL) ||
					(strstr( filename, ".DOL") != NULL) ||
					(strstr( filename, ".elf") != NULL) ||
					(strstr( filename, ".ELF") != NULL) )
				{
					Binary_struct temp;
					temp.HW_AHBPROT_ENABLED = 0;
					temp.app_name = filename;
					sprintf(filepath, BuildPath("/%s", device).c_str(), filename);
					temp.app_path.assign(filepath);
					app_list.push_back(temp);
				}
			}
			if (dir != NULL)
				closedir( dir );
			
			if( app_list.size() == 0 )
			{
				u8 mountedFlags = GetMountedFlags();
				if(device == StorageDevice::Auto && HAS_SD_FLAG(mountedFlags) && HAS_USB_FLAG(mountedFlags))
				{
					device = settings->PreferredMountPoint == PreferredMountPoint::MOUNT_USB
						? StorageDevice::SD
						: StorageDevice::USB;
					gprintf("switching to 2nd device...");
					reload = 1;
					continue;
				}

				PrintFormat( 1, TEXT_OFFSET("Couldn't find any executable files"), 208, "Couldn't find any executable files");
				PrintFormat( 1, TEXT_OFFSET("in device:/apps/ on the devices!"), 228, "in device:/apps/ on the devices!");
				sleep(5);
				return;
			}

			//ye, those tv's want a special treatment again >_>
			if( VI_TVMODE_ISFMT(rmode->viTVMode, VI_NTSC) || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
				max_pos = 14;
			else
				max_pos = 19;

			if ((s32)app_list.size() <= max_pos)
				max_pos = app_list.size() -1;

			//sort app lists
			s8 swap = 0;
			for(s32 max = 0;max < (s32)app_list.size() * (s32)app_list.size();max++)
			{
				swap = 0;
				for (int count = 0; count < (s32)app_list.size(); count++)
				{
					std::string path = BuildPath("/apps/", device);
					if(app_list[count].app_path.rfind(path.c_str(), 0) == 0)
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
				if(swap)
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
			PrintFormat( 0, TEXT_OFFSET("A(A) Install File"), rmode->viHeight-96, "A(A) Install FIle");
			PrintFormat( 0, TEXT_OFFSET("1(Y) Load File   "), rmode->viHeight-80, "1(Y) Load File");
			PrintFormat( 0, TEXT_OFFSET("2(X) Delete installed File"), rmode->viHeight-64, "2(X) Delete installed File");

			redraw = false;
		}

		Input_ScanPads();
		u32 pressed  = Input_ButtonsDown();
 
		if ( pressed & INPUT_BUTTON_B )
			break;

		if ( pressed & INPUT_BUTTON_A )
		{
			ClearScreen();
			FILE *dol = fopen(app_list[cur_off].app_path.c_str(),"rb");
			if( dol == NULL )
			{
				PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Could not open:\"%s\" for reading") + app_list[cur_off].app_name.length())*13/2))>>1, 208, "Could not open:\"%s\" for reading", app_list[cur_off].app_name.c_str());
				sleep(5);
				break;
			}
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Installing \"%s\"...") + app_list[cur_off].app_name.length())*13/2))>>1, 208, "Installing \"%s\"...", app_list[cur_off].app_name.c_str());

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
				s32 fd = ISFS_Open(BINARY_NAND_PATH, 1|2 );

				if( fd >= 0 )	//delete old file
				{
					ISFS_Close( fd );
					ISFS_Delete(BINARY_NAND_PATH);
				}

				//file not found create a new one
				ISFS_CreateFile(BINARY_NAND_PATH, 0, 3, 3, 3);
				fd = ISFS_Open(BINARY_NAND_PATH, 1|2 );

				if( ISFS_Write( fd, buf, sizeof( char ) * size ) != (signed)(sizeof( char ) * size) )
				{
					PrintFormat( 1, TEXT_OFFSET("Writing dol failed!"), 240, "Writing dol failed!");
					gprintf("writing dol failure! error %d ( 0x%08X )",fd);
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
							dol_settings->arg_cli_length += strnlen(app_list[cur_off].args[i].c_str(),128)+1;
					}
					dol_settings->arg_cli_length += 1;

					dol_settings->arg_command_line = (char*)mem_align(32,dol_settings->arg_cli_length+1);
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
						dol_settings->arg_command_line[dol_settings->arg_cli_length -1] = 0x00;
						fd = ISFS_Open(ARGUMENT_NAND_PATH, 1|2 );
						if( fd >= 0 )	//delete old file
						{
							ISFS_Close( fd );
							ISFS_Delete(ARGUMENT_NAND_PATH);
						}
						ISFS_CreateFile(ARGUMENT_NAND_PATH, 0, 3, 3, 3);
						fd = ISFS_Open(ARGUMENT_NAND_PATH, 1|2 );
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
						if(dol_settings->arg_cli_length > 0)
							ISFS_Write(fd,&dol_settings->arg_cli_length, sizeof(dol_settings->arg_cli_length));
						if(dol_settings->arg_command_line != NULL && dol_settings->arg_cli_length > 0)
							ISFS_Write(fd, dol_settings->arg_command_line, dol_settings->arg_cli_length);
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

		if ( pressed & INPUT_BUTTON_X )
		{
			ClearScreen();
			//Delete file

			PrintFormat( 0, TEXT_OFFSET("Deleting installed File..."), 208, "Deleting installed File...");

			ISFS_Delete(ARGUMENT_NAND_PATH);

			//Check if there is already a main.dol installed
			s32 fd = ISFS_Open(BINARY_NAND_PATH, 1|2 );

			if( fd >= 0 )	//delete old file
			{
				ISFS_Close( fd );
				ISFS_Delete(BINARY_NAND_PATH);

				fd = ISFS_Open(BINARY_NAND_PATH, 1|2 );

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

		if ( pressed & INPUT_BUTTON_Y )
		{
			ClearScreen();
			PrintFormat( 1, TEXT_OFFSET("Loading binary..."), 208, "Loading binary...");	
			ret = BootDolFromFile(app_list[cur_off].app_path.c_str(),app_list[cur_off].HW_AHBPROT_ENABLED,app_list[cur_off].args);
			gprintf("loading %s ret %d",app_list[cur_off].app_path.c_str(),ret);
			PrintFormat( 1, TEXT_OFFSET("failed to load binary"), 224, "failed to load binary");
			sleep(3);
			ClearScreen();
			redraw=true;
		}
		if ( pressed & INPUT_BUTTON_UP )
		{
			cur_off--;
			if (cur_off < min_pos)
			{
				min_pos = cur_off;
				if(app_list.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                                            ");
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
		if ( pressed & INPUT_BUTTON_DOWN )
		{
			cur_off++;
			if (cur_off > (max_pos + min_pos))
			{
				min_pos = cur_off - max_pos;
				if(app_list.size() > 19)
				{
					for(s8 i = min_pos; i<=(min_pos + max_pos); i++ )
					{
						PrintFormat( 0, 16, 64+(i-min_pos+1)*16, "                                                            ");
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
	u8* binary = NULL;
	struct __argv *argv = NULL;
	try
	{
		STACK_ALIGN(_dol_settings,dol_settings,sizeof(_dol_settings),32);
		memset(dol_settings,0,sizeof(_dol_settings));
		STACK_ALIGN(fstats,status,sizeof(fstats),32);
		memset(status,0,sizeof(fstats));

		try
		{
			fd = ISFS_Open(ARGUMENT_NAND_PATH, ISFS_OPEN_READ );
			if(fd < 0)
				throw "failed to open argument file";

			if (ISFS_GetFileStats(fd,status) < 0 || status->file_length < sizeof(_dol_settings) - sizeof(dol_settings->arg_command_line))
				throw "failed to get argument file info or file too small";

			//read the argument count,AHBPROT bit and arg lenght
			s32 ret = ISFS_Read( fd, dol_settings, status->file_length);
			if( ret < (s32)status->file_length)
				throw ("failed to read arguments file(" + std::to_string(ret) + ")");

			if(dol_settings->HW_AHBPROT_bit != 1 && dol_settings->HW_AHBPROT_bit != 0 )
			{
				ISFS_Close(fd);
				ISFS_Delete(ARGUMENT_NAND_PATH);
				throw "invalid HW_AHBPROT bit";
			}

			argv = (__argv*)mem_align(32,sizeof(__argv));
			if(!argv)
			{
				error = ERROR_MALLOC;
				throw "failed to allocate memory for the arguments";
			}
			memset(argv,0,sizeof(__argv));

			argv->argvMagic = 0;
			argv->commandLine = NULL;
			argv->argc = 0;
			argv->argv = &argv->commandLine;
			argv->endARGV = argv->argv + 1;
			argv->length = strnlen(BINARY_NAND_PATH,48)+1;

			if(dol_settings->argument_count > 0)
			{
				argv->length += dol_settings->arg_cli_length;
			}

			argv->commandLine = (char*) mem_align(32,argv->length);
			if(!argv->commandLine)
				throw "failed to alloc memory for cli";
			
			memset(argv->commandLine,0,argv->length);

			strncpy(argv->commandLine,BINARY_NAND_PATH,argv->length-1);
			argv->argc = 1;

			if(dol_settings->argument_count > 0)
			{
				STACK_ALIGN(char,arguments,dol_settings->arg_cli_length,32);
				if(!arguments)
					throw "failed to allocate arguments";

				memset(arguments,0,dol_settings->arg_cli_length);
				ISFS_Seek(fd , ((u32)&dol_settings->arg_command_line) - (u32)(dol_settings) ,SEEK_SET);
				if( ISFS_Read( fd, arguments, dol_settings->arg_cli_length ) < dol_settings->arg_cli_length)
					throw "failed to read arguments";
				ISFS_Close(fd);

				memcpy(argv->commandLine+strnlen(BINARY_NAND_PATH,48)+1,arguments,dol_settings->arg_cli_length);
				argv->argc += dol_settings->argument_count;
			}
			//to make sure there is no overflow
			argv->commandLine[argv->length-1] = '\0';
			argv->argv = &argv->commandLine;
			argv->endARGV = argv->argv + 1;
			argv->argvMagic = ARGV_MAGIC; //everything is set so the magic is set so it becomes valid
		}
		catch (const std::string& ex)
		{
			gprintf("AutoBootDol Exception -> %s",ex.c_str());
			memset(dol_settings,0,sizeof(_dol_settings));
		}
		catch (char const* ex)
		{
			gprintf("AutoBootDol Exception -> %s",ex);
			memset(dol_settings,0,sizeof(_dol_settings));
		}
		catch(...)
		{
			gprintf("AutoBootDol Exception was thrown");
			memset(dol_settings,0,sizeof(_dol_settings));
		}

		if(fd > 0)
		{
			ISFS_Close(fd);
			fd = 0;
		}

		if(argv != NULL && argv->argvMagic != ARGV_MAGIC)
		{
			if(argv->commandLine != NULL)
				mem_free(argv->commandLine);

			//something went wrong, so undo everything
			mem_free(argv);
		}

		//read the binary
		fd = ISFS_Open(BINARY_NAND_PATH, ISFS_OPEN_READ );
		if( fd < 0 )
		{
			error = ERROR_BOOT_DOL_OPEN;
			throw "failed to open file";
		}

		memset(status,0,sizeof(fstats));
		if(ISFS_GetFileStats(fd,status) < 0)
		{
			error = ERROR_BOOT_DOL_OPEN;
			throw "failed to get stats of main.bin";
		}

		binary = (u8*)mem_align(32,status->file_length);
		if(!binary)
		{
			error = ERROR_MALLOC;
			throw "failed to allocate memory for binary";
		}

		if(ISFS_Read( fd, binary, status->file_length) < (s32)status->file_length)
		{
			error = ERROR_BOOT_DOL_READ;
			throw "failed to read binary";
		}
		ISFS_Close(fd);

		//load it!
		BootDolFromMem(binary,dol_settings->HW_AHBPROT_bit,argv);
	}
	catch (const std::string& ex)
	{
		gprintf("AutoBootDol Exception -> %s",ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("AutoBootDol Exception -> %s",ex);
	}
	catch(...)
	{
		gprintf("AutoBootDol Exception was thrown");
	}

	if(binary)
		mem_free(binary);
	if(argv != NULL && argv->commandLine != NULL)
		mem_free(argv->commandLine);
	if(argv)
		mem_free(argv);
	if(fd > 0)
		ISFS_Close(fd);

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

//start update
//---------------
	UpdateStruct UpdateFile;
	s32 file_size = 0;
	u8* buffer = NULL;

	try
	{
		file_size = HttpGet("www.dacotaco.com", "/priiloader/versionV2.bin", buffer, NULL);
		s16 httpReply = GetLastHttpReply();
		if( file_size < 0 && httpReply >= 400 && httpReply <= 500)
		{
			gprintf("falling back to .dat");
			file_size = HttpGet("www.dacotaco.com", "/priiloader/versionV2.dat", buffer, NULL);
		}

		switch(file_size)
		{
			case -1:
			case -2:
			case -3:
			case -4:
				throw "update file error: connection error " + std::to_string(file_size);
				break;
			case -7:
				throw "update file error: HTTP " + std::to_string(GetLastHttpReply());
				break;
			default:
				if(file_size < 0)
					throw "update file error: " + std::to_string(file_size);

				if(file_size != (s32)sizeof(UpdateStruct) || buffer == NULL)
					throw "update file error: invalid size of no data";

				break;
		}

		memcpy(&UpdateFile,buffer,sizeof(UpdateStruct));
	}
	catch (const std::string& ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex);
	}
	catch(...)
	{
		gprintf("CheckForUpdate Exception was thrown");
	}

	mem_free(buffer);
	auto shaHash = UpdateFile.prod_sha1_hash;
	u32 emptyHash[5] = { 0 };
	if(memcmp(shaHash, emptyHash, sizeof(emptyHash)) == 0)
	{
		gprintf("update file is empty");
		PrintFormat( 1, TEXT_OFFSET("error getting versions from server"), 224, "error getting versions from server");
		sleep(5);		
		net_deinit();
		return;
	}	

	//generate update list if any
	//----------------------------------------
	file_size = 0;
	u8 redraw = 1;
	s8 cur_off = 0;
	s8 max_off = SGetSetting(SETTING_SHOWRCUPDATES) ? 1 : 0;
	s8 downloadVersion = 0;
	const s8 installVersion = 1;
	const s8 installRC = 2;
	//make a nice list of the updates
	bool isCurrentRCVersion = VERSION_RC > 0;
	bool VersionUpdates = smaller_version(VERSION, UpdateFile.prod_version) || (same_version(VERSION, UpdateFile.prod_version) && isCurrentRCVersion);

	//to make the if short :
	// - rc updates should be enabled
	// - the current RCversion should be less then the online rc
	// - the current version should < the rc OR the version == the rc IF a rc is installed
	bool RCUpdates = SGetSetting(SETTING_SHOWRCUPDATES) && 
						((smaller_version(VERSION, UpdateFile.rc_version) && !isCurrentRCVersion) || 
						(same_version(VERSION, UpdateFile.rc_version) && VERSION_RC < UpdateFile.rc_version.sub_version));
	try
	{
		ClearScreen();
		while(1)
		{
			if(redraw)
			{
				if ( VersionUpdates )
					PrintFormat( cur_off == 0, 16, 64+(16*1), "Update to v%d.%d.%d", UpdateFile.prod_version.major, UpdateFile.prod_version.minor, UpdateFile.prod_version.patch);
				else
					PrintFormat( cur_off == 0, 16, 64+(16*1), "No version updates\n");

				if ( RCUpdates )
					PrintFormat( cur_off==1, 16, 64+(16*2), "Update to v%d.%d.%d rc %d", UpdateFile.rc_version.major, UpdateFile.rc_version.minor, UpdateFile.rc_version.patch, UpdateFile.rc_version.sub_version);
				else
					PrintFormat( cur_off==1, 16, 64+(16*2), "No RC update\n");	

				PrintFormat( 0, TEXT_OFFSET("A(A) Download Update"), rmode->viHeight-80, "A(A) Download Update");
				PrintFormat( 0, TEXT_OFFSET("B(B) Cancel Update"), rmode->viHeight-64, "B(B) Cancel Update");
				redraw = 0;

				if (VersionUpdates == 0 && RCUpdates == 0)
				{
					sleep(2);
					break;
				}
			}

			Input_ScanPads();
			u32 pressed = Input_ButtonsDown();

			if ( pressed & INPUT_BUTTON_B )
				break;
			
			if ( pressed & INPUT_BUTTON_A )
			{
				redraw = 1;
				if(cur_off == 0 && VersionUpdates == 1)
					downloadVersion = installVersion;
				else if (cur_off == 1 && RCUpdates == 1)
					downloadVersion = installRC;

				if(downloadVersion)
					break;				
			}
			if ( pressed & INPUT_BUTTON_UP )
			{
				cur_off--;
				if(cur_off < 0)
					cur_off = RCUpdates ? 1 : 0;
				redraw = 1;
			}
			if ( pressed & INPUT_BUTTON_DOWN )
			{
				cur_off++;
				if(cur_off > max_off)
					cur_off = 0;
				redraw = 1;
			}
			VIDEO_WaitVSync();
		}
	}
	catch (const std::string& ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex);
	}
	catch(...)
	{
		gprintf("CheckForUpdate Exception was thrown");
	}

	if(downloadVersion == 0)
	{
		net_deinit();
		return;
	}

	bool confirmed = false;
	try
	{
		ClearScreen();
		//Download changelog and ask to proceed or not
		//------------------------------------------------------
		gprintf("downloading changelog...");
		const auto changelogFile = downloadVersion == installRC 
			? "/priiloader/changelog_beta.txt"
			: "/priiloader/changelog.txt";
		
		file_size = HttpGet("www.dacotaco.com", changelogFile, buffer, NULL);
		if(file_size < 0)
			throw "changelog error " + std::to_string(file_size) + "/" + std::to_string(GetLastHttpReply());

		//we are dealing with a string, so play it safe
		buffer[file_size-1] = 0;
		const std::string newlineType = strpbrk((char*)buffer , "\r\n") 
			? "\r\n"
			: "\n";
		u16 min_line = 0;
		u16 max_line = VI_TVMODE_ISFMT(rmode->viTVMode, VI_NTSC) || CONF_GetEuRGB60() || CONF_GetProgressiveScan()
			? 12
			: 17;
		redraw = 1;
		std::vector<std::string> lines;
		std::string_view stringView = (char*)buffer;
		for (auto found = stringView.find(newlineType); found != std::string_view::npos; found = stringView.find(newlineType))
		{
			lines.emplace_back(stringView, 0, found);
			stringView.remove_prefix(found + 1);
		}

		if (!stringView.empty()) 
			lines.emplace_back(stringView);

		mem_free(buffer);
		if( max_line >= lines.size() )
			max_line = lines.size()-1;

		PrintFormat( 1, TEXT_OFFSET(" Changelog "), 64+(16*1), " Changelog ");
		PrintFormat( 1, TEXT_OFFSET("-----------"), 64+(16*2), "-----------");
		if((lines.size() -1) > max_line)
		{
			PrintFormat( 0, TEXT_OFFSET("Up    Scroll Up        "), rmode->viHeight-112, "Up    Scroll Up");
			PrintFormat( 0, TEXT_OFFSET("Down  Scroll Down      "), rmode->viHeight-96, "Down  Scroll Down");
		}
		PrintFormat( 0, TEXT_OFFSET("A(A)  Proceed(Download)"), rmode->viHeight-80, "A(A)  Proceed(Download)");
		PrintFormat( 0, TEXT_OFFSET("B(B)  Cancel Update    "), rmode->viHeight-64, "B(B)  Cancel Update    ");

		u32 pressed;
		while(1)
		{
			Input_ScanPads();

			pressed = Input_ButtonsDown();
			if ( pressed & INPUT_BUTTON_A || pressed & INPUT_BUTTON_B )
			{
				confirmed = pressed & INPUT_BUTTON_A;
				ClearScreen();
				break;
			}
			if ( pressed & INPUT_BUTTON_DOWN && (min_line+max_line) < lines.size()-1)
			{
				min_line++;
				redraw = true;
			}
			if ( pressed & INPUT_BUTTON_UP && min_line > 0)
			{
				min_line--;
				redraw = true;
			}
			if ( redraw )
			{
				if((s32)lines.size() -1 > max_line && (min_line != (s32)lines.size() - max_line - 1) )
				{
					PrintFormat( 0,TEXT_OFFSET("-----More-----"),64+(max_line+2)*16,"-----More-----");
				}
				if(min_line > 0 )
				{
					PrintFormat( 0,TEXT_OFFSET("-----Less-----"),64,"-----Less-----");
				}
				for(s16 i= min_line; i<=(min_line + max_line); i++ )
				{
					PrintFormat( 0, 16, 64 + ((i-min_line+1)*16), "                                                                ");
					PrintFormat( 0, 16, 64 + ((i-min_line+1)*16), lines[i].c_str());
				}
				redraw = false;
			}
			VIDEO_WaitVSync();
		}
	}
	catch (const std::string& ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex.c_str());
		PrintFormat( 1, TEXT_OFFSET("error getting changelog from server"), 224, "error getting changelog from server");
		sleep(2);
	}
	catch (char const* ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex);
		PrintFormat( 1, TEXT_OFFSET("error getting changelog from server"), 224, "error getting changelog from server");
		sleep(2);
	}
	catch(...)
	{
		gprintf("CheckForUpdate Exception was thrown");
		PrintFormat( 1, TEXT_OFFSET("error getting changelog from server"), 224, "error getting changelog from server");
		sleep(2);
	}

	mem_free(buffer);
	if(!confirmed)
	{
		net_deinit();
		return;
	}	

	//The choice is made. lets download what the user wanted :)
	//--------------------------------------------------------------
	try
	{
		ClearScreen();
		const auto filename = downloadVersion == installRC 
			? "/priiloader/Priiloader_Beta.dol"
			: "/priiloader/Priiloader_Update.dol";
		
		gprintf("downloading %s",downloadVersion == installRC?"rc":"update");
		gprintf("downloading %s", filename);
		if(downloadVersion == installRC )
			PrintFormat( 1, TEXT_OFFSET("downloading   .   rc   ..."), 208, "downloading %d.%d.%d rc %d...", UpdateFile.rc_version.major, UpdateFile.rc_version.minor, UpdateFile.rc_version.patch, UpdateFile.rc_version.sub_version);
		else
			PrintFormat( 1, TEXT_OFFSET("downloading   .  ..."), 208, "downloading %d.%d.%d ...", UpdateFile.prod_version.major, UpdateFile.prod_version.minor, UpdateFile.prod_version.patch);

		file_size = HttpGet("www.dacotaco.com", filename, buffer, NULL);
		switch(file_size)
		{
			case -1:
			case -2:
			case -3:
			case -4:
				throw "update error: connection error " + std::to_string(file_size);
				break;
			case -7:
				throw "update error: HTTP " + std::to_string(GetLastHttpReply());
				break;
			default:
				if(file_size < 0)
					throw "update error: " + std::to_string(file_size);
				break;
		}

		u32 FileHash[5] ATTRIBUTE_ALIGN(32) = {};
		SHA_Init();
		if ( SHA_Calculate(buffer, file_size, FileHash) < 0)
			throw "failed to compute hash of update";

		auto onlineHash = downloadVersion == installRC
			? UpdateFile.rc_sha1_hash
			: UpdateFile.prod_sha1_hash;
		gprintf("Downloaded Update : %08X %08X %08X %08X %08X", FileHash[0], FileHash[1], FileHash[2], FileHash[3], FileHash[4]);
		gprintf("Online : %08X %08X %08X %08X %08X", onlineHash[0], onlineHash[1], onlineHash[2], onlineHash[3], onlineHash[4]);

		if(memcmp(FileHash, onlineHash, sizeof(FileHash)) != 0)
			throw "File not the same : hash check failure!";
		
		//Load the dol
		//---------------------------------------------------
		ClearScreen();
		if(downloadVersion == installRC)
			PrintFormat( 1, TEXT_OFFSET("loading   .   rc   ..."), 208, "loading %d.%d.%d rc %d...", UpdateFile.rc_version.major, UpdateFile.rc_version.minor, UpdateFile.rc_version.patch, UpdateFile.rc_version.sub_version);
		else
			PrintFormat( 1, TEXT_OFFSET("loading   .  ..."), 208, "loading %d.%d.%d ...", UpdateFile.prod_version.major, UpdateFile.prod_version.minor, UpdateFile.prod_version.patch);

		sleep(1);
		//load the fresh installer
		BootDolFromMem(buffer,1,NULL);
		PrintFormat( 1, TEXT_OFFSET("Error Booting Update dol"), 224, "Error Booting Update dol");
		sleep(5);
	}
	catch (const std::string& ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex.c_str());
		PrintFormat( 1, TEXT_OFFSET("error getting file from server"), 224, "error getting file from server");
		sleep(2);
	}
	catch (char const* ex)
	{
		gprintf("CheckForUpdate Exception -> %s",ex);
		PrintFormat( 1, TEXT_OFFSET("error getting file from server"), 224, "error getting file from server");
		sleep(2);
	}
	catch(...)
	{
		gprintf("CheckForUpdate Exception was thrown");
		PrintFormat( 1, TEXT_OFFSET("error getting file from server"), 224, "error getting file from server");
		sleep(2);
	}

	net_deinit();
	SHA_Close();
	mem_free(buffer);
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
			gprintf("AutoBoot:System Menu");
			BootMainSysMenu();
			break;
		case AUTOBOOT_HBC:
			gprintf("AutoBoot:Homebrew Channel");
			LoadHBC();
			break;
		case AUTOBOOT_BOOTMII_IOS:
			gprintf("AutoBoot:BootMii IOS");
			LoadBootMii();
			error=ERROR_BOOT_BOOTMII;
			break;
		case AUTOBOOT_FILE:
			gprintf("AutoBoot:Installed File");
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
	vu32* addr = (vu32*)MAGIC_WORD_ADDRESS_1;	
	while (addr != NULL)
	{
		//0x4461636f = "Daco" in hex, 0x50756e65 = "Pune", 0x41627261 = "Abra"  
		if(*addr == 0x4461636f)
			return MAGIC_WORD_DACO;
		else if(*addr == 0x50756e65)
			return MAGIC_WORD_PUNE;
		else if(*addr == 0x41627261)
			return MAGIC_WORD_ABRA;

		if (addr != (vu32*)MAGIC_WORD_ADDRESS_2)
			addr = (vu32*)MAGIC_WORD_ADDRESS_2;
		else
			break;
	}
	return 0;
}
void ClearMagicWord( void )
{
	*(vu32*)MAGIC_WORD_ADDRESS_1 = 0x00000000;
	DCFlushRange((void*)MAGIC_WORD_ADDRESS_1,4);
	*(vu32*)MAGIC_WORD_ADDRESS_2 = 0x00000000;
	DCFlushRange((void*)MAGIC_WORD_ADDRESS_2,4);
	return;
}

int main(int argc, char **argv)
{
	CheckForGecko();
#ifdef DEBUG
	InitGDBDebug();
#endif
	gprintf("priiloader");
	gprintf("Built   : %s %s", __DATE__, __TIME__ );
	gprintf("Version : %d.%d.%d (rev %s)", VERSION.major, VERSION.minor, VERSION.patch, GIT_REV_STR);
	gprintf("Firmware: %d.%d.%d", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

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
	gprintf("\"Magic Priiloader word\": %x - %x",*(vu32*)MAGIC_WORD_ADDRESS_2 ,*(vu32*)MAGIC_WORD_ADDRESS_1);

	bool isFirstTimeUse = LoadSettings() == LOADSETTINGS_INI_CREATED;
	if(SGetSetting(SETTING_DUMPGECKOTEXT) == 1)
	{
		InitMounts(_mountCallback);
	}

	SetDumpDebug(SGetSetting(SETTING_DUMPGECKOTEXT));
	SetVideoInterfaceConfig(NULL);
	s16 Bootstate = CheckBootState();
	u32 GcShutdownFlag = *(u32*)0x80003164;
	gprintf("BootState:%d", Bootstate );
	memset(&system_state,0,sizeof(wii_state));
	STACK_ALIGN(StateFlags, flags, sizeof(StateFlags), 32);
	GetStateFlags(flags);
	gprintf("Bootstate %u detected. DiscState %u ,ReturnTo %u & Flags %u & checksum %u (gcflag : 0x%08X)", flags->type, flags->discstate, flags->returnto, flags->flags, flags->checksum, GcShutdownFlag);
	s8 magicWord = CheckMagicWords();

	if (CheckvWii())
	{
		ImportWiiUConfig();
		const WiiUConfig* wiiuConfig = GetWiiUConfig();

		// Read AR from Wii U settings if it exists
		s32 ar;
		if (wiiuConfig)
			ar = wiiuConfig->av.ipl_ar;
		else
			ar = CONF_GetAspectRatio();

		// Now set the correct DMCU aspect ratio
		STM_DMCUWrite(ar ? 0x30000004 : 0x10000002);

		// bit of a hack but BC-NAND clears the boot state, so let's try to guess it
		if (!IsInitialBoot())
			Bootstate = TYPE_RETURN;
		else
			Bootstate = TYPE_UNKNOWN;

		flags->type = Bootstate;
		SetBootState(flags->type, flags->flags, flags->returnto, flags->discstate);

		// Check if Wii U side wants to boot into a channel starting with 'PRII'
		const WiiUArgs* wiiuArgs = GetWiiUArgs();
		if (wiiuArgs && (wiiuArgs->flags & 0x10) && (wiiuArgs->title_id >> 32) == 0x50524949)
		{
			uint32_t magicTid = (uint32_t) wiiuArgs->title_id;
			//0x4461636f = "Daco" in hex, 0x50756e65 = "Pune", 0x41627261 = "Abra"  
			if(magicTid == 0x4461636f)
				magicWord = MAGIC_WORD_DACO;
			else if(magicTid == 0x50756e65)
				magicWord = MAGIC_WORD_PUNE;
			else if(magicTid == 0x41627261)
				magicWord =  MAGIC_WORD_ABRA;

			// Clear args
			WiiUArgs newArgs;
			memset(&newArgs, 0, sizeof(newArgs));
			newArgs.magic = WIIU_MAGIC;
			SetWiiUArgs(&newArgs);
		}
	}

	// before anything else, poll input devices such as keyboards to see if we should stop autoboot
	r = Input_Init();
	gprintf("Input_Init():%d", r );
	
	WPAD_SetPowerButtonCallback(HandleWiiMoteEvent);
	
	//Give keyboard stack enough time to detect a keyboard and poll it. (~0.2s delay)
	usleep(500000);
	Input_ScanPads();
	
	//Check reset button state, boot to priiloader if first time
	if(!isFirstTimeUse && ((Input_ButtonsDown() & INPUT_BUTTON_B) == 0) && RESET_UNPRESSED == 1 && magicWord == 0)
	{
		//Check autoboot settings
		switch( Bootstate )
		{
			case TYPE_SHUTDOWNSYSTEM: // 5 - shutdown
				ClearState();
				//check for valid nandboot shitzle. if its found we need to change bootstate to 4.
				//yellow8 claims system menu reset everything then, but it didn't on my wii (joy). this is why its not activated code.
				//its not fully confirmed system menu does it(or ios while being standby) and if system menu does indeed clear it.
				/*if(VerifyNandBootInfo())
				{
					gprintf("Verifty of NandBootInfo : 1\nbootstate changed to %d",CheckBootState());
				}
				else
				{
					gprintf("Verifty of NandBootInfo : 0");
				}*/
				if(SGetSetting(SETTING_SHUTDOWNTO) == SHUTDOWNTO_AUTOBOOT)
				{
					//a function asked for by Abraham Anderson. i understood his issue (emulators being easy to shutdown but not return to loader, making him want to shutdown to loader) 
					//but still think its stupid. however, days of the wii are passed and i hope no idiot is going to screw his wii with this and then nag to me...
					gprintf("booting autoboot instead of shutting down...");
					Autoboot_System();
				}
				else if(SGetSetting(SETTING_SHUTDOWNTO) == SHUTDOWNTO_NONE)
				{
					gprintf("Shutting down...\n");
					DVDStopDriveAsync();
					ShutdownMounts();
					Input_Shutdown();
					USB_Deinitialize();
					*(vu32*)0xCD8000C0 &= ~0x20;
					while(DVDAsyncBusy());
					DVDCloseHandle();
					if( SGetSetting(SETTING_IGNORESHUTDOWNMODE) )
						STM_ShutdownToStandby();
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
				break;
			case TYPE_REBOOT: //2 - normal reboot which funny enough doesn't happen very often
			case TYPE_RETURN: //3 - return to system menu
				switch( SGetSetting(SETTING_RETURNTO) )
				{
					case RETURNTO_SYSMENU:
						gprintf("ReturnTo:System Menu");
						BootMainSysMenu();
						break;

					case RETURNTO_AUTOBOOT:
						Autoboot_System();
						break;

					case RETURNTO_FILE:
						AutoBootDol();
						break;

					default:
						break;
				}
				break;
			case TYPE_UNKNOWN: //255 or -1, only seen when shutting down from MIOS or booting dol from HBC. it is actually an invalid value	
				if (flags->flags == FLAGS_STARTGCGAME && GcShutdownFlag != 0) //&& temp.discstate != 2)
				{
					//if the flag is 0x82 & the GcShutdown flag is set, its probably shutdown from mios. in that case system menu 
					//will handle it perfectly (and i quote from SM's OSreport : "Shutdown system from GC!")
					//it seems to reboot into bootstate 5. but its safer to let SM handle it
					gprintf("255 : System Menu");
					BootMainSysMenu();
				}
			case TYPE_NANDBOOT: // 4 - nandboot
				//apparently a boot state in which the system menu auto boots a title. read more : http://wiibrew.org/wiki/WiiConnect24#WC24_title_booting
				//as it hardly happens i guess nothing bad can happen if we ignore it and just do autoboot instead :)
			case TYPE_EJECTDISC: // 1 - Boot when fully shutdown & wiiconnect24 is off. why its called TYPE_EJECTDISC i have no clue...
			case TYPE_WC24: // 0 - boot when wiiconnect24 is on
				Autoboot_System();
				break;
			default :
				if( ClearState() < 0 )
				{
					error = ERROR_STATE_CLEAR;
					gprintf("ClearState failure");
				}
				break;

		}
	}
	//remove the "Magic Priiloader word" cause it has done its purpose
	//Plan B address : 0x93FFFFFA
	if(magicWord == MAGIC_WORD_DACO)
 	{
		gprintf("\"Magic Priiloader Word\" 'Daco' found!");
		gprintf("clearing memory of the \"Magic Priiloader Word\"");
		ClearMagicWord();		
	}
	else if(magicWord == MAGIC_WORD_PUNE)
	{
		//detected the force for sys menu
		gprintf("\"Magic Priiloader Word\" 'Pune' found!");
		gprintf("clearing memory of the \"Magic Priiloader Word\" and starting system menu...");
		ClearMagicWord();
		BootMainSysMenu();
	}
	else if( magicWord == MAGIC_WORD_ABRA )
	{
		//detected the force for autoboot
		gprintf("\"Magic Priiloader Word\" 'Abra' found!");
		gprintf("clearing memory of the \"Magic Priiloader Word\" and starting Autorun setting...");
		ClearMagicWord();
		Autoboot_System();
	}
	else if( (
			( SGetSetting(SETTING_AUTBOOT) != AUTOBOOT_DISABLED && ( Bootstate < 2 || Bootstate == 4 ) ) 
		 || ( SGetSetting(SETTING_RETURNTO) != RETURNTO_PRIILOADER && Bootstate > 1 && Bootstate < 4 ) 
		 || ( SGetSetting(SETTING_SHUTDOWNTO) == SHUTDOWNTO_NONE && Bootstate == 5 ) ) 
		 && error == 0 )
	{
		gprintf("Reset Button is held down");
	}
	
	system_state.Init = 1;
	InitMounts(_mountCallback);
	gprintf("FAT_Init():%d", GetMountedFlags());

	//init video first so we can see crashes :)
	InitVideo();
  	if( SGetSetting(SETTING_PASSCHECKPRII) )
 		password_check();

	ClearScreen();

	s8 cur_off=0;
	s8 redraw=true;
	u32 SysVersion= GetSysMenuVersion();

	if( SGetSetting(SETTING_STOPDISC) )
	{
		DVDStopDrive();
		DVDCloseHandle();
	}
	_sync();
#ifdef DEBUG
	gdprintf("priiloader v%d.%d.%d DEBUG (Sys:%d)(IOS:%d)(%s %s)", VERSION.major, VERSION.minor, VERSION.patch, SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
#elif VERSION_RC > 0
	gprintf("priiloader v%d.%d.%d RC %d (Sys:%d)(IOS:%d)(%s %s)", VERSION.major, VERSION.minor, VERSION.patch, VERSION.sub_version, SysVersion, (*(vu32*)0x80003140)>>16, __DATE__, __TIME__);
#endif

	system_state.InMainMenu = 1;
	while(1)
	{
		_mountChanged = 0;
		Input_ScanPads();
		u32 INPUT_Pressed = Input_ButtonsDown();
 
		if ( INPUT_Pressed & INPUT_BUTTON_A )
		{
			if(cur_off < 5)
				ClearScreen();

			error = ERROR_NONE;
			system_state.InMainMenu = 0;
			redraw=true;

			switch(cur_off)
			{
				case 0:
					BootMainSysMenu();
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
				case 4: //Launch Disc
					BootDvdDrive();
					break;
				case 5:	//load main.bin from /title/00000001/00000002/data/ dir
					AutoBootDol();
					break;
				case 6:
					if((INPUT_Pressed & INPUT_BUTTON_STM) != 0)
					{
						error = ERROR_SYSMENU_FRONT_BUTTONS_FORBIDDEN;
						break;
					}

					ClearScreen();
					InstallLoadDOL();
					break;
				case 7:
					if((INPUT_Pressed & INPUT_BUTTON_STM) != 0)
					{
						error = ERROR_SYSMENU_FRONT_BUTTONS_FORBIDDEN;
						break;
					}

					ClearScreen();
					SysHackHashSettings();
					break;
				case 8:
					ClearScreen();
					CheckForUpdate();
					net_deinit();
					break;
				case 9:
					//when using the front buttons, we will refusing going into the password menu
					if((INPUT_Pressed & INPUT_BUTTON_STM) != 0)
					{
						error = ERROR_SYSMENU_FRONT_BUTTONS_FORBIDDEN;
						break;
					}
					
					ClearScreen();
					InstallPassword();
					redraw=true;
					break;
				case 10:
					SetSettings();
					break;
				default:
					break;

			}
			system_state.InMainMenu = 1;

			if(redraw == true)
				ClearScreen();		
		}

		if ( INPUT_Pressed & INPUT_BUTTON_DOWN )
		{
			cur_off++;

			if (cur_off >= (error == ERROR_UPDATE ? 12 : 11))
				cur_off = 0;

			redraw=true;
		} else if ( INPUT_Pressed & INPUT_BUTTON_UP )
		{
			cur_off--;

			if( cur_off < 0 )
				cur_off = (error == ERROR_UPDATE ? 12 : 11) - 1;

			redraw=true;
		}

		if( redraw )
		{
			PrintFormat( 0, 16, rmode->viHeight-96, "IOS v%d", (*(vu32*)0x80003140)>>16 );
			PrintFormat( 0, 16, rmode->viHeight-80, "Systemmenu v%d", SysVersion );
#if VERSION_RC > 0
			PrintFormat( 0, 16, rmode->viHeight - 64, "Priiloader v%d.%d.%d(RC%d)", VERSION.major, VERSION.minor, VERSION.patch, VERSION.sub_version);
#else
			PrintFormat (0, 16, rmode->viHeight - 64, "Priiloader v%d.%d.%d (r0x%08x)", VERSION.major, VERSION.minor, VERSION.patch, GIT_REV);
#endif

			PrintFormat( cur_off==0, TEXT_OFFSET("System Menu"), 64, "System Menu");
			PrintFormat( cur_off==1, TEXT_OFFSET("Homebrew Channel"), 80, "Homebrew Channel");
			PrintFormat( cur_off==2, TEXT_OFFSET("BootMii IOS"), 96, "BootMii IOS");
			PrintFormat( cur_off==3, TEXT_OFFSET("Launch Title"), 112, "Launch Title");
			PrintFormat( cur_off==4, TEXT_OFFSET("Launch Disc"), 128, "Launch Disc");

			PrintFormat( cur_off==5, TEXT_OFFSET("Installed File"), 160, "Installed File");
			PrintFormat( cur_off==6, TEXT_OFFSET("Load/Install File"), 176, "Load/Install File");
			PrintFormat( cur_off==7, TEXT_OFFSET("System Menu Hacks"), 192, "System Menu Hacks");
			PrintFormat( cur_off==8, TEXT_OFFSET("Check For Update"),208,"Check For Update");
			PrintFormat( cur_off==9, TEXT_OFFSET("Set Password"), 224, "Set Password");
			PrintFormat( cur_off==10, TEXT_OFFSET("Settings"), 240, "Settings");

			if(error == ERROR_REFRESH)
			{
				PrintFormat( 0, 16, (rmode->viHeight)-128, "                                                             ");
				PrintFormat( 0, 16, (rmode->viHeight)-114, "                                                             ");
				error = ERROR_NONE;
			}
			else if (error > ERROR_NONE)
			{
				ShowError();
				if(error == ERROR_SYSMENU_FRONT_BUTTONS_FORBIDDEN)
					error = ERROR_REFRESH;
				else
					error = ERROR_NONE;
			}
			redraw = false;
		}
		if( system_state.Shutdown )
		{
			*(vu32*)0xCD8000C0 &= ~0x20;
			ClearState();
			VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
			DVDStopDrive();
			DVDCloseHandle();
			Input_Shutdown();
			ShutdownMounts();
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
		VIDEO_WaitVSync();
	}

	return 0;
}
