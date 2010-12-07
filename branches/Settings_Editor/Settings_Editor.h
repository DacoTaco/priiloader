#ifndef _SETTINGS_EDITOR_H_
#define _SETTINGS_EDITOR_H_

#include "typedefs.h"
#include <stdio.h>
#include <tchar.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

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
	u8 ShowGeckoText;
	u8 PasscheckPriiloader;
	u8 PasscheckMenu;
	u32 ShowBetaUpdates;
	u8 UseClassicHacks;
} Settings_6;

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
	u8 ShowGeckoText;
	u8 PasscheckPriiloader;
	u8 PasscheckMenu;
	u32 ShowBetaUpdates;
} Settings_5;

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
		SETTING_SETTING_SHOWGECKOTEXT
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

bool Data_Need_Swapping( void );
inline void endian_swap(unsigned int& x);

#endif
