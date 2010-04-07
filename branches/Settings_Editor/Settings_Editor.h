#ifndef _SETTINGS_EDITOR_H_
#define _SETTINGS_EDITOR_H_

#include "typedefs.h"
#include <stdio.h>
#include <tchar.h>

typedef struct {
	u32 fill;
	u32 version;
}Settings_Version;

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
	u8 ShowGeckoOutput;
	u8 PasscheckPriiloader;
	u8 PasscheckMenu;
} Settings_4;

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
	u8 ShowDebugText;
} Settings_3;

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
		SETTING_SHOWGECKOOUTPUT
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
		RETURNTO_AUTOBOOT,
};

#endif
