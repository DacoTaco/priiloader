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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gccore.h>
#include <string.h>
#include <malloc.h>

#include "settings.h"
#include "error.h"
#include "gecko.h"

Settings *settings=NULL;
extern u8 error;
u32 GetSysMenuVersion( void )
{
	//Get sysversion from TMD
	u64 TitleID = 0x0000000100000002LL;
	u32 tmd_size;
	s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("SysMenuVersion : GetTMDViewSize error %d\n",r);
		return 0;
	}

	tmd_view *rTMD = (tmd_view*)memalign( 32, (tmd_size+31)&(~31) );
	if( rTMD == NULL )
	{
		gprintf("SysMenuVersion : memalign failure\n");
		return 0;
	}
	memset(rTMD,0, (tmd_size+31)&(~31) );
	r = ES_GetTMDView(TitleID, (u8*)rTMD, tmd_size);
	if(r<0)
	{
		gprintf("SysMenuVersion : GetTMDView error %d\n",r);
		free_pointer( rTMD );
		return 0;
	}	
	u32 version = rTMD->title_version;
	if(rTMD)
	{
		free_pointer(rTMD);
	}
	return version;
}

u32 GetSysMenuIOS( void )
{
	//Get sysversion from TMD
	u64 TitleID = 0x0000000100000002LL;
	u32 tmd_size;

	s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("GetSysMenuIOS : GetTMDViewSize error %d\n",r);
		return 0;
	}

	tmd_view *rTMD = (tmd_view*)memalign( 32, (tmd_size+31)&(~31) );
	if( rTMD == NULL )
	{
		gprintf("GetSysMenuIOS : memalign failure\n");
		return 0;
	}
	memset(rTMD,0, (tmd_size+31)&(~31) );
	r = ES_GetTMDView(TitleID, (u8*)rTMD, tmd_size);
	if(r<0)
	{
		gprintf("GetSysMenuIOS : GetTMDView error %d\n",r);
		free_pointer( rTMD );
		return 0;
	}
	u8 IOS = rTMD->title_version;
	if(rTMD)
	{
		free_pointer(rTMD);
	}
	return IOS;
}

u32 SGetSetting( u32 s )
{
	if( settings == NULL )
		return 0;

	switch( s )
	{
		case SETTING_AUTBOOT:
			return settings->autoboot;

		case SETTING_RETURNTO:
			return settings->ReturnTo;

		case SETTING_SHUTDOWNTOPRELOADER:
			return settings->ShutdownToPreloader;

		case SETTING_STOPDISC:
			return settings->StopDisc;

		case SETTING_LIDSLOTONERROR:
			return settings->LidSlotOnError;

		case SETTING_IGNORESHUTDOWNMODE:
			return settings->IgnoreShutDownMode;

		case SETTING_SYSTEMMENUIOS:
			return settings->SystemMenuIOS;

		case SETTING_USESYSTEMMENUIOS:
			return settings->UseSystemMenuIOS;

		case SETTING_BLACKBACKGROUND:
			return settings->BlackBackground;
		case SETTING_SHOWGECKOTEXT:
			return settings->ShowGeckoText;
		case SETTING_PASSCHECKPRII:
			return settings->PasscheckPriiloader;
		case SETTING_PASSCHECKMENU:
			return settings->PasscheckMenu;
		case SETTING_SHOWBETAUPDATES:
			return settings->ShowBetaUpdates;
		case SETTING_CLASSIC_HACKS:
			return settings->UseClassicHacks;
		default:
			return 0;
		break;
	}
}
void LoadSettings( void )
{
	if(settings == NULL)
	{
		//the settings still need to be aligned/allocated. so lets do that
		settings = (Settings*)memalign( 32, (sizeof( Settings )+31)&(~31));
	}
	memset( settings, 0, sizeof( Settings ) );
	
	s32 fd = ISFS_Open("/title/00000001/00000002/data/loader.ini", 1|2 );
	if( fd < 0 )
	{
		//file not found create a new one
		ISFS_CreateFile("/title/00000001/00000002/data/loader.ini", 0, 3, 3, 3);
		//set a few default settings
		settings->version = VERSION;
		settings->UseSystemMenuIOS = true;
		settings->autoboot = AUTOBOOT_SYS;
		settings->ShowBetaUpdates = false;
		fd = ISFS_Open("/title/00000001/00000002/data/loader.ini", 1|2 );
		if( fd < 0 )
		{
			error = ERROR_SETTING_OPEN;
			return;
		}
		if(ISFS_Write( fd, settings, sizeof( Settings ) )<0)
		{
			ISFS_Close( fd );
			error = ERROR_SETTING_WRITE;
			return;
		}
		ISFS_Seek( fd, 0, 0 );
	}
	fstats *status = (fstats*)memalign(32,(sizeof(fstats)+31)&(~31));
	memset(status,0,sizeof(fstats));
	ISFS_GetFileStats(fd,status);
	if ( status->file_length != sizeof(Settings) )
	{
		ISFS_Close(fd);
		gprintf("LoadSettings : status->file_length != struct size , resetting...\n");
		//recreate settings file
		ISFS_Delete("/title/00000001/00000002/data/loader.ini");
		ISFS_CreateFile("/title/00000001/00000002/data/loader.ini", 0, 3, 3, 3);
		//set a few default settings
		settings->version = VERSION;
		settings->UseSystemMenuIOS = true;
		settings->autoboot = AUTOBOOT_SYS;
		fd = ISFS_Open("/title/00000001/00000002/data/loader.ini", 1|2 );
		if( fd < 0 )
		{
			error = ERROR_SETTING_OPEN;
			if(status)
			{
				free_pointer(status);				
			}
			return;
		}
		if(ISFS_Write( fd, settings, sizeof( Settings ) )<0)
		{
			ISFS_Close( fd );
			if(status)
			{
				free_pointer(status);
			}
			error = ERROR_SETTING_WRITE;
			return;
		}
		ISFS_Seek( fd, 0, 0 );
	}
	if(status)
	{
		free_pointer(status);
	}
	if(ISFS_Read( fd, settings, sizeof( Settings ) )<0)
	{
		ISFS_Close( fd );
		error = ERROR_SETTING_READ;
		return;
	}
	if( settings->version == 0 || settings->version != VERSION || settings->BetaVersion == 0 || settings->BetaVersion != BETAVERSION )
	{
		settings->version = VERSION;
		settings->BetaVersion = BETAVERSION;
		ISFS_Seek( fd, 0, 0 );
		ISFS_Write( fd, settings, sizeof( Settings ) );
	}
	ISFS_Close( fd );
	return;
}
int SaveSettings( void )
{
	s32 fd = ISFS_Open("/title/00000001/00000002/data/loader.ini", 1|2 );
	
	if( fd < 0 )
	{
		// musn't happen!
		error = ERROR_SETTING_OPEN;
		return 0;
	}

	ISFS_Seek( fd, 0, 0 );

	s32 r = ISFS_Write( fd, settings, sizeof( Settings ) );
	
	ISFS_Close( fd );

	if( r == sizeof( Settings ) )
		return 1;
	error = ERROR_SETTING_WRITE;
	return r;
}
