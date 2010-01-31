#ifndef _SETTINGS_EDITOR_H_
#define _SETTINGS_EDITOR_H_

#include "typedefs.h"

typedef struct {
	u32 fill;
	u32 version;
}Settings_Version;

/*
//for this to work we need to convert everything. fuck that i say :P
typedef struct {
	unsigned int autoboot;
	unsigned int version;
	unsigned int clean_level;
	unsigned int ReturnTo;
	unsigned int ShutdownToPreloader;
	unsigned int StopDisc;
	unsigned int ShowBetaUpdates;
	unsigned int LidSlotOnError;
	unsigned int IgnoreShutDownMode;
	unsigned int BetaVersion;
	unsigned int SystemMenuIOS;
	char padding[16];
	bool UseSystemMenuIOS;
	bool BlackBackground;
} Settings_3;*/

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
		SETTING_SHOWDEBUGTEXT
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
