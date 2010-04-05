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


#define VERSION		04
//#define BETAVERSION	0x00000003
#ifndef BETAVERSION
#define BETAVERSION	0
#endif	
#include "Global.h"

typedef struct {
	u32 autoboot;
	u32 version;
	u32 ReturnTo;
	u8 ShutdownToPreloader;
	u8 StopDisc;
	u8 LidSlotOnError;
	u8 IgnoreShutDownMode;
	u32 BetaVersion;
	u8 SystemMenuIOS;
	u8 UseSystemMenuIOS;
	u8 BlackBackground;
	u8 ShowGeckoText;
	u8 PasscheckPriiloader;
	u8 PasscheckMenu;
} Settings;
enum {
		SETTING_AUTBOOT,
		SETTING_RETURNTO,
		SETTING_SHUTDOWNTOPRELOADER,
		SETTING_STOPDISC,
		SETTING_SHOWBETAUPDATE,
		SETTING_LIDSLOTONERROR,
		SETTING_IGNORESHUTDOWNMODE,
		SETTING_BETAVERSION,
		SETTING_SYSTEMMENUIOS,
		SETTING_USESYSTEMMENUIOS,
		SETTING_BLACKBACKGROUND,
		SETTING_SHOWGECKOTEXT,
		SETTING_PASSCHECKPRII,
		SETTING_PASSCHECKMENU,
};

enum {
		AUTOBOOT_DISABLED,
		AUTOBOOT_HBC,
		AUTOBOOT_BOOTMII_IOS,
		AUTOBOOT_SYS,
		AUTOBOOT_FILE,
		AUTOBOOT_ERROR
};
enum {	
		RETURNTO_SYSMENU,
		RETURNTO_PRELOADER,
		RETURNTO_AUTOBOOT
};



u32 GetSysMenuVersion( void );
u32 GetSysMenuIOS( void );
u32 SGetSetting( u32 s );
void LoadSettings( void );
int SaveSettings( void );
