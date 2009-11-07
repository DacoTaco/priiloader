/*

preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

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

Settings *settings=NULL;
u32 ShowUpdate=0;
extern u32 error;
u32 GetSysMenuVersion( void )
{
	//Get sysversion from TMD
	static u64 TitleID ATTRIBUTE_ALIGN(32) = 0x0000000100000002LL;
	static u32 tmd_size ATTRIBUTE_ALIGN(32);

	s32 r=ES_GetStoredTMDSize(TitleID, &tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMDSize(%llX, %08X):%d\n", TitleID, (u32)(&tmd_size), r );
#endif
	if(r<0)
		return 0;

	signed_blob *TMD = (signed_blob *)memalign( 32, (tmd_size+32)&(~31) );
	memset(TMD, 0, tmd_size);

	r=ES_GetStoredTMD(TitleID, TMD, tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMD(%llX, %08X, %d):%d\n", TitleID, (u32)(TMD), tmd_size, r );
#endif
	if(r<0)
		return 0;
	
	tmd *rTMD = (tmd *)(TMD+(0x140/sizeof(tmd *)));

	u32 version = rTMD->title_version;

	free( TMD );

	return version;
}

u32 GetSysMenuIOS( void )
{
	//Get sysversion from TMD
	static u64 TitleID ATTRIBUTE_ALIGN(32) = 0x0000000100000002LL;
	static u32 tmd_size ATTRIBUTE_ALIGN(32);

	s32 r=ES_GetStoredTMDSize(TitleID, &tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMDSize(%llX, %08X):%d\n", TitleID, (u32)(&tmd_size), r );
#endif
	if(r<0)
		return 0;

	signed_blob *TMD = (signed_blob *)memalign( 32, (tmd_size+32)&(~31) );
	memset(TMD, 0, tmd_size);

	r=ES_GetStoredTMD(TitleID, TMD, tmd_size);
#ifdef DEBUG
	printf("ES_GetStoredTMD(%llX, %08X, %d):%d\n", TitleID, (u32)(TMD), tmd_size, r );
#endif
	if(r<0)
		return 0;
	
	tmd *rTMD = (tmd *)(TMD+(0x140/sizeof(tmd *)));

	u32 IOS = rTMD->sys_version;

	free( TMD );

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

		case SETTING_SHOWBETAUPDATE:
			return settings->ShowBetaUpdates;

		case SETTING_LIDSLOTONERROR:
			return settings->LidSlotOnError;

		case SETTING_IGNORESHUTDOWNMODE:
			return settings->IgnoreShutDownMode;

		case SETTING_BETAVERSION:
			return settings->BetaVersion;

		case SETTING_SYSTEMMENUIOS:
			return settings->SystemMenuIOS;
		case SETTING_USESYSTEMMENUIOS:
			return settings->UseSystemMenuIOS;
		case SETTING_BLACKBACKGROUND:
			return settings->BlackBackground;
		default:
			return 0;
		break;
	}
}
void LoadSettings( void )
{
	if(!settings)
	{
		//the settings still need to be aligned/allocated. so lets do that
		settings = (Settings*)memalign( 32, sizeof( Settings ) );
	}
	memset( settings, 0, sizeof( Settings ) );
	
	
	s32 fd = ISFS_Open("/title/00000001/00000002/data/loader.ini", 1|2 );

	if( fd < 0 )
	{
		//file not found create a new one
		ISFS_CreateFile("/title/00000001/00000002/data/loader.ini", 0, 3, 3, 3);
		settings->version = VERSION;
		settings->UseSystemMenuIOS = true;
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
