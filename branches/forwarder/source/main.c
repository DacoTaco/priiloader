#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ogc/machine/processor.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <ogc/usb.h>

#include "dol.h"
#include "elf.h"
#include "video.h"
// Apps goes here
#define EXECUTABLE_MEM_ADDR 0x92000000

extern void __exception_closeall();
extern bool sdio_Deinitialize();
extern bool sdio_Startup();
typedef void (*entrypoint) (void);

/* Path to load */
char *exePath1 = "fat:/apps/usbloader_gx/boot.dol";
char *exePath2 ="fat:/apps/usbloader_gx/boot.elf";
char *exePath3 = "fat:/apps/usbloader_gx/boot.DOL";
char *exePath4 ="fat:/apps/usbloader_gx/boot.ELF";
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* don't change anything from here */
bool MountDevicesSD(void)
{
	return fatMountSimple("fat",&__io_wiisd);
}
bool MountDevicesUSB(void)
{
	return fatMountSimple("fat", &__io_usbstorage);
}
void ShutdownDevices()
{
	fatUnmount("fat:/");
	__io_wiisd.shutdown();
	__io_usbstorage.shutdown();
}

int main(int argc, char **argv) {
/* some needed vars */
	u32 cookie;
	FILE *exeFile = NULL;
	void *exeBuffer          = (void *)EXECUTABLE_MEM_ADDR;
	int exeSize              = 0;
	int exeValid;
	u32 exeEntryPointAddress = 0;
	entrypoint exeEntryPoint;

	WPAD_Init();
	initVideoSubsys();

/* Open dol/elf File and check exist */
	if( MountDevicesSD() )
	{
		if (fopen (exePath1 ,"rb") != NULL)
			exeFile = fopen (exePath1 ,"rb");
		else if(fopen (exePath2 ,"rb") != NULL)
			exeFile = fopen (exePath2 ,"rb");
		else if(fopen (exePath3 ,"rb") != NULL)
			exeFile = fopen (exePath3 ,"rb");
		else if(fopen (exePath4 ,"rb") != NULL)
			exeFile = fopen (exePath4 ,"rb");
		else
			ShutdownDevices();
	}

	if( exeFile == NULL && MountDevicesUSB() )
	{
		if (fopen (exePath1 ,"rb") != NULL)
			exeFile = fopen (exePath1 ,"rb");
		else if(fopen (exePath2 ,"rb") != NULL)
			exeFile = fopen (exePath2 ,"rb");
		else if(fopen (exePath3 ,"rb") != NULL)
			exeFile = fopen (exePath3 ,"rb");
		else if(fopen (exePath4 ,"rb") != NULL)
			exeFile = fopen (exePath4 ,"rb");
		else
			ShutdownDevices();
	}

	if( exeFile == NULL )
	{
		printf("File not found....\n");
		goto error;
	}

/* read whole file in */
	fseek (exeFile, 0, SEEK_END);
	exeSize = ftell(exeFile);
	fseek (exeFile, 0, SEEK_SET);
	if(fread (exeBuffer, 1, exeSize, exeFile) != exeSize) {
		printf("Can't open file...\n");
		goto error;
	}
	fclose (exeFile);

/* load entry point */
	struct __argv args[10];
	exeValid = valid_elf_image(exeBuffer);
	if( exeValid < 0 ) {
		printf("File isn't a valide DOL/ELF...\n.");
		goto error;
	}
 	if ( exeValid == 1 ){
		exeEntryPointAddress = load_elf_image(exeBuffer);
	}else {
		exeEntryPointAddress = load_dol_image(exeBuffer, args);
	}
	ShutdownDevices();
	sdio_Deinitialize();
	if (exeEntryPointAddress == 0) {
		printf("EntryPointAddress mismatch...\n");
		goto error;
	}
	exeEntryPoint = (entrypoint) exeEntryPointAddress;

/* cleaning up and load dol */
	__IOS_ShutdownSubsystems ();
	_CPU_ISR_Disable (cookie);
	__exception_closeall ();
	exeEntryPoint ();   //Jump :D
	_CPU_ISR_Restore (cookie);
	return 0;

/* error Code from Multi-Linker @ http://www.aamon.net/wii-homebrew/source/multi-linker/source/ THX */
	error:
	printf ("Press HOME to exit...\n");
	while(1) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if ( pressed & WPAD_BUTTON_HOME ) SYS_ResetSystem(SYS_RESTART,0,0);
		VIDEO_WaitVSync();
	}
	return 0;
}
