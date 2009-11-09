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

//rev version
#include "../../Shared/svnrev.h"

// Bin Files
#include "certs_bin.h"
#include "su_tik.h"
#include "su_tmd.h"

// Preloader Application
#include "priiloader_app.h"

static GXRModeObj *vmode = NULL;
static void *xfb = NULL;

//a must have in every app
s32 __IOS_LoadStartupIOS()
{
	return 0;
}
const char* abort(const char* msg, ...)
{
	va_list args;
	char text[ 40032 ];
	va_start( args, msg );
	strcpy( text + vsprintf( text,msg,args ),""); 
	va_end( args );

	printf("  %s, aborting mission...", text);
	ISFS_Deinitialize();
	__ES_Close();
	sleep(5);
	exit(0);
}
s32 nand_copy(char file1[1024], char file2[1024])
{
	u8 *buffer;
        int f1;
        s32 f2,ret;

        f1 = ISFS_Open(file1,ISFS_OPEN_READ);
        if (f1 < 0)
                return f1;
       
        ISFS_Delete(file2);
        ISFS_CreateFile(file2,0,3,3,3);
        f2 = ISFS_Open(file2,ISFS_OPEN_RW);
        if (!f2)
		return -1;

        fstats * status = (fstats*)memalign(32,sizeof(fstats));
        ret = ISFS_GetFileStats(f1,status);
        if (ret < 0)
        {
                ISFS_Close(f1);
                ISFS_Close(f2);
                ISFS_Delete(file2);
                free(status);
                return ret;
        }

        buffer = (u8 *)memalign(32,status->file_length);

        ret = ISFS_Read(f1,buffer,status->file_length);
        if (ret < 0)
        {
                ISFS_Close(f1);
                ISFS_Close(f2);
                ISFS_Delete(file2);
                free(status);
                free(buffer);
                return ret;
        }

	ret = ISFS_Write(f2,buffer,status->file_length);
        if (!ret)
        {
                ISFS_Close(f1);
                ISFS_Close(f2);
                ISFS_Delete(file2);
                free(status);
                free(buffer);
                return ret;
        }

        ISFS_Close(f1);
        ISFS_Close(f2);
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

	printf("\x1b[2;0H");
	printf("IOS %d rev %d\n\n",IOS_GetVersion(),IOS_GetRevision());
	printf("       priiloader rev %d (preloader v0.30b) Installation / Removal Tool\n\n\n\n",SVN_REV);
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

			printf("\x1b[2J\x1b[2;0H");
			fflush(stdout);
			if (pHeld & WPAD_BUTTON_B)
			{
				WPAD_Shutdown();
        		IOS_ReloadIOS(249);
        		WPAD_Init();
        	}
			printf("IOS %d rev %d\n\n\n\n\n",IOS_GetVersion(),IOS_GetRevision());
        	__ES_Init();
			fd = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)su_tmd, su_tmd_size, (signed_blob *)su_tik, su_tik_size, &tmp_ikey);
			if(fd > 0)
			{
				printf("ES_Identify failed, error %u. ios%d not patched with ES_DIVerify?\n",fd,IOS_GetVersion());
				printf("will continue but chances are it WILL fail\n");
				printf("using cios (holding b and then pressing + or - ) will probably solve this. NOTE: you need CIOS for this.");
			}
			else
				printf("  Logged in as \"su\"!\n");
			if (ISFS_Initialize() < 0)
				abort("Failed to get root");
			printf("  Got ROOT!\n");
			fd = ISFS_Open(getAlignedName("/title/00000001/00000002/content/ticket"),ISFS_OPEN_READ);
			if (fd <0)
			{
				printf("  priiloader system menu ticket not found/access denied.\n  trying to read original ticket...\n");
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
							abort("ticket not found");
							break;
						case -102:
							abort("unautorised to get ticket. is ios%d trucha signed?",IOS_GetVersion());
							break;
						default:
							printf("Unable to read ticket. error %d. ",fd);
							abort("");
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

			char * file = (char*)memalign(32,256);
			char * load = (char*)memalign(32,256);
			if (file == NULL || load == NULL)
			{
				free(TMD);
				free(buffer);
				abort("Unable to prepare title for memory");
			}

			memset(file,0,256);
			memset(load,0,256);
			sprintf(file, "/title/00000001/00000002/content/%08x.app",id);
			sprintf(load, "/title/00000001/00000002/content/%08x.app",id);
			load[33] = '1';

			if (pDown & WPAD_BUTTON_PLUS)
			{
				printf("  Checking for preloader...\n");
				fd = ISFS_Open(load,ISFS_OPEN_RW);
				if (fd < 0)
				{
					printf("  Preloader not found, moving the system menu...\n");
					if(CopyTicket)
					{
						printf("  Copying Ticket...\n");
						if (nand_copy("/ticket/00000001/00000002.tik","/title/00000001/00000002/content/ticket") < 0)
						{
							abort("Unable to copy the system menu ticket");
						}
					}
					else
					{
						printf("  priiloader system menu ticket found\n");
					}
					if (nand_copy(file,load) < 0)
					{
						abort("Unable to move the system menu");
					}
					else
					{
						printf("  Done!\n");
						ISFS_Delete(file);
						ISFS_Delete("/title/00000001/00000002/data/loader.ini");
						printf("  Installing preloader...\n");
						ISFS_CreateFile(file,0,3,3,3);
						fd = ISFS_Open(file,ISFS_OPEN_RW);
						ISFS_Write(fd,priiloader_app,priiloader_app_size);
						ISFS_Close(fd);
						printf("  Install done, exiting to loader... waiting 5s...\n");
						ISFS_Deinitialize();
						__ES_Close();
						sleep(5);
						exit(0);
					}
				}
				else
				{
					ISFS_Close(fd);
					printf("  Preloader installation found, skipping moving the system menu.\n");
					printf("  Skipped!\n");
					if(CopyTicket)
					{
						if (nand_copy("/ticket/00000001/00000002.tik","/title/00000001/00000002/content/ticket") < 0)
						{
							abort("Unable to copy the system menu ticket");
						}
					}
					else
					{
						printf("  skipping copy of priiloader system menu ticket\n");
					}
					ISFS_Delete(file);
					ISFS_Delete("/title/00000001/00000002/data/loader.ini");
					printf("  Updating preloader...\n");
					ISFS_CreateFile(file,0,3,3,3);
					fd = ISFS_Open(file,ISFS_OPEN_RW);
					ISFS_Write(fd,priiloader_app,priiloader_app_size);
					ISFS_Close(fd);
					printf("  Update done, exiting to loader... waiting 5s...\n");
					ISFS_Deinitialize();
					__ES_Close();
					sleep(5);
					exit(0);
				}
			}

			else if (pDown & WPAD_BUTTON_MINUS)
			{
				printf("  Checking for preloader...\n");
				fd = ISFS_Open(load,ISFS_OPEN_RW);
				if (fd < 0)
				{
					abort("Preloader not found");
				}
				else
				{
					printf("  Preloader installation found, restoring the system menu.\n");
					ISFS_Delete(file);
					if (nand_copy(load,file) < 0)
					{
						ISFS_Close(fd);
						ISFS_CreateFile(file,0,3,3,3);
						fd = ISFS_Open(file,ISFS_OPEN_RW);
						ISFS_Write(fd,priiloader_app,priiloader_app_size);
						ISFS_Close(fd);
						abort("Unable to restore the system menu");
					}
					else
					{
						printf("  Done!\n");
						ISFS_Close(fd);
						ISFS_Delete(load);
						ISFS_Delete("/title/00000001/00000002/data/loader.ini");
						//its best we delete that ticket altho its completely useless lol
						ISFS_Delete("/title/00000001/00000002/content/ticket");
						ISFS_Delete("/title/00000001/00000002/data/hacks_s.ini");
						ISFS_Delete("/title/00000001/00000002/data/hacks.ini");
						ISFS_Delete("/title/00000001/00000002/data/main.bin");
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
