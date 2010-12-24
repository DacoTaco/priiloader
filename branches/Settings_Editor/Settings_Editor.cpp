// Settings_Editor.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include "Settings_Editor.h"
#define VERSION 0
#define SUBVERSION 3
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
	int file_version = 0;
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
			printf("Settings file from 0.3c ( rev 54 - rev 57 ) detected\n");
			file_version = 3;
			break;
		case sizeof(Settings_4):
			printf("Settings file from 0.4 ( rev 58 - 88 ) detected\n");
			file_version = 4;
			break;
		case sizeof(Settings_5):
			printf("Settings file from 0.5 ( rev 89 or above ) detected\n");
			file_version = 5;
			break;
		case sizeof(Settings_6):
			printf("Settings file from 0.6 detected\n");
			file_version = 6;
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
	Settings_6 *Settings = (Settings_6*) malloc(sizeof(Settings_6));
	memset(Settings,0,sizeof(Settings_6));
	fread(Settings,1,SettingsSize,Settings_fd);
	fclose(Settings_fd);

	//check for endian swapping
	if(Data_Need_Swapping())
	{
		endian_swap(Settings->autoboot);
		endian_swap(Settings->ReturnTo);
		endian_swap(Settings->BetaVersion);
		endian_swap(Settings->ShowBetaUpdates);
	}
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
			printf("\t3 : Shutdown To       :\t\t%s\n",Settings->ShutdownToPreloader?"Priiloader":"off       ");
			printf("\t4 : Stop disc         :\t\t%s\n", Settings->StopDisc?"on ":"off");
			printf("\t5 : Light on error    :\t\t%s\n", Settings->LidSlotOnError?"on ":"off");
			printf("\t6 : Ignore Standby    :\t\t%s\n", Settings->IgnoreShutDownMode?"on ":"off");
			printf("\t7 : Background Color  :\t\t%s\n", Settings->BlackBackground?"Black":"White");
			printf("\t8 : Show Debug Info   :\t\t%s\n", Settings->ShowGeckoText?"on ":"off");
			//added options from versions > 3
			if (file_version >= 4)
			{
				printf("\tQ : Protect Priiloader:\t\t%s\n",Settings->PasscheckPriiloader?"on ":"off");
				printf("\tW : Protect Autoboot  :\t\t%s\n",Settings->PasscheckMenu?"on ":"off");
			}
			if (file_version >= 5)
			{
				printf("\tE : Show Beta Updates :\t\t%s\n",Settings->ShowBetaUpdates?"on ":"off       ");
			}
			if(file_version == 6)
			{
				printf("\tR : Classic Hacks :\t\t%s\n",Settings->UseClassicHacks?"on ":"off       ");
			}
			printf("\t9 : Use SysMenu IOS   :\t\t%s\n", Settings->UseSystemMenuIOS?"on ":"off");
			if(!Settings->UseSystemMenuIOS)
				printf("\t0 : IOS For SysMenu   :\t\t%d\n", Settings->SystemMenuIOS);
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
							if (temp == 27 || temp == 110 || temp == 78) // 27 = esc , 78 = N & 110 = n
								exit(0);
							else if(temp == 121 || temp == 89 ) // 121 = y , 89 = Y
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
									printf("Failed to open file!\n");
									sleep(2);
									break;
								}
								if(Data_Need_Swapping())
								{
									endian_swap(Settings->autoboot);
									endian_swap(Settings->ReturnTo);
									endian_swap(Settings->BetaVersion);
									endian_swap(Settings->ShowBetaUpdates);
								}
								switch(file_version)
								{
									case 3:
										fwrite(Settings,1,sizeof(Settings_3),Settings_fd);
										break;
									case 4:
										fwrite(Settings,1,sizeof(Settings_4),Settings_fd);
										break;	
									case 5:
										fwrite(Settings,1,sizeof(Settings_5),Settings_fd);
										break;
									case 6:
										fwrite(Settings,1,sizeof(Settings_6),Settings_fd);
										break;
									default:
										printf("unknown version detected. refusing to save");
										sleep(2);
										exit(0);
								}
								fclose(Settings_fd);
								printf("Saved\n");
								sleep(2);
								exit(0);
							}
							else
							{
								break;
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
					if( Settings->IgnoreShutDownMode )
						Settings->IgnoreShutDownMode = 0;
					else 
						Settings->IgnoreShutDownMode = 1;
					redraw=1;
					break;
				case 55:// 7 , Background Colour
					if( Settings->BlackBackground )
						Settings->BlackBackground = 0;
					else 
						Settings->BlackBackground = 1;
					redraw=1;
					break;
				case 56:// 8 , Show Gecko Output
					if( Settings->ShowGeckoText )
						Settings->ShowGeckoText = 0;
					else 
						Settings->ShowGeckoText = 1;
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
				case 81:
				case 113:// Q/q , protect priiloader
					if (file_version < 4)
						break;
					if( Settings->PasscheckPriiloader )
					{
						Settings->PasscheckPriiloader = false;
					}
					else
					{
						Settings->PasscheckPriiloader = true;
					}
					redraw = 1;
					break;
				case 87:
				case 119:// W/w , protect autoboot
					if (file_version < 4)
						break;
					if( Settings->PasscheckMenu )
					{
						Settings->PasscheckMenu = false;
					}
					else
					{
						Settings->PasscheckMenu = true;
					}
					redraw = 1;
					break;
				case 69:
				case 101: // E/e , ShowBetaUpdates
					if (file_version < 5)
						break;
					if( Settings->ShowBetaUpdates )
					{
						Settings->ShowBetaUpdates = false;
					}
					else
					{
						Settings->ShowBetaUpdates = true;
					}
					redraw = 1;
					break;
				case 114:
				case 82: //r or R : classic hacks
					if(file_version != 6)
						break;
					if( Settings->UseClassicHacks )
					{
						Settings->UseClassicHacks = false;
					}
					else
					{
						Settings->UseClassicHacks = true;
					}
					redraw = 1;
					break;
				default:
#ifdef _DEBUG
					printf("key %c (int value %i) is pressed\n",test,test);
#endif
					break;
			}
		}
	}
	return 0;
}
bool Data_Need_Swapping( void )
{
    int test_var = 1;
    char *cptr = (char*)&test_var;

	//true means little ending
	//false means big endian
    return (cptr != NULL);
}
inline void endian_swap(unsigned int& x)
{
    x = (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}
