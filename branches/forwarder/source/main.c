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
#include <unistd.h>

#include "dol.h"
#include "elf.h"
#include "video.h"
// Apps goes here
#define EXECUTABLE_MEM_ADDR 0x92000000

/*Pathes to load, sperator is , */
/* if ONE Path is set, it will load this one */
char Path[200] = "apps/usbloader,apps/usbloader_gx,apps/usbloader_cfg,apps/WiiFlow_222,apps/WiiFlow_249,apps/NeoGamma,apps/uLoader,apps/CoverFloader,usbloader";
/* example for loading directly */
// char Path[200] = "apps/WiiFlow_249";

extern void __exception_closeall();
extern bool sdio_Deinitialize();
extern bool sdio_Startup();
typedef void (*entrypoint) (void);

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
void load_forwarder( void )
{
	FILE *forwarder = NULL;
	char buf[200];
	bool for_found = true;
	if( MountDevicesSD() )
	{

		if ( fopen("fat:/priiloader_forwarder.txt", "rb") != NULL)
		{
			forwarder = fopen("fat:/priiloader_forwarder.txt", "rb");
			fgets(buf, 200, forwarder);
			strcpy(Path, buf);
			fclose(forwarder);
			for_found = false;
		}
		ShutdownDevices();
	}

	if( MountDevicesUSB() && for_found)
	{

		if ( fopen("fat:/priiloader_forwarder.txt", "rb") != NULL)
		{
			forwarder = fopen("fat:/priiloader_forwarder.txt", "rb");
			fgets(buf, 200, forwarder);
			strcpy(Path, buf);
			fclose(forwarder);
		}
		ShutdownDevices();
	}
}
		
int main(int argc, char **argv) {
/* some needed vars */
	initVideoSubsys();

	u32 cookie;
	FILE *exeFile = NULL;
	void *exeBuffer          = (void *)EXECUTABLE_MEM_ADDR;
	int exeSize              = 0;
	int exeValid;
	u32 exeEntryPointAddress = 0;
	entrypoint exeEntryPoint;
	int i = 0;
	int a = 0;
	char *ptr;
	int x = 0;
	int y = 0;
	bool vid = true;
	bool LOAD = true;
	char exePath1[100];
	char exePath2[100];
	load_forwarder();
	char Path2[strlen(Path)+1];
	char Path3[strlen(Path)+1];
	strcpy(Path2, Path);
	strcpy(Path3, Path);

main:
	load_forwarder();
	strcpy(Path2, Path);
	strcpy(Path3, Path);
	exeFile = NULL;
	i = 0;
	a = 0;
	ptr = NULL;
	x = 0;
	y = 0;
	vid = true;
	if( strchr(Path, ',') == NULL )
		vid = false;

	WPAD_Init();
	LOAD = true;
	if( vid )
		printf("\x1b[0;0H");
	ptr = strtok(Path3, ",");
	while (ptr != NULL)
	{
		if( vid )
		{
			printf("\x1b[%d;7H",y);
			printf("fat:/%s/",ptr);
		}
		ptr = strtok (NULL, ",");
		if( vid )
		{
			printf("\x1b[%d;1H",y);
			printf("  ");
		}
		y++;
	}
	y--;
	if( y == 0)
	{
		ptr = strtok(Path2, ",");
		while (ptr != NULL)
		{
			sprintf(exePath1, "fat:/%s/boot.dol", ptr);
			sprintf(exePath2, "fat:/%s/boot.elf", ptr);
			ptr = strtok (NULL, ",");
		}
		LOAD = false;
	}
	if( vid )
	{
		printf("\x1b[%d;7H",y+2);
		printf("                                                                           ");
		printf("\x1b[%d;7H",y+3);
		printf("                                                                           ");
		printf("\x1b[%d;7H",y+4);
		printf("                                                                           ");
	}
	while(LOAD)
	{
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if( vid )
		{
			printf("\x1b[%d;1H",i);
			printf(">>");
		}
		if ( pressed & WPAD_BUTTON_DOWN )
		{
			if( vid )
			{
				printf("\x1b[%d;1H",i);
				printf("  ");
			}
			i++;
			if(i>y)
				i=0;
		}
		if ( pressed & WPAD_BUTTON_UP )
		{
			if( vid )
			{
				printf("\x1b[%d;1H",i);
				printf("  ");
			}
			i--;
			if(i<0)
				i=y;
		}
		if ( pressed & WPAD_BUTTON_A )
		{
			ptr = strtok(Path2, ",");
			while (ptr != NULL)
			{
				if(i == x)
				{
					sprintf(exePath1, "fat:/%s/boot.dol", ptr);
					sprintf(exePath2, "fat:/%s/boot.elf", ptr);
					break;
				}
				x++;
				ptr = strtok (NULL, ",");
			}
			LOAD = false;
		}
	}
/* Open dol/elf File and check exist */
	if( vid )
	{
		printf("\x1b[%d;7H",y+2);
		printf("Checking devices (max 20 Seconds)....");
	}
	for(a = 0; a < 20; a++)
	{
		if( MountDevicesSD() )
		{
			if (fopen (exePath1 ,"rb") != NULL)
				exeFile = fopen (exePath1 ,"rb");
			else if(fopen (exePath2 ,"rb") != NULL)
				exeFile = fopen (exePath2 ,"rb");
			else
				ShutdownDevices();
		}
		
		if(  exeFile == NULL && MountDevicesUSB() )
		{
			if (fopen (exePath1 ,"rb") != NULL)
				exeFile = fopen (exePath1 ,"rb");
			else if(fopen (exePath2 ,"rb") != NULL)
				exeFile = fopen (exePath2 ,"rb");
			else
				ShutdownDevices();
		}

		if( exeFile == NULL && a == 19 )
		{
			if( vid )
			{
				printf("\x1b[%d;7H",y+3);
				printf("File not found....(%s)",exePath1);
			}
			goto error;
		}
		else if( exeFile != NULL )
		{
			break;
		}
		sleep(1);
	}
/* read whole file in */
	fseek (exeFile, 0, SEEK_END);
	exeSize = ftell(exeFile);
	fseek (exeFile, 0, SEEK_SET);
	if(fread (exeBuffer, 1, exeSize, exeFile) != exeSize) {
		if( vid )
		{
			printf("\x1b[%d;7H",y+3);
			printf("Can't open file...");
		}
		goto error;
	}
	fclose (exeFile);

/* load entry point */
	struct __argv args[10];
	exeValid = valid_elf_image(exeBuffer);
	if( exeValid < 0 ) {
		if( vid )
		{
			printf("\x1b[%d;7H",y+3);
			printf("File isn't a valide DOL/ELF....");
		}
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
		if( vid )
		{
			printf("\x1b[%d;7H",y+3);
			printf("EntryPointAddress mismatch...\n");
		}
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

error:
	if( vid )
	{
		printf("\x1b[%d;7H",y+4);
		printf ("Press HOME to exit or B to reload...");
	}
	while(1) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if ( pressed & WPAD_BUTTON_HOME || vid == false )
		{
			u32 *stub = (u32 *)0x80001800;
			if (*stub)
				exit(0);

			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		}
		if ( pressed & WPAD_BUTTON_B )
			goto main;
		VIDEO_WaitVSync();
	}
	return 0;
}
