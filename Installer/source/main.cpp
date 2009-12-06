/*
Preloader Installer - An installation utiltiy for preloader (c) 2008-2009 crediar

Copyright (c) 2009  phpgeek

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include <ogc/isfs.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbgecko.h>

//rev version
#include "../../Shared/svnrev.h"

// Bin Files
#include "certs_bin.h"
#include "su_tik.h"
#include "su_tmd.h"

// Priiloader Application
#include "priiloader_app.h"

static GXRModeObj *vmode = NULL;
static void *xfb = NULL;
bool GeckoFound = false;
//a must have in every app
s32 __IOS_LoadStartupIOS()
{
	return 0;
}
void gprintf( const char *str, ... )
{
	if(!GeckoFound)
		return;

	char astr[4096];

	va_list ap;
	va_start(ap,str);

	vsprintf( astr, str, ap );

	va_end(ap);
	
	usb_sendbuffer( 1, astr, strlen(astr) );
}
void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
	{
		usb_flush(EXI_CHANNEL_1);
		gprintf("gecko found\n");
	}
	return;
}
const char* abort(const char* msg, ...)
{
	va_list args;
	char text[ 40032 ];
	va_start( args, msg );
	strcpy( text + vsprintf( text,msg,args ),""); 
	va_end( args );

	printf("   %s, aborting mission...", text);
	ISFS_Deinitialize();
	__ES_Close();
	sleep(5);
	exit(0);
}
s32 nand_copy(char source[1024], char destination[1024])
{
    u8 *buffer;
    s32 source_handler, dest_handler, ret;

    source_handler = ISFS_Open(source,ISFS_OPEN_READ);
    if (source_handler < 0)
            return source_handler;
   
    ISFS_Delete(destination);
    ISFS_CreateFile(destination,0,3,3,3);
    dest_handler = ISFS_Open(destination,ISFS_OPEN_RW);
    if (dest_handler < 0)
            return dest_handler;

    fstats * status = (fstats*)memalign(32,sizeof(fstats));
    ret = ISFS_GetFileStats(source_handler,status);
    if (ret < 0)
    {
		printf("\n\n  Failed to get information about %s!\n",source);
		sleep(2);
        ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
        return ret;
    }
	if ( status->file_length == 0 )
	{
		printf("\n\n  source file of nand_copy is reported as 0kB!\n");
		return -1;
	}

    buffer = (u8 *)memalign(32,status->file_length);

    ret = ISFS_Read(source_handler,buffer,status->file_length);
    if (ret < 0)
    {
		printf("\n\nFailed to Read Data from %s!\n",source);
		sleep(2);
		ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
        free(buffer);
        return ret;
    }

    ret = ISFS_Write(dest_handler,buffer,status->file_length);
    if (ret < 0)
    {
        ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
        free(buffer);
        return ret;
    }

    ISFS_Close(source_handler);
    ISFS_Close(dest_handler);
    free(status);
    free(buffer);
    return 0;
}
//a nice function found in muppen64gc
static inline char* getAlignedName(char* name){
	static char alignedBuf[64] __attribute__((aligned(32)));
	if((int)name % 32){
		strncpy(alignedBuf, name, 64);
		return alignedBuf;
	} else return name;
}


int main(int argc, char **argv)
{
	bool CopyTicket = false;
	VIDEO_Init();
	WPAD_Init();
	vmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	console_init(xfb,20,20,vmode->fbWidth,vmode->xfbHeight,vmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	CheckForGecko();
	gprintf("resolution is %dx%d\n",vmode->viWidth,vmode->viHeight);
	printf("\x1b[2;0H");
	printf("  IOS %d rev %d\n\n",IOS_GetVersion(),IOS_GetRevision());
	printf("       priiloader rev %d (preloader v0.30 mod) Installation / Removal Tool\n\n\n\n",SVN_REV);
	printf("                          PLEASE READ THIS CAREFULLY\n\n");
	printf("                THIS PROGRAM/TOOL COMES WITHOUT ANY WARRANTIES!\n");
	printf("               YOU ACCEPT THAT YOU INSTALL THIS AT YOUR OWN RISK\n\n\n");
	printf("      THE AUTHOR(S) CANNOT BE HELD LIABLE FOR ANY DAMAGE IT MIGHT CAUSE\n\n");
	printf("            IF YOU DO NOT AGREE WITH THESE TERMS TURN YOUR WII OFF\n\n\n\n");
	printf("					Press (+) to install or update Priiloader\n");
	printf("			  Press (-) to remove Priiloader and restore system menu\n");
	printf("			 Hold Down (B) with any above options to use IOS249 (cios)\n");
	printf("				Press (HOME) to chicken out and quit the installer!\n\n");
	printf("							Enjoy! DacoTaco & /phpgeek\n							");

	while(1)
	{
		WPAD_ScanPads();
		u16 pDown = WPAD_ButtonsDown(0);
		u16 pHeld = WPAD_ButtonsHeld(0);
		if (pDown & WPAD_BUTTON_PLUS || pDown & WPAD_BUTTON_MINUS)
		{
			static u32 tmp_ikey ATTRIBUTE_ALIGN(32);
			static u32 tmd_size ATTRIBUTE_ALIGN(32);
			static u64 title_id ATTRIBUTE_ALIGN(32)=0x0000000100000002LL;

			s32 fd,fs;
			u32 id = 0;

			if (pHeld & WPAD_BUTTON_B)
			{
				WPAD_Shutdown();
        		IOS_ReloadIOS(249);
        		WPAD_Init();
        	}
			printf("\x1b[2J\x1b[2;0H");
			fflush(stdout);
			printf("  IOS %d rev %d\n\n\n\n\n",IOS_GetVersion(),IOS_GetRevision());
        	__ES_Init();
			fd = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)su_tmd, su_tmd_size, (signed_blob *)su_tik, su_tik_size, &tmp_ikey);
			if(fd > 0)
			{
				printf("  ES_Identify failed, error %u. ios%d not patched with ES_DIVerify?\n",fd,IOS_GetVersion());
				printf("  will continue but chances are it WILL fail\n");
				printf("  using cios (holding b and then pressing + or - ) will probably solve this. NOTE: you need CIOS for this.");
			}
			else
				printf("  Logged in as \"su\"!\n");
			if (ISFS_Initialize() < 0)
				abort("Failed to get root");
			printf("  Got ROOT!\n");
			fd = ISFS_Open(getAlignedName("/title/00000001/00000002/content/ticket"),ISFS_OPEN_READ);
			if (fd <0)
			{
				printf("  Priiloader system menu ticket not found/access denied.\n  trying to read original ticket...\n");
				ISFS_Close(fd);
				fd = ISFS_Open(getAlignedName("/ticket/00000001/00000002.tik"),ISFS_OPEN_READ);
				//"/ticket/00000001/00000002.tik" -> original path which should be there on every wii.
				//however needs nand permissions which SU doesn't have without trucha? >_>
				//we need mini if we want to be patch free...
				if (fd < 0)
				{
					switch(fd)
					{
						case ISFS_EINVAL:
							abort("Unable to read ticket.path is wrong/to long or ISFS isn't init yet?");
							break;
						case ISFS_ENOMEM:
							abort("Unable to read ticket.(Out of memory)");
							break;
						case -106:
							abort("Ticket not found");
							break;
						case -102:
							abort("Unautorised to get ticket. is ios%d trucha signed?",IOS_GetVersion());
							break;
						default:
							abort("Unable to read ticket. error %d. ",fd);
							break;
					}

				}
				else
				{
					printf("  Original ticket loaded & set for copy\n");
					CopyTicket = true;
				}
			}
			fstats * status = (fstats*)memalign(32,sizeof(fstats));
			fs = ISFS_GetFileStats(fd,status);
			if (fs < 0)
			{
				ISFS_Close(fd);
				abort("Unable to get ticket size");
			}

			char * buffer = (char*)memalign(32,(status->file_length+32)&(~31));
			if (buffer == NULL)
			{
				ISFS_Close(fd);
				abort("Unable to create buffer");
			}

			memset(buffer,0,(status->file_length+32)&(~31));
			fs = ISFS_Read(fd,buffer,status->file_length);
			if (fs < 0)
			{
				free(buffer);
				ISFS_Close(fd);
				abort("Unable to read buffer");
			}

			ISFS_Close(fd);

			fs = ES_GetStoredTMDSize(title_id,&tmd_size);
			if (fs < 0)
			{
				free(buffer);
				abort("Unable to get stored tmd size");
			}

			signed_blob *TMD = (signed_blob *)memalign(32,(tmd_size+32)&(~31));
			if (TMD == NULL)
			{
				free(buffer);
				abort("Unable to prepare tmd for memory");
			}

			memset(TMD,0,tmd_size);
			fs = ES_GetStoredTMD(title_id,TMD,tmd_size);
			if (fs < 0)
			{
				free(TMD);
				free(buffer);
				abort("Unable to get stored tmd");
			}
	
			tmd *rTMD = (tmd *)(TMD+(0x140/sizeof(tmd *)));
			for(u32 i=0; i < rTMD->num_contents; ++i)
			{
				if (rTMD->contents[i].index == rTMD->boot_index)
				{
					id = rTMD->contents[i].cid;
					break;
				}
			}

			if (id == 0)
			{
				free(TMD);
				free(buffer);
				abort("Unable to retrieve title id");
			}

			char * original_app = (char*)memalign(32,256);
			char * copy_app = (char*)memalign(32,256);
			if (original_app == NULL || copy_app == NULL)
			{
				free(TMD);
				free(buffer);
				abort("Unable to prepare title for memory");
			}

			memset(original_app,0,256);
			memset(copy_app,0,256);
			sprintf(original_app, "/title/00000001/00000002/content/%08x.app",id);
			sprintf(copy_app, "/title/00000001/00000002/content/%08x.app",id);
			copy_app[33] = '1';

			if (pDown & WPAD_BUTTON_PLUS)
			{
				bool Priiloader_found = false;
				printf("  Checking for Priiloader...\n");
				fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					Priiloader_found = false;
				}
				else
				{
					fstats * status = (fstats*)memalign(32,sizeof(fstats));
					if (ISFS_GetFileStats(fd,status) < 0)
					{
						printf("\n\n  WARNING: failed to get stats of %s. ignoring priiloader \"installation\"...\n\n",copy_app);
						Priiloader_found = false;
					}
					else
					{
						if ( status->file_length == 0 )
						{
							printf("\n\n  WARNING: %s is reported as 0kB. ignoring priiloader \"installation\"...\n\n",copy_app);
							Priiloader_found = false;
						}
						else
							Priiloader_found = true;
					}
					free(status);
					ISFS_Close(fd);
				}
				if(!Priiloader_found)
				{
					printf("  Priiloader not found.\n  Installing Priiloader...\n\n\n");
				}
				else
				{
					printf("  Priiloader installation found\n  Updating Priiloader...\n\n\n");
				}
				if(CopyTicket)
				{
					printf("  Coping system menu ticket...");
					if (nand_copy("/ticket/00000001/00000002.tik","/title/00000001/00000002/content/ticket") < 0)
					{
						abort("Unable to copy the system menu ticket");
					}
					printf("Done!\n");
				}
				else
				{
					printf("  Skipping copy of system menu ticket...\n");
				}
				if(!Priiloader_found)
				{
					printf("  Moving System Menu app...");
					if (nand_copy(original_app,copy_app) < 0)
					{
						abort("\n  Unable to move the system menu");
					}
					else
						printf("Done!\n");
				}
				else
				{
					printf("  Skipping Moving of System menu app...\n");
				}				
				s32 ret = 0;
				ret = ISFS_Delete("/title/00000001/00000002/data/loader.ini");
				gprintf("loader.ini deletion returned %d\n",ret);
				
				printf("  Writing Priiloader app...");
				ISFS_Delete(original_app);
				ISFS_CreateFile(original_app,0,3,3,3);
				fd = ISFS_Open(original_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					nand_copy(copy_app,original_app);
					abort("\n  Failed to open file for Priiloader writing.");
				}
				ret = ISFS_Write(fd,priiloader_app,priiloader_app_size);
				if (ret < 0 ) //check if the app was writen correctly				
				{
					ISFS_Close(fd);
					nand_copy(copy_app,original_app);
					gprintf("Write failed. ret %d\n",ret);
					abort("\n  Write of Priiloader app failed.");
				}
				//ISFS_Close(fd);
				printf("Done\n");
				printf("  Checking Priiloader Installation...");
				fstats * status = (fstats*)memalign(32,sizeof(fstats));
				if (ISFS_GetFileStats(fd,status) < 0)
				{
					ISFS_Close(fd);
					nand_copy(copy_app,original_app);
					abort("\n  Failed to get stats of %s. System Menu Recovered.",original_app);
				}
				else
				{
					if ( status->file_length != priiloader_app_size )
					{
						ISFS_Close(fd);
						nand_copy(copy_app,original_app);
						abort("\n  Written Priiloader app isn't the correct size.");
					}
				}
				free(status);
				printf("Done\n\n");
				if(Priiloader_found)
					printf("  Update done, exiting to loader... waiting 5s...\n");
				else
					printf("  Install done, exiting to loader... waiting 5s...\n");
				ISFS_Deinitialize();
				__ES_Close();
				sleep(5);
				exit(0);
			}

			else if (pDown & WPAD_BUTTON_MINUS)
			{
				printf("  Checking for Priiloader...\n");
				fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					abort("Priiloader not found");
				}
				else
				{
					printf("  Priiloader installation found.removing...\n\n  Removing Priiloader...");
					ISFS_Delete(original_app);
					printf("Done!\n  Restoring System menu app...");
					s32 ret = nand_copy(copy_app,original_app);
					if (ret < 0)
					{
						ISFS_Close(fd);
						ISFS_CreateFile(original_app,0,3,3,3);
						fd = ISFS_Open(original_app,ISFS_OPEN_RW);
						ISFS_Write(fd,priiloader_app,priiloader_app_size);
						ISFS_Close(fd);
						abort("\n  Unable to restore the system menu! (ret = %d)",ret);
					}
					else
					{
						ISFS_Close(fd);
						ISFS_Delete(copy_app);
						printf("Done!\n");
						printf("  Deleting extra Priiloader files...");
						s32 ret = ISFS_Delete("/title/00000001/00000002/data/loader.ini");
						gprintf("loader.ini : %d\n",ret);
						//its best we delete that ticket but its completely useless and will only get in our 
						//way when installing again later...
						//ret = ISFS_Delete("/title/00000001/00000002/content/ticket");
						//gprintf("ticket : %d\n",ret);
						ret = ISFS_Delete("/title/00000001/00000002/data/hacks_s.ini");
						gprintf("hacks_s.ini : %d\n",ret);
						ret = ISFS_Delete("/title/00000001/00000002/data/hacks.ini");
						gprintf("hacks.ini : %d\n",ret);
						ret = ISFS_Delete("/title/00000001/00000002/data/main.bin");
						gprintf("main.bin : %d\n",ret);
						printf("Done!\n\n");
						printf("  Removal done, exiting to loader... waiting 5s...\n");
						ISFS_Deinitialize();
						__ES_Close();
						sleep(5);
						exit(0);
					}
				}
			}
		}
		else if (pDown & WPAD_BUTTON_HOME)
		{
			printf("\x1b[2J\x1b[2;0H");
			fflush(stdout);
			printf("Sorry, but our princess is in another castle... goodbye!");
			sleep(5);
			exit(0);
		}
	}
	return 0;
}
