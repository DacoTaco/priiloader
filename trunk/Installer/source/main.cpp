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
//defines
//#define BETA 1

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>
#include <ogc/isfs.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <ogc/usbgecko.h>
#include <ogc/es.h>

//project includes

//rev version
#include "../../Shared/svnrev.h"
// Bin Files
#include "certs_bin.h"
#include "su_tik.h"
#include "su_tmd.h"

// Priiloader Application
#include "priiloader_app.h"

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;
bool GeckoFound = false;
char original_app[128] ATTRIBUTE_ALIGN(32);
char copy_app[128] ATTRIBUTE_ALIGN(32);

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
void sleepx (int seconds)
{
	time_t start,end;
	time(&start);
	//i hate while loops. but its safer then sleep this way
	while(difftime(end, start) < seconds)
	{
		time(&end);
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
	printf("%s, aborting mission...\n", text);
	gprintf("%s, aborting mission...\n", text);
	ISFS_Deinitialize();
	//__ES_Close();
	sleepx(5);
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
s32 nand_copy(const char *source, const char *destination,u8 patch_permissions)
{
    u8 *buffer = NULL;
    s32 source_handler, ret, dest_handler;
	ISFS_Delete(destination);
    source_handler = ISFS_Open(source,ISFS_OPEN_READ);
    if (source_handler < 0)
	{
		gprintf("failed to open source : %s\n",source);
		return source_handler;
	}
   
	if(patch_permissions == 0)
	{
		//its nice and all; but has some holes. for example : booting system menu from priiloader will (for some odd reason) result in access denied
		//get attributes
		u32 owner;
		u16 group;
		u8 attributes, ownerperm, groupperm, otherperm;
		ret = ISFS_GetAttr(source, &owner, &group, &attributes, &ownerperm, &groupperm, &otherperm);
		if(ret < 0 )
		{
			//attribute getting failed. returning to default
			printf("\x1b[%u;%dm", 33, 1);
			printf("\nWARNING : failed to get file's permissions. using defaults\n");
			printf("\x1b[%u;%dm", 37, 1);
			gprintf("permission failure on %s! error %d\n",destination,ret);
			gprintf("writing %s with max permissions\n",destination);
			ISFS_Delete(destination);
			ret = ISFS_CreateFile(destination,0,ISFS_OPEN_RW,ISFS_OPEN_RW,ISFS_OPEN_RW);
		}
		else
		{
			gprintf("ret %d owner %d group %d attributes %X perm:%X-%X-%X\n", ret, owner, (s32)group, (s32)attributes, (s32)ownerperm, (s32)groupperm, (s32)otherperm);
			ISFS_Delete(destination);
			ret = ISFS_CreateFile(destination,attributes,ownerperm,groupperm,otherperm);
		}
	}
	else
	{
		ISFS_Delete(destination);
		ret = ISFS_CreateFile(destination,0,ISFS_OPEN_RW,ISFS_OPEN_RW,ISFS_OPEN_RW);
	}
	if (ret != ISFS_OK) 
	{
		printf("Failed to create file %s. ret = %d\n",destination,ret);
		gprintf("Failed to create file %s. ret = %d\n",destination,ret);
		ISFS_Close(source_handler);
		return ret;
	}
    dest_handler = ISFS_Open(destination,ISFS_OPEN_RW);
    if (dest_handler < 0)
	{
		gprintf("failed to open destination : %s\n",destination);
		ISFS_Delete(destination);
		ISFS_Close(source_handler);
		return dest_handler;
	}

    fstats * status = (fstats*)memalign(32,sizeof(fstats));
    ret = ISFS_GetFileStats(source_handler,status);
    if (ret < 0)
    {
		printf("\n\n  Failed to get information about %s!\n",source);
		sleepx(2);
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
		gprintf("buffer failed to align\n");
		sleepx(2);
        ISFS_Close(source_handler);
        ISFS_Close(dest_handler);
        ISFS_Delete(destination);
        free(status);
		status = NULL;
		return 0;
	}

    ret = ISFS_Read(source_handler,buffer,status->file_length);
    if (ret < 0)
    {
		printf("\n\nFailed to Read Data from %s!\n",source);
		sleepx(2);
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
		gprintf("failed to write destination : %s\n",destination);
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
	{
		ISFS_Delete(destination);
		return -80;
	}
    return 1;
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
			abort("User command");
			break;
		}
	}
	//it should never get here, but to kill that silly warning... :)
	return false;
}
u8 proccess_delete_ret( s32 ret )
{
	if(ret == -106)
	{
		printf("\x1b[%u;%dm", 32, 1);
		printf("Not found\n");
		printf("\x1b[%u;%dm", 37, 1);
	}
	else if(ret == -102)
	{
		printf("\x1b[%u;%dm", 33, 1);
		printf("Error deleting file: access denied\n");
		printf("\x1b[%u;%dm", 37, 1);
	}
	else if (ret < 0)
	{
		printf("\x1b[%u;%dm", 33, 1);
		printf("Error deleting file. error %d\n",ret);
		printf("\x1b[%u;%dm", 37, 1);
	}
	else
	{
		printf("\x1b[%u;%dm", 32, 1);
		printf("Deleted\n");
		printf("\x1b[%u;%dm", 37, 1);
	}
	return 1;
}
u8 Delete_Priiloader_Files( u8 mode )
{
	bool settings = false;
	bool hacks = false;
	bool password = false;
	bool main_bin = false;
	bool ticket = false;
	switch(mode)
	{
		case 2: //remove
			hacks = true;
			main_bin = true;
			ticket = true;
		case 0: //install
		case 1: //update
			settings = true;
			password = true;
		default:
			break;
	}
	s32 ret = 0;
	if(password)
	{
		ret = ISFS_Delete("/title/00000001/00000002/data/password.txt");
		gprintf("password.txt : %d\n",ret);
		printf("password file : ");
		proccess_delete_ret(ret);
	}
	if(settings)
	{
		ret = ISFS_Delete("/title/00000001/00000002/data/loader.ini");
		gprintf("loader.ini : %d\n",ret);
		printf("Settings file : ");
		proccess_delete_ret(ret);
	}
	//its best we delete that ticket but its completely useless and will only get in our 
	//way when installing again later...
	if(ticket)
	{
		ret = ISFS_Delete("/title/00000001/00000002/content/ticket");
		gprintf("ticket : %d\n",ret);
		printf("Ticket : ");
		proccess_delete_ret(ret);
	}
	if(hacks)
	{
		ret = ISFS_Delete("/title/00000001/00000002/data/hacks_s.ini");
		gprintf("hacks_s.ini : %d\n",ret);
		printf("Hacks_s.ini : ");
		proccess_delete_ret(ret);

		ret = ISFS_Delete("/title/00000001/00000002/data/hacks.ini");
		gprintf("hacks.ini : %d\n",ret);
		printf("Hacks.ini : ");
		proccess_delete_ret(ret);

		ret = ISFS_Delete("/title/00000001/00000002/data/hacksh_s.ini");
		gprintf("hacksh_s.ini : %d\n",ret);
		printf("Hacksh_s.ini : ");
		proccess_delete_ret(ret);

		ret = ISFS_Delete("/title/00000001/00000002/data/hackshas.ini");
		gprintf("hacks_hash : %d\n",ret);
		printf("Hacks_hash : ");
		proccess_delete_ret(ret);

	}
	if(main_bin)
	{
		ret = ISFS_Delete("/title/00000001/00000002/data/main.bin");
		gprintf("main.bin : %d\n",ret);
		printf("main.bin : ");
		proccess_delete_ret(ret);
	}
	return 1;
}
s8 PatchTMD( u64 title_id , u8 delete_mode )
{
	if(delete_mode == 0)
	{
		gprintf("patching TMD...\n");
		printf("Patching TMD...");
	}
	else
	{
		//return 1;
		gprintf("restoring TMD...\n");
		printf("Restoring System Menu TMD...\n");
	}
	char TMD_Path[265] ATTRIBUTE_ALIGN(32);
	char TMD_Path2[265] ATTRIBUTE_ALIGN(32);
	u8 *mTMD = 0;
	u8 *TMD_ptr = 0;
	u8 *TMD_chk = NULL;
	s32 fd = 0;
	s32 r = 0;
	memset(TMD_Path,0,sizeof(TMD_Path));
	memset(TMD_Path2,0,sizeof(TMD_Path));
	sprintf(TMD_Path, "/title/%08x/%08x/content/title.tmd",(u32)(title_id >> 32),(u32)title_id);
	sprintf(TMD_Path2, "/title/%08x/%08x/content/title_or.tmd",(u32)(title_id >> 32),(u32)title_id);
#ifdef _DEBUG
	gprintf("Path : %s\n",TMD_Path);
#endif
	fd = ISFS_Open(TMD_Path2,ISFS_OPEN_READ);
	if(fd < 0)
	{
		ISFS_Close(fd);
		if(delete_mode)
		{
			printf("TMD backup not found. leaving TMD alone...\n");
			return 0;
		}
		else
		{
			//got to make tmd copy :)
			gprintf("Making tmd backup...\n");
			r = nand_copy(TMD_Path,TMD_Path2,0);
			if ( r < 0)
			{
				gprintf("Failure making TMD backup.error %d\n",r);
				//abort("TMD backup/Patching Failure : error %d",r);
				printf("TMD backup/Patching Failure : error %d",r);
				goto _return;
			}
		}
	}
	else
	{
		ISFS_Close(fd);
		gprintf("TMD backup found\n");
		//not so sure why we'd want to delete the tmd modification...
		if(delete_mode)
		{
			//printf("restoring original TMD...\n");
			if ( nand_copy(TMD_Path2,TMD_Path,0) < 0)
			{
				if(r == -80)
				{
					//checksum issues
					printf("\x1b[%u;%dm", 33, 1);
					printf("\nWARNING!!\nInstaller could not calculate the Checksum when restoring the TMD back!\n");
					printf("the TMD however was copied...\n");
					printf("Do you want to Continue ?\n");
					printf("A = Yes       B = No       Home/Start = Exit\n  ");
					printf("\x1b[%u;%dm", 37, 1);
					if(!UserYesNoStop())
					{
						nand_copy(TMD_Path,TMD_Path2,0);
						abort("TMD restoring failure.");
					}		
				}
				else
				{
					printf("\x1b[%u;%dm", 33, 1);
					printf("UNABLE TO RESTORE THE SYSTEM MENU TMD!!!\n\nTHIS COULD BRICK THE WII SO PLEASE REINSTALL SYSTEM MENU\nWHEN RETURNING TO THE HOMEBREW CHANNEL!!!\n\n");
					printf("\x1b[%u;%dm", 37, 1);
					printf("press A to return to the homebrew channel\n");
					nand_copy(TMD_Path,TMD_Path2,0);
					UserYesNoStop();
					exit(0);
				}
			}
			return 1;
		}
	}
	u32 tmd_size_temp;
	r=ES_GetStoredTMDSize(title_id, &tmd_size_temp);
	if(r < 0)
	{
		gprintf("Patch_TMD: GetStoredTMDSize error %d\n",r);
		goto _return;
	}
	mTMD = (u8* )memalign( 32, (tmd_size_temp+31)&(~31) );
	if(mTMD == NULL)
	{
		gprintf("Patch_TMD: memalign TMD failure\n");
		goto _return;
	}
	memset(mTMD, 0, tmd_size_temp);

	r=ES_GetStoredTMD(title_id, (signed_blob*)mTMD, tmd_size_temp);
	if(r < 0)
	{
		gprintf("Patch_TMD: GetStoredTMD error %d\n",r);
		free(mTMD);
		goto _return;
	}
	TMD_ptr = (u8*)mTMD+0x1db;
	//gprintf("address is 0x%X & 0x%X\n",mTMD,TMD_ptr);
	gprintf("detected access rights : 0x%X\n",*TMD_ptr);
	if(*TMD_ptr == 0x03)
	{
		gprintf("no TMD modification needed\n");
		printf("no Patches needed\n");
		free(mTMD);
		return 2;
	}
	else
	{
		*TMD_ptr = 0x03;
		DCFlushRange(TMD_ptr,8);
		if(*TMD_ptr != 0x03)
		{
			gprintf("rights change failure.\n");
			free(mTMD);
			goto _return;
		}
		//get attributes
		u32 owner;
		u16 group;
		u8 attributes, ownerperm, groupperm, otherperm;
		r = ISFS_GetAttr(TMD_Path, &owner, &group, &attributes, &ownerperm, &groupperm, &otherperm);
		if(r < 0)
		{
			//attribute getting failed. returning to default
			printf("\x1b[%u;%dm", 33, 1);
			printf("\nWARNING : failed to get TMD permissions. using defaults\nthis *could* result in syscheck saying nand permissions are always patched");
			printf("\x1b[%u;%dm", 37, 1);
			ISFS_Delete(TMD_Path);
			ISFS_CreateFile(TMD_Path,0,3,3,0);
		}
		else
		{
			gprintf("ret %d owner %d group %d attributes %X perm:%X-%X-%X\n", r, owner, (s32)group, (s32)attributes, (s32)ownerperm, (s32)groupperm, (s32)otherperm);
			ISFS_Delete(TMD_Path);
			ISFS_CreateFile(TMD_Path,attributes,ownerperm,groupperm,otherperm);
		}
		fd = ISFS_Open(TMD_Path,ISFS_OPEN_RW);
		if(fd < 0)
		{
			gprintf("failed to open TMD(%s)\n",TMD_Path);
			free(mTMD);
			goto _return;
		}
		r = ISFS_Write(fd,mTMD,tmd_size_temp);
		if(r < 0)
		{
			free(mTMD);
			gprintf("write failure >_>\n");
			goto _return;
		}
		ISFS_Close(fd);


//lets reopen the TMD for checksum comparing to mTMD
		fd = ISFS_Open(TMD_Path,ISFS_OPEN_READ);
		if(fd < 0)
		{
			gprintf("failed to open TMD for checksum comparing!(%s)\n",TMD_Path);
			goto _checkreturn;
			//checksum issues
		}
		TMD_chk = (u8 *)memalign(32,(tmd_size_temp+31)&(~31));
		if (TMD_chk == NULL)
		{
			goto _checkreturn;
		}
		r = ISFS_Read(fd,TMD_chk,tmd_size_temp);
		if(r < 0)
		{
			goto _checkreturn;
		}
		if( !CompareChecksum(TMD_chk,tmd_size_temp,mTMD,tmd_size_temp))
		{
			goto _checkreturn;
		}
		else
		{
			gprintf("TMD checksum success!\n");
		}

		free(mTMD);
		printf("Done\n");
	}
_return:
	if(TMD_chk)
	{
		free(TMD_chk);
		TMD_chk = NULL;
	}
	if(mTMD)
	{
		free(mTMD);
		mTMD = NULL;
	}
	if (fd < r)
	{
		r = fd;
	}
	if(fd)
	{
		ISFS_Close(fd);
	}
	if (r < 0)
	{
		printf("\x1b[%u;%dm", 33, 1);
		printf("\nWARNING!!\n  Installer couldn't Patch the system menu TMD.");
		printf("Priiloader could still end up being installed but could end up working differently\n");
		printf("Do you want the Continue ?\n");
		printf("A = Yes       B = No       Home/Start = Exit\n  ");
		printf("\x1b[%u;%dm", 37, 1);
		if(!UserYesNoStop())
		{
			fd = ISFS_Open(TMD_Path2,ISFS_OPEN_RW);
			if(fd >= 0)
			{
				//the backup is there. as safety lets copy it back.
				ISFS_Close(fd);
				nand_copy(TMD_Path2,TMD_Path,0);
			}
			abort("TMD failure\n");
		}		
		else
		{
			nand_copy(TMD_Path2,TMD_Path,0);
			printf("\nDone!\n");
		}
		return r;
	}
	else
		return 1;
_checkreturn:
	if(fd)
	{
		ISFS_Close(fd);
	}
	if(TMD_chk)
	{
		free(TMD_chk);
		TMD_chk = NULL;
	}
	printf("\x1b[%u;%dm", 33, 1);
	printf("\nWARNING!!\n  Installer could not calculate the Checksum for the TMD!");
	printf("\nbut Patch write was successfull.\n");
	printf("Do you want the Continue ?\n");
	printf("A = Yes       B = No       Home/Start = Exit\n  ");
	printf("\x1b[%u;%dm", 37, 1);
	if(!UserYesNoStop())
	{
		printf("reverting changes...\n");
		if(mTMD)
		{
			free(mTMD);
			mTMD = NULL;
		}
		nand_copy(TMD_Path2,TMD_Path,0);
		abort("TMD Patch failure\n");
	}		
	else
	{
		printf("\nDone!\n");
	}
	if(mTMD)
	{
		free(mTMD);
		mTMD = NULL;
	}
	return -80;
}
s8 CopyTicket ( u64 title_id )
{
	s32 fd = 0;
	char TIK_Path_dest[265] ATTRIBUTE_ALIGN(32);
	char TIK_Path_org[265] ATTRIBUTE_ALIGN(32);
	memset(TIK_Path_dest,0,sizeof(TIK_Path_dest));
	memset(TIK_Path_org,0,sizeof(TIK_Path_org));
	sprintf(TIK_Path_dest, "/title/%08x/%08x/content/ticket",(u32)(title_id >> 32),(u32)title_id);
	sprintf(TIK_Path_org, "/ticket/%08x/%08x.tik",(u32)(title_id >> 32),(u32)title_id);


	gprintf("Checking for copy ticket...\n");
	//fd = ISFS_Open("/title/00000001/00000002/content/ticket",ISFS_OPEN_READ);
	fd = ISFS_Open(TIK_Path_dest,ISFS_OPEN_READ);
	if (fd <0)
	{
		printf("Priiloader system menu ticket not found.\n\tTrying to read original ticket...\n");
		ISFS_Close(fd);
		//fd = ISFS_Open("/ticket/00000001/00000002.tik",ISFS_OPEN_READ);
		fd = ISFS_Open(TIK_Path_org,ISFS_OPEN_READ);
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
			ISFS_Close(fd);
			printf("Copying system menu ticket...");
			/*char original_tik[256];
			char copy_tik[256];
			sprintf(original_tik, "/ticket/00000001/00000002.tik");
			sprintf(copy_tik, "/title/00000001/00000002/content/ticket");*/
			if (nand_copy(TIK_Path_org,TIK_Path_dest,1) < 0)
			{
				abort("Unable to copy the system menu ticket");
			}
			printf("Done!\n");
		}
	}
	else
	{
		printf("Skipping copy of system menu ticket...\n");
		ISFS_Close(fd);
	}
	return 1;
}
s8 Copy_SysMenu( void )
{
	s32 ret = 0;
	//system menu coping
	printf("Moving System Menu app...");
	ret = nand_copy(original_app,copy_app,1);
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
			abort("\nUnable to move the system menu. error %d",ret);
	}
	else
	{
		gprintf("Moving System Menu Done\n");
		printf("Done!\n");
	}
	return 1;
}
s8 WritePriiloader( void )
{
	s32 ret = 0;
	s32 fd = 0;
	fstats* status = NULL;

	printf("Writing Priiloader app...");
	gprintf("Writing Priiloader\n");

	ISFS_Delete(original_app);
	ISFS_CreateFile(original_app,0,ISFS_OPEN_RW,ISFS_OPEN_RW,ISFS_OPEN_RW);
	fd = ISFS_Open(original_app,ISFS_OPEN_RW);
	if (fd < 0)
	{
		nand_copy(copy_app,original_app,1);
		ISFS_Delete(copy_app);
		abort("\nFailed to open file for Priiloader writing");
	}
	ret = ISFS_Write(fd,priiloader_app,priiloader_app_size);
	if (ret < 0 ) //check if the app was writen correctly				
	{
		ISFS_Close(fd);
		nand_copy(copy_app,original_app,1);
		ISFS_Delete(copy_app);
		gprintf("Write failed. ret %d\n",ret);
		abort("\nWrite of Priiloader app failed");
	}
	printf("Done!!\n");
	gprintf("Wrote Priiloader App.Checking Installation\n");
	printf("\nChecking Priiloader Installation...\n");
	status = (fstats*)memalign(32,sizeof(fstats));
	//reset fd. the write might have been delayed. a close causes the delayed write to happen and creates the the file correctly
	ISFS_Close(fd);
	fd = ISFS_Open(original_app,ISFS_OPEN_READ);
	if (fd < 0)
	{
		nand_copy(copy_app,original_app,1);
		ISFS_Delete(copy_app);
		abort("\nFailed to open file for Priiloader checking");
	}
	if (ISFS_GetFileStats(fd,status) < 0)
	{
		ISFS_Close(fd);
		nand_copy(copy_app,original_app,1);
		abort("Failed to get stats of %s. System Menu Recovered",original_app);
	}
	else
	{
		if ( status->file_length != priiloader_app_size )
		{
			ISFS_Close(fd);
			nand_copy(copy_app,original_app,1);
			ISFS_Delete(copy_app);
			abort("Written Priiloader app isn't the correct size.System Menu Recovered");
		}
		else
		{
			gprintf("Size Check Success\n");
			printf("Size Check Success!\n");
		}
	}
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
		nand_copy(copy_app,original_app,1);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure! MemAlign Failure of AppData\n");
	}
	ISFS_Close(fd);
	if (ret < 0)
	{
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
		nand_copy(copy_app,original_app,1);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure! read of priiloader app returned %u\n",ret);
	}
	if(CompareChecksum((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
		printf("Checksum comparison Success!\n");
	else
	{
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
		nand_copy(copy_app,original_app,1);
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
	gprintf("Priiloader Update Complete\n");
	printf("Done!\n\n");
	return 1;
}
s8 RemovePriiloader ( void )
{
	s32 fd = 0;
	//ISFS_Delete(original_app);
	printf("Restoring System menu app...");
	s32 ret = nand_copy(copy_app,original_app,1);
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
			//ISFS_Close(fd);
			ISFS_CreateFile(original_app,0,3,3,3);
			fd = ISFS_Open(original_app,ISFS_OPEN_RW);
			ISFS_Write(fd,priiloader_app,priiloader_app_size);
			ISFS_Close(fd);
			abort("\nUnable to restore the system menu! (ret = %d)",ret);
		}
	}
	ret = ISFS_Delete(copy_app);
	printf("Done!\n");
	return 1;
}
s8 HaveNandPermissions( void )
{
	gprintf("testing permissions...");
	s8 temp = ISFS_Open("/title/00000001/00000002/content/title.tmd",ISFS_OPEN_RW);
	if ( temp < 0 )
	{
		gprintf("no permissions.error %d\n",temp);
		return false;
	}
	else
	{
		ISFS_Close(temp);
		gprintf("and bingo was his name-O\n");
		return true;
	}
}
int main(int argc, char **argv)
{
	s8 ios_patched = 0;
	s8 patches_applied = 0;
	u8 Cios_Detected = 0;


	VIDEO_Init();
	if(vmode == NULL)
		vmode = VIDEO_GetPreferredMode(NULL);
	//adjust overscan a bit
	/*if( vmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
	{
		//the correct one would be * 0.035 to be sure to get on the Action safe of the screen.
		//GX_AdjustForOverscan(vmode, vmode, 0, vmode->viWidth * 0.026 ); 
		GX_AdjustForOverscan(vmode, vmode, vmode->viHeight * 0.016, 0 ); 
	}*/
	//GX_AdjustForOverscan(vmode, vmode, 16,16 ); 
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	console_init(xfb,32,20,vmode->fbWidth,vmode->xfbHeight,vmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	VIDEO_ClearFrameBuffer( vmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	CheckForGecko();
	gprintf("resolution is %dx%d\n",vmode->viWidth,vmode->viHeight);
	printf("\x1b[2J");
	fflush(stdout);

	//reload ios so that IF the user started this with AHBPROT we lose everything from HBC. also, IOS36 is the most patched ios :')
	IOS_ReloadIOS(36);
	
	printf("IOS %d rev %d\n\n",IOS_GetVersion(),IOS_GetRevision());
	printf("\tPriiloader rev %d (preloader v0.30 mod) Installation / Removal Tool\n\n\n\n\t",SVN_REV);
	printf("\t\t\t\t\tPLEASE READ THIS CAREFULLY\n\n\t");
	printf("\t\tTHIS PROGRAM/TOOL COMES WITHOUT ANY WARRANTIES!\n\t");
	printf("\t\tYOU ACCEPT THAT YOU INSTALL THIS AT YOUR OWN RISK\n\n\n\t");
	printf("THE AUTHOR(S) CANNOT BE HELD LIABLE FOR ANY DAMAGE IT MIGHT CAUSE\n\n\t");
	printf("\tIF YOU DO NOT AGREE WITH THESE TERMS TURN YOUR WII OFF\n\n\n\n\t");
	printf("\t\t\t\t\tPlease wait while we init...");

	if (ISFS_Initialize() < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort("\n\n\n\nFailed to init ISFS");
	}
	gprintf("Got ROOT!\n");
	if (HaveNandPermissions())
	{
		ios_patched++;
	}
	else
	{
		printf("\x1b[2J");
		fflush(stdout);
		printf("Failed to retrieve nand permissions from nand!IOS 36 isn't patched!\n");
		sleepx(5);
		exit(0);
	}

	WPAD_Init();
	PAD_Init();
	sleepx(6);

	printf("\r\t\t\t    Press (+/A) to install or update Priiloader\n\t");
	printf("\tPress (-/Y) to remove Priiloader and restore system menu\n\t");
	if( (ios_patched < 1) && (IOS_GetVersion() != 36 ) )
		printf("  Hold Down (B) with any above options to use IOS36(recommended)\n\t");
	printf("\t Press (HOME/Start) to chicken out and quit the installer!\n\n\t");
	printf("\t\t\t\t\tEnjoy! DacoTaco & BadUncle\n");

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
			if (pHeld & WPAD_BUTTON_B || GCpHeld & PAD_BUTTON_B )
			{
				if((ios_patched < 1) && (IOS_GetVersion() != 36 ) )
				{
					WPAD_Shutdown();
    				IOS_ReloadIOS(36);
    				WPAD_Init();
					if(HaveNandPermissions())
						ios_patched++;
					else
					{
						printf("\x1b[2J");
						fflush(stdout);
						printf("Failed to retrieve nand permissions from nand!ios 36 isn't patched!\n");
						sleepx(5);
						exit(0);
					}
				}
        	}
			static u32 tmp_ikey ATTRIBUTE_ALIGN(32);
			static u32 tmd_size ATTRIBUTE_ALIGN(32);
			static u64 title_id ATTRIBUTE_ALIGN(32)=0x0000000100000002LL;
			//static u64 title_id ATTRIBUTE_ALIGN(32)=0x0001000147534654LL;

			s32 fd,fs;
			u32 id = 0;
			s32 ret = 0;

			printf("\x1b[2J");
			fflush(stdout);
			printf("IOS %d rev %d\n\n\n",IOS_GetVersion(),IOS_GetRevision());
			//read TMD so we can get the main booting dol
			fs = ES_GetStoredTMDSize(title_id,&tmd_size);
			if (fs < 0)
			{
				abort("Unable to get stored tmd size");
			}

			static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
			signed_blob *TMD = (signed_blob *)tmd_buf;
			fs = ES_GetStoredTMD(title_id,TMD,tmd_size);
			if (fs < 0)
			{
				abort("Unable to get stored tmd");
			}

			tmd *rTMD = (tmd*)SIGNATURE_PAYLOAD(TMD);
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
				abort("Unable to retrieve title booting app");
			}

			//sprintf(original_app, "/title/00000001/00000002/content/%08x.app",id);
			//sprintf(copy_app, "/title/00000001/00000002/content/%08x.app",id);
			sprintf(original_app, "/title/%08x/%08x/content/%08x.app",(u32)(title_id >> 32),(u32)title_id,id);
			sprintf(copy_app, "/title/%08x/%08x/content/%08x.app",(u32)(title_id >> 32),(u32)title_id,id);
			copy_app[33] = '1';
			if (pDown & WPAD_BUTTON_PLUS || GCpDown & PAD_BUTTON_A)
			{
#ifdef BETA
				printf("\x1b[%u;%dm", 33, 1);
				printf("\nWARNING : ");
				printf("\x1b[%u;%dm", 37, 1);
				printf("this is a beta version. are you SURE you want to install this?\nA to confirm, Home/Start to abort\n");
				sleepx(1);
				if(!UserYesNoStop())
				{
					abort("user command");
				}
#endif
				//install or update
				fstats * status;
				bool _Copy_Sysmenu = true;
				printf("Checking for Priiloader...\n");				
				//check copy app
				gprintf("checking for SystemMenu Dol\n");
				//TODO : redo using the code from Luna + size check
				fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
				if (fd < 0)
				{
					printf("Priiloader not found.\nInstalling Priiloader...\n");
					_Copy_Sysmenu = true;
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
						ISFS_Close(fd);
						if(UserYesNoStop())
						{
							printf("Ignoring Priiloader Installation...\n\n");
							_Copy_Sysmenu = true;
						}
						else
						{
							printf("Using Current Priiloader Installation...\n\n");
							_Copy_Sysmenu = false;
						}
					}
					else
					{
						ISFS_Close(fd);
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
								_Copy_Sysmenu = true;
								break;
							}
							else
							{
								printf("Using Current Priiloader \"Installation\"...\n\nUpdating Priiloader...\n\n\n");
								_Copy_Sysmenu = false;
							}
						}
						else
						{
							_Copy_Sysmenu = false;
							printf("Priiloader installation found\nUpdating Priiloader...\n\n\n");
						}
					}
					free(status);
					status = NULL;
				}
				CopyTicket(title_id);
				ret = PatchTMD(title_id,0);
				if(ret < 0)
				{
					abort("\npatching TMD error %d!\n",ret);
				}
				if(_Copy_Sysmenu)
					Copy_SysMenu();
				WritePriiloader();
				if(!_Copy_Sysmenu)
				{
					printf("deleting extra priiloader files...\n");
					Delete_Priiloader_Files(1);
					printf("\n\nUpdate done, exiting to loader... waiting 5s...\n");
				}
				else if(_Copy_Sysmenu)
				{
					//currently mode 0 means no extra files to delete so why do the printf?
					printf("Attempting to delete leftover files...\n");
					Delete_Priiloader_Files(0);
					printf("Install done, exiting to loader... waiting 5s...\n");
				}
					
				ISFS_Deinitialize();
				sleepx(5);
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
					ISFS_Close(fd);
					printf("Priiloader installation found.\n\n");//Removing Priiloader app...");
					RemovePriiloader();
					PatchTMD(title_id,1);
					printf("Deleting extra Priiloader files...\n");
					Delete_Priiloader_Files(2);
					printf("Done!\n\n");
					printf("Removal done, exiting to loader... waiting 5s...\n");
					ISFS_Deinitialize();
					sleepx(5);
					exit(0);
				}
			}
		}
		else if (pDown & WPAD_BUTTON_HOME || GCpDown & PAD_BUTTON_START)
		{
			printf("\x1b[2J");
			fflush(stdout);
			printf("Sorry, but our princess is in another castle... goodbye!");
			gprintf("aboring the fun\n");
			sleepx(5);
			exit(0);
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
