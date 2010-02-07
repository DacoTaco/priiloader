// Settings_Editor.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "Settings_Editor.h"
#define VERSION 0
#define SUBVERSION 1
#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define cls() system("cls")
#else
#include <unistd.h>
#define cls() system("clear")
#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <stropts.h>

int _kbhit() {
    static const int STDIN = 0;
    static bool initialized = false;

    if (! initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}
#endif

int _tmain(int argc, _TCHAR* argv[])
{
	printf("-------------------------------------------------------------------------------\n");
	printf("\t\tDacoTaco's Priiloader Settings Editor v%d.%d\n",VERSION,SUBVERSION);
	printf("-------------------------------------------------------------------------------\n\n\n");
	printf("opening file...\n");
	FILE* Settings_fd = fopen("loader.ini","r+");
	if (!Settings_fd )
	{
		printf("failed to open Settings file!\n");
		sleep(2);
		exit(0);
	}

	//detect the struct version
	sl32 SettingsSize;
	fseek (Settings_fd  , 0 , SEEK_END);
	SettingsSize = ftell (Settings_fd );
	rewind (Settings_fd );
	switch (SettingsSize)
	{
		case sizeof(Settings_3):
			printf("Settings file from 0.3c ( rev 54 or above ) detected\n");
			break;
		default:
			printf("unknown Settings File! exiting...\n");
			sleep(2);
			exit(0);
	}
	Settings_Version *version = (Settings_Version*) malloc (sizeof(Settings_Version));
	if(!version)
	{
		printf("failed to allocate version\n");
		sleep(2);
		exit(0);
	}
	memset( version,0,sizeof(Settings_Version));
	printf("reading version...\n");
	fread(version,1,sizeof(Settings_Version),Settings_fd );
	u32 fileversion = version->version;
	free(version);
	printf("version %x.%x\n",fileversion&0xFF,fileversion>>24);
	rewind (Settings_fd );

	printf("reading Settings...\n");
	sleep(2);
	Settings_3 *Settings = (Settings_3*) malloc(sizeof(Settings_3));
	memset(Settings,0,sizeof(Settings_3));
	fread(Settings,1,SettingsSize,Settings_fd);
	fclose(Settings_fd);
	//start of main loop
	bool redraw = 1;
	while(1)
	{
		if(redraw)
		{
			cls();
			printf("-------------------------------------------------------------------------------\n");
			printf("\t\tDacoTaco's Priiloader Settings Editor v%d.%d\n",VERSION,SUBVERSION);
			printf("-------------------------------------------------------------------------------\n\n\n");
			printf("Current Settings : \n\n");
			printf("\t1 : Autoboot To      :\t\t");
			switch(Settings->autoboot)
			{
				case AUTOBOOT_DISABLED:
					printf("Disabled\n");
					break;
				case AUTOBOOT_HBC:
					printf("Homebrew Channel\n");
					break;
				case AUTOBOOT_BOOTMII_IOS:
					printf("Bootmii IOS\n");
					break;
				case AUTOBOOT_SYS:
					printf("System Menu\n");
					break;
				case AUTOBOOT_FILE:
					printf("Installed File\n");
					break;
				case AUTOBOOT_ERROR:
					printf("error?\n");
					break;
				default:
					printf("unknown value %u\n",Settings->autoboot);
			}
			printf("\t2 : Return To        :\t\t");
			switch(Settings->ReturnTo)
			{
				case RETURNTO_SYSMENU:
					printf("System Menu\n");
					break;
				case RETURNTO_PRELOADER:
					printf("Priiloader\n");
					break;
				case RETURNTO_AUTOBOOT:
					printf("Autboot Setting\n");
					break;
				default:
					printf("unknown value %u\n",Settings->ReturnTo);
			}
			printf("\t3 : Shutdown To      :\t\t%s\n",Settings->ShutdownToPreloader?"Priiloader":"off       ");
			printf("\t4 : Stop disc        :\t\t%s\n", Settings->StopDisc?"on ":"off");
			printf("\t5 : Light on error   :\t\t%s\n", Settings->LidSlotOnError?"on ":"off");
			printf("\t6 : Ignore Standby   :\t\t%s\n", Settings->IgnoreShutDownMode?"on ":"off");
			printf("\t7 : Background Color :\t\t%s\n", Settings->BlackBackground?"Black":"White");
			printf("\t8 : Show Debug Info  :\t\t%s\n", Settings->ShowDebugText?"on ":"off");
			printf("\t9 : Use SysMenu IOS  :\t\t%s\n", Settings->UseSystemMenuIOS?"on ":"off");
			if(!Settings->UseSystemMenuIOS)
				printf("\t0 : IOS For SysMenu  :\t\t%d\n", Settings->SystemMenuIOS);
			else
				printf("\n");

			printf("\n\n\n\tesc : Quit(and Save)\n");
			redraw = 0;
		}
		//keyboard input
		if(_kbhit())
		{
			//48 - 57
			char test = _getch();
			switch(test)
			{
				case 27: //esc , save and exit
					printf("Save before exitting?\n");
					while(1)
					{
						if(_kbhit())
						{
							char temp = _getch();
							if (temp == 27 || temp == 110)
								exit(0);
							else
							{
								printf("Creating backup...\n");
#ifdef WIN32
								system("copy loader.ini loader_backup.ini");
#else
								system("cp loader.ini loader_backup.ini");
#endif
								printf("Saving...\n");
								Settings_fd = fopen("loader.ini","w");
								if(!Settings_fd)
								{
									printf("Failed to create backup!\n");
									sleep(2);
									break;
								}
								fwrite(Settings,1,sizeof(Settings_3),Settings_fd);
								fclose(Settings_fd);
								printf("Saved\n");
								sleep(2);
								exit(0);
							}
						}
					}
					redraw = 1;
					break;
				case 49: //1 , autboot
					if( Settings->autoboot == AUTOBOOT_FILE )
						Settings->autoboot = AUTOBOOT_DISABLED;
					else
						Settings->autoboot++;
					redraw=1;
					break;
				case 50: // 2 , return to
					Settings->ReturnTo++;
					if( Settings->ReturnTo > RETURNTO_AUTOBOOT )
						Settings->ReturnTo = RETURNTO_SYSMENU;
					redraw=1;
					break;
				case 51: //3 , shutdown to
					if( Settings->ShutdownToPreloader )
						Settings->ShutdownToPreloader = false;
					else 
						Settings->ShutdownToPreloader = true;
					redraw=1;
					break;
				case 52: // 4 , stop disc
					if( Settings->StopDisc )
						Settings->StopDisc = 0;
					else 
						Settings->StopDisc = 1;
					redraw=1;
					break;
				case 53:// 5 , light on error
					if( Settings->LidSlotOnError )
						Settings->LidSlotOnError = 0;
					else 
						Settings->LidSlotOnError = 1;
					redraw=1;
					break;
				case 54:// 6 , Ignore Standby
					if( Settings->LidSlotOnError )
						Settings->LidSlotOnError = 0;
					else 
						Settings->LidSlotOnError = 1;
					redraw=1;
					break;
				case 55:// 7 , Background Colour
					if( Settings->LidSlotOnError )
						Settings->LidSlotOnError = 0;
					else 
						Settings->LidSlotOnError = 1;
					redraw=1;
					break;
				case 56:// 8 , Show Debug Info
					if( Settings->LidSlotOnError )
						Settings->LidSlotOnError = 0;
					else 
						Settings->LidSlotOnError = 1;
					redraw=1;
					break;
				case 57: // 9 , Use System menu IOS
					if(Settings->UseSystemMenuIOS)
						Settings->UseSystemMenuIOS = false;
					else
						Settings->UseSystemMenuIOS = true;
					redraw = 1;
					break;
				case 48: // 0 , choosing system menu ios to load
					if(!Settings->UseSystemMenuIOS)
					{
						if(Settings->SystemMenuIOS == 255)
							Settings->SystemMenuIOS = 9;
						else
							Settings->SystemMenuIOS++;
						redraw = 1;
					}
					break;
				default:
					printf("key %c (int value %i) is pressed\n",test,test);
					break;
			}
		}
	}
	return 0;
}
