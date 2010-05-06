/*
Preloader/Priiloader Installer - An installation utiltiy for preloader (c) 2008-2009 crediar

Copyright (c) 2009  phpgeek
Edited by DacoTaco

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
	usb_flush(EXI_CHANNEL_1);
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
	printf("\x1b[%u;%dm", 36, 1);
	printf("%s, aborting mission...", text);
	gprintf("%s, aborting mission...", text);
	ISFS_Deinitialize();
	__ES_Close();
	sleep(5);
	exit(0);
}
bool CompareChecksum(u8 *Data1,u32 Data1_Size,u8 *Data2,u32 Data2_Size)
{
	u32 chksumD1 = 0;
	u32 chksumD2 = 0;
	if(Data1 == NULL || Data2 == NULL )
	{
		gprintf("Data1 or Data2 == NULL\n");
		return false;
	}
	if(Data1_Size <= 0 || Data2_Size <= 0)
	{
		gprintf("Data1 or Data2 size == NULL\n");
		return false;
	}

	for( u32 i=1; i<Data1_Size>>2; ++i )
		chksumD1+= Data1[i];

	for( u32 i=1; i<Data2_Size>>2; ++i )
		chksumD2+= Data2[i];

	if( chksumD1 == chksumD2 )
	{
		gprintf("Checksums are correct\n");
		return true;
	}
	else
	{
#ifdef DEBUG
		fatMountSimple("fat",&__io_wiisd);
		FILE* temp = fopen("fat:/D1.app","w");
		fwrite(Data1,1,Data1_Size,temp);
		fclose(temp);
		temp = fopen("fat:/D2.app","w");
		fwrite(Data2,1,Data2_Size,temp);
		fclose(temp);
#endif
		gprintf("Checksum D1 = %u , Checksum D2 = %u\n", chksumD1, chksumD2);
	}
	return false;
}
s32 nand_copy(char source[1024], char destination[1024])
{
    u8 *buffer = NULL;
    s32 source_handler, dest_handler, ret;

    source_handler = ISFS_Open(source,ISFS_OPEN_READ);
    if (source_handler < 0)
            return source_handler;
   
    ISFS_Delete(destination);
    ret = ISFS_CreateFile(destination,0,ISFS_OPEN_RW,ISFS_OPEN_RW,ISFS_OPEN_RW);
	if (ret != ISFS_OK) 
	{
		printf("Failed to create file %s. ret = %d\n",destination,ret);
		gprintf("Failed to create file %s. ret = %d\n",destination,ret);
		return ret;
	}
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
		status = NULL;
        return ret;
    }
	if ( status->file_length == 0 )
	{
		printf("\n\n  source file of nand_copy is reported as 0kB!\n");
		return -1;
	}

    buffer = (u8 *)memalign(32,(status->file_length+31)&(~31));
	if (buffer == NULL)
	{
		gprintf("buffer failed to allign\n");
		return 0;
	}

    ret = ISFS_Read(source_handler,buffer,status->file_length);
    if (ret < 0)
    {
		printf("\n\nFailed to Read Data from %s!\n",source);
		sleep(2);
		ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
		status = NULL;
        free(buffer);
		buffer = NULL;
        return ret;
    }

    ret = ISFS_Write(dest_handler,buffer,status->file_length);
    if (ret < 0)
    {
        ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
		status = NULL;
        free(buffer);
		buffer = NULL;
        return ret;
    }
	gprintf("starting checksum...\n");
	s32 temp = 0;
	u8 *Data2 = NULL;
	fstats * D2stat = (fstats*)memalign(32,sizeof(fstats));
	if (D2stat == NULL)
	{
		temp = -1;
		goto free_and_Return;
	}
	ISFS_Close(dest_handler);
	dest_handler = ISFS_Open(destination,ISFS_OPEN_READ);
	if(!dest_handler)
	{
		temp = -1;
		goto free_and_Return;
	}
	temp = ISFS_GetFileStats(dest_handler,D2stat);
	if(temp < 0)
	{
		goto free_and_Return;
	}
	Data2 = (u8*)memalign(32,(D2stat->file_length+31)&(~31));
	if (Data2 == NULL)
	{
		temp = -1;
		goto free_and_Return;
	}
	if( ISFS_Read(dest_handler,Data2,D2stat->file_length) > 0 )
	{
		if( !CompareChecksum(buffer,status->file_length,Data2,D2stat->file_length))
		{
			temp = -1;
		}
	}
	else
	{
		temp = -1;
	}

free_and_Return:
	if(Data2 != NULL)
	{
		free(Data2);
		Data2 = NULL;
	}
	if(D2stat != NULL)
	{
		free(D2stat);
		D2stat = NULL;
	}
	if(source_handler)
		ISFS_Close(source_handler);
	if(dest_handler)
		ISFS_Close(dest_handler);
	if(status)
	{
		free(status);
		status = NULL;
	}
	if(buffer)
	{
		free(buffer);
		buffer = NULL;
	}
	if (temp < 0)
		return -80;

    return 0;
}
bool UserYesNoStop()
{
	u16 pDown;
	u16 GCpDown;
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();
		pDown = WPAD_ButtonsDown(0);
		GCpDown = PAD_ButtonsDown(0);
		if (pDown & WPAD_BUTTON_A || GCpDown & PAD_BUTTON_A)
		{
			return true;
		}
		if (pDown & WPAD_BUTTON_B || GCpDown & PAD_BUTTON_B)
		{
			return false;
		}
		if (pDown & WPAD_BUTTON_HOME || GCpDown & PAD_BUTTON_START)
		{
			abort("User command.");
			break;
		}
	}
	//it should never get here, but to kill that silly warning... :)
	return false;
}

int main(int argc, char **argv)
{
	VIDEO_Init();
	vmode = VIDEO_GetPreferredMode(NULL);
	//adjust overscan a bit
	GX_AdjustForOverscan(vmode, vmode, 16,0);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	console_init(xfb,20,20,vmode->fbWidth,vmode->xfbHeight,vmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	WPAD_Init();
	PAD_Init();

	CheckForGecko();
	gprintf("resolution is %dx%d\n",vmode->viWidth,vmode->viHeight);
	printf("\x1b[2;0H");
	printf("IOS %d rev %d\n\n\t",IOS_GetVersion(),IOS_GetRevision());
	printf("Priiloader rev %d (preloader v0.30 mod) Installation / Removal Tool\n\n\n\n\t",SVN_REV);
	printf("\t\t\t\t\tPLEASE READ THIS CAREFULLY\n\n\t");
	printf("\t\tTHIS PROGRAM/TOOL COMES WITHOUT ANY WARRANTIES!\n\t");
	printf("\t\tYOU ACCEPT THAT YOU INSTALL THIS AT YOUR OWN RISK\n\n\n\t");
	printf("THE AUTHOR(S) CANNOT BE HELD LIABLE FOR ANY DAMAGE IT MIGHT CAUSE\n\n\t");
	printf("\tIF YOU DO NOT AGREE WITH THESE TERMS TURN YOUR WII OFF\n\n\n\n\t");
	printf("\t\t\tPress (+/A) to install or update Priiloader\n\t");
	printf("\t Press (-/Y) to remove Priiloader and restore system menu\n\t");
	printf("\tHold Down (B) with any above options to use IOS249 (cios)\n\t");
	printf("\t Press (HOME/Start) to chicken out and quit the installer!\n\n\t");
	printf("\t\t\t\tEnjoy! DacoTaco, _Dax_ & /phpgeek\n							");

	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();
		u16 pDown = WPAD_ButtonsDown(0);
		u16 GCpDown = PAD_ButtonsDown(0);
		u16 pHeld = WPAD_ButtonsHeld(0);
		u16 GCpHeld = PAD_ButtonsHeld(0);
		if (pDown & WPAD_BUTTON_PLUS || pDown & WPAD_BUTTON_MINUS || GCpDown & PAD_BUTTON_A || GCpDown & PAD_BUTTON_Y)
		{
			static u32 tmp_ikey ATTRIBUTE_ALIGN(32);
			static u32 tmd_size ATTRIBUTE_ALIGN(32);
			static u64 title_id ATTRIBUTE_ALIGN(32)=0x0000000100000002LL;

			s32 fd,fs;
			u32 id = 0;

			if (pHeld & WPAD_BUTTON_B || GCpHeld & PAD_BUTTON_B )
			{
				WPAD_Shutdown();
        		IOS_ReloadIOS(249);
        		WPAD_Init();
        	}
			printf("\x1b[2J\x1b[2;0H");
			fflush(stdout);
			printf("IOS %d rev %d\n\n\n\n\n",IOS_GetVersion(),IOS_GetRevision());
        	__ES_Init();
			fd = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)su_tmd, su_tmd_size, (signed_blob *)su_tik, su_tik_size, &tmp_ikey);
			if(fd < 0)
			{
				printf("\x1b[%u;%dm", 33, 1);
				printf("ES_Identify failed,error %d(-%#x).ios%d not patched with ES_DIVerify?\n",fd,-fd,IOS_GetVersion());
				printf("using cios (holding b and then pressing + or - ) will probably solve this.\nNOTE: you need CIOS for this.\n");
				printf("Do you wish to continue?\n");
				printf("A = Yes       B = No       Home/Start = Exit\n");
				printf("\x1b[%u;%dm", 37, 1);
				if(!UserYesNoStop())
				{
					abort("ES_Identify failed");
				}
				else
				{
					printf("continuing...\n\n");
				}
			}
			else
				printf("Logged in as \"su\"!\n");
			if (ISFS_Initialize() < 0)
				abort("Failed to get root");
			printf("Got ROOT!\n");
		
			//read TMD so we can get the main booting dol
			fs = ES_GetStoredTMDSize(title_id,&tmd_size);
			if (fs < 0)
			{
				abort("Unable to get stored tmd size");
			}

			signed_blob *TMD = (signed_blob *)memalign(32,(tmd_size+31)&(~31));
			if (TMD == NULL)
			{
				abort("Unable to prepare tmd for memory");
			}

			memset(TMD,0,tmd_size);
			fs = ES_GetStoredTMD(title_id,TMD,tmd_size);
			if (fs < 0)
			{
				free(TMD);
				TMD = NULL;
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
				TMD = NULL;
				abort("Unable to retrieve title id");
			}

			char * original_app = (char*)memalign(32,256);
			char * copy_app = (char*)memalign(32,256);
			if (original_app == NULL || copy_app == NULL)
			{
				free(TMD);
				TMD = NULL;
				abort("Unable to prepare title for memory");
			}

			memset(original_app,0,256);
			memset(copy_app,0,256);
			sprintf(original_app, "/title/00000001/00000002/content/%08x.app",id);
			sprintf(copy_app, "/title/00000001/00000002/content/%08x.app",id);
			copy_app[33] = '1';

			if(TMD)
			{
				free(TMD);
				TMD = NULL;
			}
			
			if (pDown & WPAD_BUTTON_PLUS || GCpDown & PAD_BUTTON_A)
			{
				//install or update
				fstats * status;
				s32 ret = 0;
				bool CopyTicket = false;
				bool Priiloader_found = false;
				//check if the copy ticket exists
				printf("Checking for Priiloader...\n");
				gprintf("Checking for copy ticket...\n");
				fd = ISFS_Open("/title/00000001/00000002/content/ticket",ISFS_OPEN_READ);
				if (fd <0)
				{
					printf("\tPriiloader system menu ticket not found.\n\tTrying to read original ticket...\n");
					ISFS_Close(fd);
					fd = ISFS_Open("/ticket/00000001/00000002.tik",ISFS_OPEN_READ);
					//"/ticket/00000001/00000002.tik" -> original path which should be there on every wii.
					//however needs nand permissions which trucha gives >_>
					if (fd < 0)
					{
						switch(fd)
						{
							case ISFS_EINVAL:
								abort("Unable to read ticket.path is wrong/too long or ISFS isn't init yet?");
								break;
							case ISFS_ENOMEM:
								abort("Unable to read ticket.(Out of memory)");
								break;
							case -106:
								abort("Ticket not found");
								break;
							case -102:
								abort("Unauthorised to get ticket. is ios%d trucha signed?",IOS_GetVersion());
								break;
							default:
								abort("Unable to read ticket. error %d. ",fd);
								break;
						}

					}
					else
					{
						printf("\tOriginal ticket loaded & set for copy\n");
						CopyTicket = true;
					}
				}
				ISFS_Close(fd);
				
				//check copy app
				gprintf("checking for copy of SystemMenu Dol\n");
				//TODO : redo using the code from Luna + size check
				fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					Priiloader_found = false;
					printf("Priiloader not found.\nInstalling Priiloader...\n\n\n");
				}
				else
				{
					status = (fstats*)memalign(32,sizeof(fstats));
					memset(status,0,sizeof(fstats));
					if (ISFS_GetFileStats(fd,status) < 0)
					{
						printf("\x1b[%u;%dm", 33, 1);
						printf("\n\nWARNING: failed to get stats of %s.  Ignore Priiloader \"installation\" ?\n",copy_app);
						printf("A = Yes       B = No(Recommended if priiloader is installed)       Home/Start = Exit\n");
						printf("\x1b[%u;%dm", 37, 1);
						if(UserYesNoStop())
						{
							printf("Ignoring Priiloader Installation...\n\n");
							Priiloader_found = false;
						}
						else
						{
							printf("Using Current Priiloader Installation...\n\n");
							Priiloader_found = true;
						}
					}
					else
					{
						if ( status->file_length == 0 )
						{
							printf("\x1b[%u;%dm", 33, 1);
							printf("\n\nWARNING: %s is reported as 0kB!\n  Ignore priiloader \"installation\" ?\n",copy_app);
							printf("It is recommended that you ignore the installation if Priiloader hasn't\n  succesfully installed yet\n");
							printf("A = Yes       B = No       Home/Start = Exit\n");
							printf("\x1b[%u;%dm", 37, 1);
							if(UserYesNoStop())
							{
								printf("Ignoring Priiloader \"Installation\"...\n\nReinstalling Priiloader...\n\n\n");
								Priiloader_found = false;
								break;
							}
							else
							{
								printf("Using Current Priiloader \"Installation\"...\n\nUpdating Priiloader...\n\n\n");
								Priiloader_found = true;
							}
						}
						else
						{
							Priiloader_found = true;
							printf("Priiloader installation found\nUpdating Priiloader...\n\n\n");
						}
					}
					free(status);
					status = NULL;
					ISFS_Close(fd);
				}
				if(CopyTicket)
				{
					printf("Copying system menu ticket...");
					char original_tik[256];
					char copy_tik[256];
					sprintf(original_tik, "/ticket/00000001/00000002.tik");
					sprintf(copy_tik, "/title/00000001/00000002/content/ticket");
					if (nand_copy(original_tik,copy_tik) < 0)
					{
						abort("Unable to copy the system menu ticket");
					}
					printf("Done!\n");
				}
				else
				{
					printf("Skipping copy of system menu ticket...\n");
				}
				if(!Priiloader_found)
				{
					printf("Moving System Menu app...");
					ret = nand_copy(original_app,copy_app);
					if (ret < 0)
					{
						if (ret == -80)
						{
							//checksum issues
							printf("\x1b[%u;%dm", 33, 1);
							printf("\nWARNING!!\n  Installer could not calculate the Checksum for the System menu app");
							printf("\nbut Copy was successfull.\n");
							printf("Do you want the Continue ?\n");
							printf("A = Yes       B = No       Home/Start = Exit\n  ");
							printf("\x1b[%u;%dm", 37, 1);
							if(!UserYesNoStop())
							{
								printf("reverting changes...\n");
								ISFS_Delete(copy_app);
								abort("System Menu Copying Failure");
							}		
							else
								printf("\nDone!\n");
						}
						else
							abort("\nUnable to move the system menu");
					}
					else
					{
						gprintf("Moving System Menu Done\n");
						printf("Done!\n");
					}
				}
				else
				{
					printf("Skipping Moving of System menu app...\n");
				}				  
				printf("deleting extra priiloader files...\n");
				ret = ISFS_Delete("/title/00000001/00000002/data/loader.ini");
				if(ret < 0)
				{
					printf("\x1b[%u;%dm", 33, 1);
					printf("WARNING : Priiloader Settings not deleted. error %d\n",ret);
					printf("\x1b[%u;%dm", 37, 1);
					printf("continuing...\n");
					sleep(4);
				}
				else
					printf("Priiloader Settings deleted\n");
				gprintf("/title/00000001/00000002/data/loader.ini deletion returned %d\n",ret);
				ret = ISFS_Delete("/title/00000001/00000002/data/password.txt");
				if(ret < 0)
				{
					printf("\x1b[%u;%dm", 33, 1);
					printf("WARNING : Priiloader Password file not deleted. error %d\n",ret);
					printf("\x1b[%u;%dm", 37, 1);
					printf("continuing...\n");
					sleep(4);
				}
				else
					printf("Priiloader Password file deleted\n");
				gprintf("/title/00000001/00000002/data/password.txt deletion returned %d\n",ret);
				printf("Done!\n");
				printf("Writing Priiloader app...");
				gprintf("Writing Priiloader\n");
				ISFS_Delete(original_app);
				ISFS_CreateFile(original_app,0,ISFS_OPEN_RW,ISFS_OPEN_RW,ISFS_OPEN_RW);
				fd = ISFS_Open(original_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					nand_copy(copy_app,original_app);
					abort("\nFailed to open file for Priiloader writing.");
				}
				ret = ISFS_Write(fd,priiloader_app,priiloader_app_size);
				if (ret < 0 ) //check if the app was writen correctly				
				{
					ISFS_Close(fd);
					nand_copy(copy_app,original_app);
					gprintf("Write failed. ret %d\n",ret);
					abort("\nWrite of Priiloader app failed.");
				}
				printf("Done!!\n");
				gprintf("Wrote Priiloader App.Checking Installation\n");
				printf("\nChecking Priiloader Installation...\n");
				status = (fstats*)memalign(32,sizeof(fstats));
				if (ISFS_GetFileStats(fd,status) < 0)
				{
					ISFS_Close(fd);
					nand_copy(copy_app,original_app);
					abort("Failed to get stats of %s. System Menu Recovered.",original_app);
				}
				else
				{
					if ( status->file_length != priiloader_app_size )
					{
						ISFS_Close(fd);
						nand_copy(copy_app,original_app);
						abort("Written Priiloader app isn't the correct size.System Menu Recovered.");
					}
					else
					{
						gprintf("Size Check Success\n");
						printf("Size Check Success!\n");
					}
				}
				//reset fd. otherwise the read data isn't correct? (checked by writing data away before comparing) :-/
				ISFS_Close(fd);
				fd = ISFS_Open(original_app,ISFS_OPEN_READ);
				u8 *AppData = (u8 *)memalign(32,status->file_length);
				if (AppData)
					ret = ISFS_Read(fd,AppData,status->file_length);
				else
				{
					ISFS_Close(fd);
					if(status)
					{
						free(status);
						status = NULL;
					}
					nand_copy(copy_app,original_app);
					abort("Checksum comparison Failure! MemAlign Failure of AppData %u\n",ret);
				}
				if (ret < 0)
				{
					ISFS_Close(fd);
					if (AppData)
					{
						free(AppData);
						AppData = NULL;
					}
					if(status)
					{
						free(status);
						status = NULL;
					}
					nand_copy(copy_app,original_app);
					abort("Checksum comparison Failure! read of priiloader app returned %u\n",ret);
				}
				if(CompareChecksum((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
					printf("Checksum comparison Success!\n");
				else
				{
					ISFS_Close(fd);
					if (AppData)
					{
						free(AppData);
						AppData = NULL;
					}
					if(status)
					{
						free(status);
						status = NULL;
					}
					nand_copy(copy_app,original_app);
					ISFS_Delete(copy_app);
					abort("Checksum comparison Failure!\n");
				}
				if (AppData)
				{
					free(AppData);
					AppData = NULL;
				}
				if(status)
				{
					free(status);
					status = NULL;
				}
				ISFS_Close(fd);
				gprintf("Priiloader Installation Complete\n");
				printf("Done!!!\n\n");
				if(Priiloader_found)
					printf("Update done, exiting to loader... waiting 5s...\n");
				else
					printf("Install done, exiting to loader... waiting 5s...\n");
				ISFS_Deinitialize();
				__ES_Close();
				sleep(5);
				exit(0);
			}

			else if (pDown & WPAD_BUTTON_MINUS || GCpDown & PAD_BUTTON_Y )
			{
				printf("Checking for Priiloader...\n");
				fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					abort("Priiloader not found");
				}
				else
				{
					printf("Priiloader installation found.removing...\n\nRemoving Priiloader...");
					ISFS_Delete(original_app);
					printf("Done!\nRestoring System menu app...");
					s32 ret = nand_copy(copy_app,original_app);
					if (ret < 0)
					{
						if(ret == -80)
						{
							//checksum issues
							printf("\x1b[%u;%dm", 33, 1);
							printf("\nWARNING!!\n  Installer could not calculate the Checksum when coping the System menu app\n");
							printf("back! the app however was copied...\n");
							printf("Do you want to Continue ?\n");
							printf("A = Yes       B = No       Home/Start = Exit\n  ");
							printf("\x1b[%u;%dm", 37, 1);
							if(!UserYesNoStop())
							{
								printf("reverting changes...\n");
								ISFS_Close(fd);
								ISFS_CreateFile(original_app,0,3,3,3);
								fd = ISFS_Open(original_app,ISFS_OPEN_RW);
								ISFS_Write(fd,priiloader_app,priiloader_app_size);
								ISFS_Close(fd);
								abort("System Menu Copying Failure");
							}		
						}
						else
						{
							ISFS_Close(fd);
							ISFS_CreateFile(original_app,0,3,3,3);
							fd = ISFS_Open(original_app,ISFS_OPEN_RW);
							ISFS_Write(fd,priiloader_app,priiloader_app_size);
							ISFS_Close(fd);
							abort("\nUnable to restore the system menu! (ret = %d)",ret);
						}
					}
					ISFS_Close(fd);
					ISFS_Delete(copy_app);
					printf("Done!\n");
					printf("Deleting extra Priiloader files...");
					ret = ISFS_Delete("/title/00000001/00000002/data/password.txt");
					gprintf("password.txt : %d\n",ret);
					ret = ISFS_Delete("/title/00000001/00000002/data/loader.ini");
					gprintf("loader.ini : %d\n",ret);
					//its best we delete that ticket but its completely useless and will only get in our 
					//way when installing again later...
					ret = ISFS_Delete("/title/00000001/00000002/content/ticket");
					gprintf("ticket : %d\n",ret);
					ret = ISFS_Delete("/title/00000001/00000002/data/hacks_s.ini");
					gprintf("hacks_s.ini : %d\n",ret);
					ret = ISFS_Delete("/title/00000001/00000002/data/hacks.ini");
					gprintf("hacks.ini : %d\n",ret);
					ret = ISFS_Delete("/title/00000001/00000002/data/main.bin");
					gprintf("main.bin : %d\n",ret);
					printf("Done!\n\n");
					printf("Removal done, exiting to loader... waiting 5s...\n");
					ISFS_Deinitialize();
					__ES_Close();
					sleep(5);
					exit(0);
				}
			}
		}
		else if (pDown & WPAD_BUTTON_HOME || GCpDown & PAD_BUTTON_START)
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
