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
#define TITLE_UPPER(x) (u32)(x >> 32)
#define TITLE_LOWER(x) (u32)(x & 0xFFFFFFFF)
#define ALIGN32(x) (((x) + 31) & ~31)


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
#include <ogc/ios.h>
#include <ogc/usbgecko.h>

#ifdef BETA
#include <debug.h>
#endif
#include <ogc/es.h>

//project includes
#include "sha1.h"

//rev version
#include "../../Shared/svnrev.h"

// Bin Files

// Priiloader Application
#include "priiloader_app.h"
#include "certs_bin.h"
#include "su_tmd.h"
#include "su_tik.h"

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;
bool GeckoFound = false;

char original_app[ISFS_MAXPATH];
char copy_app[ISFS_MAXPATH];

char TMD_Path[ISFS_MAXPATH];
char TMD_Path2[ISFS_MAXPATH];

u32 tmd_size;
static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
static signed_blob *mTMD;
static tmd *rTMD;
static u64 title_id = 0x0000000100000002LL;
//static u64 title_id = 0x0001000147534654LL;
typedef struct
{
	u32 owner;
	u16 group;
	u8 attributes;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} ATTRIBUTE_PACKED Nand_Permissions;

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
	return;
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
	time_t start;
	time_t end;
	time(&start);
	//i hate while loops. but its safer then sleep this way
	while(difftime(end, start) < seconds)
	{
		time(&end);
	}
	return;
}
void abort(const char* msg, ...)
{
	va_list args;
	char text[4096];
	va_start( args, msg );
	strcpy( text + vsprintf( text,msg,args ),""); 
	va_end( args );
	printf("\x1b[%u;%dm", 36, 1);
	printf("%s, aborting mission...\n", text);
	gprintf("%s, aborting mission...\n", text);
	//ISFS_Deinitialize();
	sleepx(5);
	exit(0);
	return;
}
bool CompareSha1Hash(u8 *Data1,u32 Data1_Size,u8 *Data2,u32 Data2_Size)
{
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
	u32 FileHash_D1[5];
	u32 FileHash_D2[5];
	memset(FileHash_D2,0xFF,5); // set FileHash_D2 to different value so that if something goes wrong; the check fails
	SHA1 sha; // SHA-1 class
	sha.Reset();
	sha.Input(Data1,Data1_Size);
	if (!sha.Result(FileHash_D1))
	{
		gprintf("could not compute Hash of D1!\n");
		return false;
	}
	sha.Reset();
	sha.Input(Data2,Data2_Size);
	if (!sha.Result(FileHash_D2))
	{
		gprintf("could not compute Hash of D1!\n");
		return false;
	}
	sha.Reset();
	/*gprintf("sha1 original(0x%x) : %x %x %x %x %x\nsha1 written(0x%x) : %x %x %x %x %x\n",
																		  Data1_Size,FileHash_D1[0],FileHash_D1[1],FileHash_D1[2],FileHash_D1[3],FileHash_D1[4],
																		  Data2_Size,FileHash_D2[0],FileHash_D2[1],FileHash_D2[2],FileHash_D2[3],FileHash_D2[4]);*/
	if (!memcmp(FileHash_D1,FileHash_D2,sizeof(FileHash_D1)))
	{
		return true;
	}
	else
	{		
		gprintf("SHA1 check failed!\n");
	}
	return false;
}
s32 nand_copy(const char *destination,u8* Buf_To_Write_to_Copy, u32 buf_size,Nand_Permissions src_perm)
{
	if( Buf_To_Write_to_Copy == NULL || buf_size < 1 )
	{
		return -1;
	}
	s32 ret, dest_handler;
	gprintf("owner %d group %d attributes %X perm:%X-%X-%X\n", src_perm.owner, (u32)src_perm.group, (u32)src_perm.attributes, (u32)src_perm.ownerperm, (u32)src_perm.groupperm, (u32)src_perm.otherperm);
	
	//extract filename from destination
	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = NULL;
	ptemp = strstr(destination,"/");
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}

	//create temp path
	memset(temp_dest,0,ISFS_MAXPATH);
	sprintf(temp_dest,"/tmp/%s",ptemp);
	ISFS_Delete(temp_dest);

	//and go for it
	ret = ISFS_CreateFile(temp_dest,src_perm.attributes,src_perm.ownerperm,src_perm.groupperm,src_perm.otherperm);
	if (ret != ISFS_OK) 
	{
		printf("Failed to create file %s. ret = %d\n",temp_dest,ret);
		gprintf("Failed to create file %s. ret = %d\n",temp_dest,ret);
		return ret;
	}
	dest_handler = ISFS_Open(temp_dest,ISFS_OPEN_RW);
    if (dest_handler < 0)
	{
		gprintf("failed to open destination : %s\n",temp_dest);
		ISFS_Delete(temp_dest);
		return dest_handler;
	}

	ret = ISFS_Write(dest_handler,Buf_To_Write_to_Copy,buf_size);
	if (ret < 0)
	{
		gprintf("failed to write destination : %s\n",temp_dest);
		ISFS_Close(dest_handler);
		ISFS_Delete(temp_dest);
		return ret;
	}
	ISFS_Close(dest_handler);
	s32 temp = 0;
	u8 *Data2 = NULL;
	STACK_ALIGN(fstats,D2stat,sizeof(fstats),32);
	/*if (D2stat == NULL)
	{
		temp = -1;
		goto free_and_Return;
	}*/
	dest_handler = ISFS_Open(temp_dest,ISFS_OPEN_RW);
	if(dest_handler < 0)
	{
		gprintf("temp_dest open error %d\n",dest_handler);
		temp = -2;
		goto free_and_Return;
	}
	temp = ISFS_GetFileStats(dest_handler,D2stat);
	if(temp < 0)
	{
		goto free_and_Return;
	}
	Data2 = (u8*)memalign(32,ALIGN32(D2stat->file_length));
	if (Data2 == NULL)
	{
		temp = -3;
		goto free_and_Return;
	}
	if( ISFS_Read(dest_handler,Data2,D2stat->file_length) > 0 )
	{
			if( !CompareSha1Hash(Buf_To_Write_to_Copy,buf_size,Data2,D2stat->file_length))
			{
				temp = -4;
				goto free_and_Return;
			}
	}
	else
	{
		temp = -5;
		goto free_and_Return;
	}
	if(Data2)
	{
		free(Data2);
		Data2 = NULL;
	}
	ISFS_Close(dest_handler);
	//so it was written to /tmp correctly. lets call ISFS_Rename and compare AGAIN
	ISFS_Delete(destination);
	ret = ISFS_Rename(temp_dest,destination);
	if(ret < 0 )
	{
		gprintf("nand_copy(buf) : rename returned %d\n",ret);
		temp = -6;
		goto free_and_Return;
	}
free_and_Return:
	if(Data2 != NULL)
	{
		free(Data2);
		Data2 = NULL;
	}
	ISFS_Close(dest_handler);
	if (temp < 0)
	{
		gprintf("temp %d\n",temp);
		//ISFS_Delete(destination);
		return -80;
	}
    return 1;
}
s32 nand_copy(const char *source, const char *destination,Nand_Permissions src_perm)
{
	//variables
	u8 *buffer = NULL;
	STACK_ALIGN(fstats,status,sizeof(fstats),32);
	s32 file_handler, ret;
	u32 FileHash_D1[5];
	memset(FileHash_D1,0,5);
	u32 FileHash_D2[5];
	memset(FileHash_D2,0xFF,5); //place different data in D2 so that if something goes wrong later on, the comparison will fail
	SHA1 sha;
	sha.Reset();

	//variables - temp dir & SHA1 check
	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = NULL;
	u8 temp = 0;

	//get temp filename
	ptemp = strstr(destination,"/");
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}
	memset(temp_dest,0,ISFS_MAXPATH);
	sprintf(temp_dest,"/tmp/%s",ptemp);

	//get data into pointer from original file
	file_handler = ISFS_Open(source,ISFS_OPEN_READ);
    if (file_handler < 0)
	{
		gprintf("failed to open source : %s\n",source);
		return file_handler;
	}
    
	ret = ISFS_GetFileStats(file_handler,status);
    if (ret < 0)
    {
		printf("\n\nFailed to get information about %s!\n",source);
		sleepx(2);
        ISFS_Close(file_handler);
        return ret;
    }

    buffer = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (buffer == NULL)
	{
		gprintf("buffer failed to align\n");
		sleepx(2);
        ISFS_Close(file_handler);
		return 0;
	}
	memset(buffer,0,status->file_length);
    ret = ISFS_Read(file_handler,buffer,status->file_length);
    if (ret < 0)
    {
		printf("\n\nFailed to Read Data from %s!\n",source);
		sleepx(2);
		ISFS_Close(file_handler);
        free(buffer);
		buffer = NULL;
        return ret;
    }
	ISFS_Close(file_handler);
	//everything read into buffer. generate SHA1 hash of the buffer
	sha.Input(buffer,status->file_length);
	if (!sha.Result(FileHash_D1))
	{
		gprintf("could not compute Hash of D1!\n");
		free(buffer);
		buffer = NULL;
		return -80;
	}
	sha.Reset();
	//done, lets create temp file and write :')

	ISFS_Delete(temp_dest);
	ISFS_CreateFile(temp_dest,src_perm.attributes,src_perm.ownerperm,src_perm.groupperm,src_perm.otherperm);
	//created. opening it...
	file_handler = ISFS_Open(temp_dest,ISFS_OPEN_RW);
	if (file_handler < 0)
	{
		gprintf("failed to open destination : %s\n",temp_dest);
		ISFS_Delete(temp_dest);
		free(buffer);
		buffer = NULL;
		return file_handler;
	}
	ret = ISFS_Write(file_handler,buffer,status->file_length);
    if (ret < 0)
    {
		gprintf("failed to write destination : %s\n",destination);
        ISFS_Close(file_handler);
        ISFS_Delete(temp_dest);
        free(buffer);
		buffer = NULL;
        return ret;
    }
	//write done. reopen file for reading and compare SHA1 hash
	ISFS_Close(file_handler);
	free(buffer);
	buffer = NULL;
	memset(status,0,sizeof(fstats));
	file_handler = ISFS_Open(temp_dest,ISFS_OPEN_READ);
	if(!file_handler)
	{
		temp = -1;
		goto free_and_Return;
	}
	ret = ISFS_GetFileStats(file_handler,status);
    if (ret < 0)
    {
        ISFS_Close(file_handler);
		temp = -2;
		goto free_and_Return;
    }
	buffer = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (buffer == NULL)
	{
		gprintf("buffer failed to align\n");
        ISFS_Close(file_handler);
		temp = -3;
		goto free_and_Return;
	}
	memset(buffer,0,status->file_length);
	if( ISFS_Read(file_handler,buffer,status->file_length) < 0 )
	{
		temp = -4;
		goto free_and_Return;
	}
	ISFS_Close(file_handler);
	sha.Reset();
	sha.Input(buffer,status->file_length);
	free(buffer);
	buffer = NULL;
	if (!sha.Result(FileHash_D2))
	{
		gprintf("could not compute Hash of D2!\n");
		return -80;
	}
	sha.Reset();
	/*gprintf("sha1 original : %x %x %x %x %x\nsha1 written : %x %x %x %x %x\n",
																		  FileHash_D1[0],FileHash_D1[1],FileHash_D1[2],FileHash_D1[3],FileHash_D1[4],
																		  FileHash_D2[0],FileHash_D2[1],FileHash_D2[2],FileHash_D2[3],FileHash_D2[4]);*/
	if (!memcmp(FileHash_D1,FileHash_D2,sizeof(FileHash_D1)))
	{
		gprintf("nand_copy : SHA1 hash success\n");
		ISFS_Delete(destination);
		ret = ISFS_Rename(temp_dest,destination);
		gprintf("ISFS_Rename ret %d\n",ret);
		if ( ret < 0)
			temp = -5;
		goto free_and_Return;
	}
	else
	{		
		temp = -6;
		goto free_and_Return;
	}
free_and_Return:
	if(buffer)
	{
		free(buffer);
		buffer = NULL;
	}
	if (temp < 0)
	{
		gprintf("nand_copy temp %d fail o.o;\n",temp);
		ISFS_Delete(temp_dest);
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
void proccess_delete_ret( s32 ret )
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
	return;
}
void Delete_Priiloader_Files( u8 mode )
{
	bool settings = false;
	bool hacks = false;
	bool password = false;
	bool main_bin = false;
	bool ticket = false;
	switch(mode)
	{
		case 2: //remove
			main_bin = true;
			ticket = true;
		case 0: //install
			hacks = true;
		case 1: //update
			settings = true;
			password = true;
		default:
			break;
	}
	s32 ret = 0;
	static char file_path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	memset(file_path,0,ISFS_MAXPATH);
	if(password)
	{
		sprintf(file_path, "/title/%08x/%08x/data/password.txt",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("password.txt : %d\n",ret);
		printf("password file : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	if(settings)
	{
		sprintf(file_path, "/title/%08x/%08x/data/loader.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("loader.ini : %d\n",ret);
		printf("Settings file : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	//its best we delete that ticket but its completely useless and will only get in our 
	//way when installing again later...
	if(ticket)
	{
		sprintf(file_path, "/title/%08x/%08x/content/ticket",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("ticket : %d\n",ret);
		printf("Ticket : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	if(hacks)
	{
		sprintf(file_path, "/title/%08x/%08x/data/hacks_s.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks_s.ini : %d\n",ret);
		printf("Hacks_s.ini : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);

		sprintf(file_path, "/title/%08x/%08x/data/hacks.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks.ini : %d\n",ret);
		printf("Hacks.ini : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);

		sprintf(file_path, "/title/%08x/%08x/data/hacksh_s.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacksh_s.ini : %d\n",ret);
		printf("Hacksh_s.ini : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);

		sprintf(file_path, "/title/%08x/%08x/data/hackshas.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks_hash : %d\n",ret);
		printf("Hacks_hash : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);

	}
	if(main_bin)
	{
		sprintf(file_path, "/title/%08x/%08x/data/main.nfo",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("main.nfo : %d\n",ret);
		sprintf(file_path, "/title/%08x/%08x/data/main.bin",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("main.bin : %d\n",ret);
		printf("main.bin : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	return;
}
s8 PatchTMD( u8 delete_mode )
{
	Nand_Permissions Perm;
	memset(&Perm,0,sizeof(Nand_Permissions));
	u8 SaveTmd = 0;
	SHA1 sha; // SHA-1 class
	sha.Reset();
	u32 FileHash[5];
	u8 *TMD_chk = NULL;
	s32 fd = 0;
	s32 r = 0;
#ifdef _DEBUG
	gprintf("Path : %s\n",TMD_Path);
#endif
	r = ISFS_GetAttr(TMD_Path, &Perm.owner, &Perm.group, &Perm.attributes, &Perm.ownerperm, &Perm.groupperm, &Perm.otherperm);
	if(r < 0 )
	{
		//attribute getting failed. returning to default
		printf("\x1b[%u;%dm", 33, 1);
		printf("\nWARNING : failed to get file's permissions. using defaults\n");
		printf("\x1b[%u;%dm", 37, 1);
		gprintf("permission failure on desination! error %d\n",r);
		gprintf("writing with max permissions\n");
		Perm.ownerperm = 3;
		Perm.groupperm = 3;
		Perm.otherperm = 0;
	}
	else
	{
		gprintf("r %d owner %d group %d attributes %X perm:%X-%X-%X\n", r, Perm.owner, Perm.group, Perm.attributes, Perm.ownerperm, Perm.groupperm, Perm.otherperm);
	}
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

	fd = ISFS_Open(TMD_Path2,ISFS_OPEN_READ);
	if(fd < 0)
	{
		if(delete_mode)
		{
			printf("TMD backup not found. leaving TMD alone...\n");
			return 1;
		}
		else
		{
			//got to make tmd copy :)
			gprintf("Making tmd backup...\n");
			r = nand_copy(TMD_Path2,tmd_buf,tmd_size,Perm);
			if ( r < 0)
			{
				gprintf("Failure making TMD backup.error %d\n",r);
				printf("TMD backup/Patching Failure : error %d",r);
				goto _return;
			}
		}
		fd = 0;
	}
	ISFS_Close(fd);
	gprintf("TMD backup found\n");
	//not so sure why we'd want to delete the tmd modification but ok...
	if(delete_mode)
	{
		if ( nand_copy(TMD_Path2,TMD_Path,Perm) < 0)
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
					nand_copy(TMD_Path,TMD_Path2,Perm);
					abort("TMD restoring failure.");
				}		
			}
			else
			{
				printf("\x1b[%u;%dm", 33, 1);
				printf("UNABLE TO RESTORE THE SYSTEM MENU TMD!!!\n\nTHIS COULD BRICK THE WII SO PLEASE REINSTALL SYSTEM MENU\nWHEN RETURNING TO THE HOMEBREW CHANNEL!!!\n\n");
				printf("\x1b[%u;%dm", 37, 1);
				printf("press A to return to the homebrew channel\n");
				nand_copy(TMD_Path,TMD_Path2,Perm);
				UserYesNoStop();
				exit(0);
			}
		}
		else
			ISFS_Delete(TMD_Path2);
		return 1;
	}
	else
	{
		//check if version is the same
		STACK_ALIGN(fstats,TMDb_status,sizeof(fstats),32);
		static u8 tmdb_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
		static signed_blob *mbTMD;
		static tmd* pbTMD;

		fd = ISFS_Open(TMD_Path2,ISFS_OPEN_READ);
		if (fd < 0)
		{
			gprintf("TMD bCheck : failed to open source : %s\n",TMD_Path2);
			goto patch_tmd;
		}
		r = ISFS_GetFileStats(fd,TMDb_status);
		if (r < 0)
		{
			gprintf("TMD bCheck : Failed to get information about %s!\n",TMD_Path2);
			ISFS_Close(fd);
			goto patch_tmd;
		}
		memset(tmdb_buf,0,MAX_SIGNED_TMD_SIZE);

		r = ISFS_Read(fd,tmdb_buf,TMDb_status->file_length);
		if (r < 0)
		{
			gprintf("TMD bCheck : Failed to Read Data from %s!\n",TMD_Path2);
			ISFS_Close(fd);
			goto patch_tmd;
		}
		ISFS_Close(fd);
		mbTMD = (signed_blob *)tmdb_buf;
		pbTMD = (tmd*)SIGNATURE_PAYLOAD(mbTMD);
		if (pbTMD->title_version != rTMD->title_version)
		{
			gprintf("TMD bCheck : backup TMD version mismatch: %d & %d\n",rTMD->title_version,pbTMD->title_version);
			//got to make tmd copy :)
			r = nand_copy(TMD_Path2,tmd_buf,tmd_size,Perm);
			if ( r < 0)
			{
				gprintf("TMD bCheck : Failure making TMD backup.error %d\n",r);
				printf("TMD backup/Patching Failure : error %d",r);
				goto _return;
			}
		}
		else
			gprintf("TMD bCheck : backup TMD is correct\n");
		r = 0;
	}
patch_tmd:
	gprintf("detected access rights : 0x%08X\n",rTMD->access_rights);
	if(rTMD->access_rights == 0x03)
	{
		gprintf("no AHBPROT modification needed\n");
	}
	else
	{
		rTMD->access_rights = 0x03;
		DCFlushRange(rTMD,sizeof(tmd));
		if(rTMD->access_rights != 0x03)
		{
			gprintf("rights change failure.\n");
			goto _return;
		}
		SaveTmd++;
	}
	gprintf("checking Boot app SHA1 hash...\n");
	gprintf("bootapp ( %d ) SHA1 hash = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",rTMD->boot_index
		,rTMD->contents[rTMD->boot_index].hash[0],rTMD->contents[rTMD->boot_index].hash[1],rTMD->contents[rTMD->boot_index].hash[2],rTMD->contents[rTMD->boot_index].hash[3]
		,rTMD->contents[rTMD->boot_index].hash[4],rTMD->contents[rTMD->boot_index].hash[5],rTMD->contents[rTMD->boot_index].hash[6],rTMD->contents[rTMD->boot_index].hash[7]
		,rTMD->contents[rTMD->boot_index].hash[8],rTMD->contents[rTMD->boot_index].hash[9],rTMD->contents[rTMD->boot_index].hash[10],rTMD->contents[rTMD->boot_index].hash[11]
		,rTMD->contents[rTMD->boot_index].hash[12],rTMD->contents[rTMD->boot_index].hash[13],rTMD->contents[rTMD->boot_index].hash[14],rTMD->contents[rTMD->boot_index].hash[15]
		,rTMD->contents[rTMD->boot_index].hash[16],rTMD->contents[rTMD->boot_index].hash[17],rTMD->contents[rTMD->boot_index].hash[18],rTMD->contents[rTMD->boot_index].hash[19]);
	gprintf("generated priiloader SHA1 : ");
	sha.Reset();
	sha.Input(priiloader_app,priiloader_app_size);
	if (!sha.Result(FileHash))
	{
		gprintf("could not compute Hash of Priiloader!\n");
		r = -1;
		goto _return;
	}
	if (!memcmp(rTMD->contents[rTMD->boot_index].hash, FileHash, sizeof(FileHash) ) )
	{
		gprintf("no SHA hash change needed\n");
	}
	else
	{
		memcpy(rTMD->contents[rTMD->boot_index].hash,FileHash,sizeof(FileHash));
		gprintf("%08x %08x %08x %08x %08x\n",FileHash[0],FileHash[1],FileHash[2],FileHash[3],FileHash[4]);
		DCFlushRange(rTMD,sizeof(tmd));
		SaveTmd++;
	}
	if(SaveTmd > 0)
	{
		gprintf("saving TMD\n");
		r = nand_copy(TMD_Path,tmd_buf,tmd_size,Perm);
		if(r < 0 )	
		{
			gprintf("nand_copy failure. error %d\n",r);
			if(r == -80)
				goto _checkreturn;
			else
				goto _return;
		}
	}
	else
	{
		gprintf("no TMD mod's needed\n");
		printf("no TMD modifications needed\n");
		goto _return;
	}
	printf("Done\n");
_return:
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
		printf("\nWARNING!!\nInstaller couldn't Patch the system menu TMD.\n");
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
				nand_copy(TMD_Path2,TMD_Path,Perm);
			}
			abort("TMD failure");
			return -1;
		}		
		else
		{
			nand_copy(TMD_Path2,TMD_Path,Perm);
			printf("\nDone!\n");
			return 1;
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
		nand_copy(TMD_Path2,TMD_Path,Perm);
		abort("TMD Patch failure\n");
	}		
	else
	{
		printf("\nDone!\n");
	}
	return -80;
}
s8 CopyTicket ( )
{
	s32 fd = 0;
	char TIK_Path_dest[64];
	char TIK_Path_org[64];
	memset(TIK_Path_dest,0,64);
	memset(TIK_Path_org,0,64);
	sprintf(TIK_Path_dest, "/title/%08x/%08x/content/ticket",TITLE_UPPER(title_id),TITLE_LOWER(title_id));
	sprintf(TIK_Path_org, "/ticket/%08x/%08x.tik",TITLE_UPPER(title_id),TITLE_LOWER(title_id));
	gprintf("Checking for copy ticket...\n");
	fd = ISFS_Open(TIK_Path_dest,ISFS_OPEN_READ);
	if (fd >= 0)
	{
		ISFS_Close(fd);
		printf("Skipping copy of system menu ticket...\n");
		return 1;
	}
	switch(fd)
	{
		case ISFS_EINVAL:
			abort("Unable to read ticket.path is wrong/too long or ISFS isn't init yet?");
			break;
		case ISFS_ENOMEM:
			abort("Unable to read ticket.(Out of memory)");
			break;
		case -102:
			abort("Unauthorised to get ticket. is ios%d trucha signed?",IOS_GetVersion());
			break;
		default:
			if(fd < 0)
				abort("Unable to read ticket. error %d. ",fd);
		case -106:
			printf("Priiloader system menu ticket not found.\n\tTrying to read original ticket...\n");
			break;
	}
	fd = ISFS_Open(TIK_Path_org,ISFS_OPEN_READ);
	//"/ticket/00000001/00000002.tik" -> original path which should be there on every wii.
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
	ISFS_Close(fd);
	printf("Copying system menu ticket...");
	Nand_Permissions TikPerm;
	memset(&TikPerm,0,sizeof(Nand_Permissions));
	TikPerm.otherperm = 3;
	TikPerm.groupperm = 3;
	TikPerm.ownerperm = 3;
	if (nand_copy(TIK_Path_org,TIK_Path_dest,TikPerm) < 0)
	{
		abort("Unable to copy the system menu ticket");
	}
	printf("Done!\n");
	return 1;
}
bool CheckForPriiloader( void )
{
	bool ret = false;
	s32 fd = 0;
	printf("Checking for Priiloader...\n");				
	gprintf("checking for SystemMenu Dol\n");
	fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
	if (fd < 0)
	{
		printf("Priiloader not found : Installing Priiloader...\n\n");
		ret = false;
	}
	else
	{
		ISFS_Close(fd);
		printf("Priiloader installation found : Updating Priiloader...\n\n");
		ret = true;
	}
	return ret;
}
s8 WritePriiloader( bool priiloader_found )
{
	s32 ret = 0;
	s32 fd = 0;
	Nand_Permissions SysPerm;
	if(priiloader_found == false)
	{
		memset(&SysPerm,0,sizeof(Nand_Permissions));
		SysPerm.otherperm = 3;
		SysPerm.groupperm = 3;
		SysPerm.ownerperm = 3;
		//system menu coping
		printf("Moving System Menu app...");
		ret = nand_copy(original_app,copy_app,SysPerm);
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
	}
	ret = 0;
	//sys_menu app moved. lets write priiloader
	STACK_ALIGN(fstats,status,sizeof(fstats),32);
	memset(&SysPerm,0,sizeof(Nand_Permissions));
	SysPerm.otherperm = 3;
	SysPerm.groupperm = 3;
	SysPerm.ownerperm = 3;

	printf("Writing Priiloader app...");
	gprintf("Writing Priiloader\n");

	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = NULL;
	ptemp = strstr(original_app,"/");
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}
	memset(temp_dest,0,ISFS_MAXPATH);
	sprintf(temp_dest,"/tmp/%s",ptemp);
	ISFS_Delete(temp_dest);
	ret = ISFS_CreateFile(temp_dest,SysPerm.attributes,SysPerm.ownerperm,SysPerm.groupperm,SysPerm.otherperm);

	fd = ISFS_Open(temp_dest,ISFS_OPEN_RW);
	if (fd < 0)
	{
		gprintf("error %d\n",fd);
		abort("\nFailed to open file for Priiloader writing");
	}
	ret = ISFS_Write(fd,priiloader_app,priiloader_app_size);
	if (ret < 0 ) //check if the app was writen correctly				
	{
		ISFS_Close(fd);
		ISFS_Delete(copy_app);
		ISFS_Delete(temp_dest);
		gprintf("Write failed. ret %d\n",ret);
		abort("\nWrite of Priiloader app failed");
	}
	ISFS_Close(fd);

	//SHA1 check here
	fd = ISFS_Open(temp_dest,ISFS_OPEN_READ);
	if (fd < 0)
	{
		ISFS_Delete(copy_app);
		abort("\nFailed to open file for Priiloader checking");
	}
	if (ISFS_GetFileStats(fd,status) < 0)
	{
		ISFS_Close(fd);
		ISFS_Delete(copy_app);
		abort("Failed to get stats of %s. System Menu Recovered",temp_dest);
	}
	else
	{
		if ( status->file_length != priiloader_app_size )
		{
			ISFS_Close(fd);
			ISFS_Delete(copy_app);
			abort("Written Priiloader app isn't the correct size.System Menu Recovered");
		}
		else
		{
			gprintf("Size Check Success\n");
			printf("Size Check Success!\n");
		}
	}
	u8 *AppData = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (AppData)
		ret = ISFS_Read(fd,AppData,status->file_length);
	else
	{
		ISFS_Close(fd);
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
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure! read of priiloader app returned %u\n",ret);
	}
	if(CompareSha1Hash((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
		printf("Checksum comparison Success!\n");
	else
	{
		if (AppData)
		{
			free(AppData);
			AppData = NULL;
		}
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure!\n");
	}
	if (AppData)
	{
		free(AppData);
		AppData = NULL;
	}
	// rename and do a final SHA1 chezck
	ISFS_Delete(original_app);
	ret = ISFS_Rename(temp_dest,original_app);
	if(ret < 0 )
	{
		gprintf("WritePriiloader : rename returned %d\n",ret);
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("\nFailed to Write Priiloader : error Ren %d",ret);
	}
	printf("Done!!\n");
	gprintf("Wrote Priiloader App.Checking Installation\n");
	printf("\nChecking Priiloader Installation...\n");
	memset(status,0,sizeof(fstats));
	fd = ISFS_Open(original_app,ISFS_OPEN_READ);
	if (fd < 0)
	{
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("\nFailed to open file for Priiloader checking");
	}
	if (ISFS_GetFileStats(fd,status) < 0)
	{
		ISFS_Close(fd);
		nand_copy(copy_app,original_app,SysPerm);
		abort("Failed to get stats of %s. System Menu Recovered",original_app);
	}
	else
	{
		if ( status->file_length != priiloader_app_size )
		{
			ISFS_Close(fd);
			nand_copy(copy_app,original_app,SysPerm);
			ISFS_Delete(copy_app);
			abort("Written Priiloader app isn't the correct size.System Menu Recovered");
		}
		else
		{
			gprintf("Size Check Success\n");
			printf("Size Check Success!\n");
		}
	}
	AppData = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (AppData != NULL)
		ret = ISFS_Read(fd,AppData,status->file_length);
	else
	{
		ISFS_Close(fd);
		nand_copy(copy_app,original_app,SysPerm);
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
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure! read of priiloader app returned %u\n",ret);
	}
	if(CompareSha1Hash((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
		printf("Checksum comparison Success!\n");
	else
	{
		if (AppData)
		{
			free(AppData);
			AppData = NULL;
		}
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure!\n");
	}
	if (AppData)
	{
		free(AppData);
		AppData = NULL;
	}
	gprintf("Priiloader Update Complete\n");
	printf("Done!\n\n");
	return 1;
}
s8 RemovePriiloader ( void )
{
	s32 fd = 0;
	Nand_Permissions SysPerm;
	memset(&SysPerm,0,sizeof(Nand_Permissions));
	SysPerm.otherperm = 3;
	SysPerm.groupperm = 3;
	SysPerm.ownerperm = 3;
	printf("Restoring System menu app...");
	s32 ret = nand_copy(copy_app,original_app,SysPerm);
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
	s32 temp = ISFS_Open("/title/00000001/00000002/content/title.tmd",ISFS_OPEN_RW);
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
	s32 ret = 0;

	CheckForGecko();
	VIDEO_Init();

	vmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	int x = 20, y = 20, w, h;
	w = vmode->fbWidth - (x * 2);
	h = vmode->xfbHeight - (y + 20);

	// Initialize the console
	CON_InitEx(vmode, x, y, w, h);

	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);

	gprintf("resolution is %dx%d\n",vmode->viWidth,vmode->viHeight);
	printf("\x1b[2J");
	/*sleep(3);
	gprintf("crashing...\n");
	free((void*)0x1);*/
	/*free((void*)0x1);
	free((void*)0x1);
	free((void*)0x1);
	memcpy((void*)0x0,(void*)0x1,5);*/
	//return 0;
	//reload ios so that IF the user started this with AHBPROT we lose everything from HBC. also, IOS36 is the most patched ios :')
	IOS_ReloadIOS(36);
	printf("\nIOS %d rev %d\n\n",IOS_GetVersion(),IOS_GetRevision());
	printf("\tPriiloader rev %d (preloader v0.30 mod) Installation / Removal Tool\n\n\n\n\t",SVN_REV);
	printf("\t\t\t\t\tPLEASE READ THIS CAREFULLY\n\n\t");
	printf("\t\tTHIS PROGRAM/TOOL COMES WITHOUT ANY WARRANTIES!\n\t");
	printf("\t\tYOU ACCEPT THAT YOU INSTALL THIS AT YOUR OWN RISK\n\n\n\t");
	printf("THE AUTHOR(S) CANNOT BE HELD LIABLE FOR ANY DAMAGE IT MIGHT CAUSE\n\n\t");
	printf("\tIF YOU DO NOT AGREE WITH THESE TERMS TURN YOUR WII OFF\n\n\n\n\t");
	printf("\t\t\t\t\tPlease wait while we init...");
	u64 TitleID = 0;
	u32 keyId = 0;
	ret = ES_Identify( (signed_blob*)certs_bin, certs_bin_size, (signed_blob*)su_tmd, su_tmd_size, (signed_blob*)su_tik, su_tik_size, &keyId);
	gprintf("ES_Identify : %d\n",ret);
	if(ret < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort("\n\n\n\ncIOS%d isn't ES_Identify patched : error %d.",IOS_GetVersion(),ret);
	}
	ret = ES_GetTitleID(&TitleID);
	gprintf("identified as = 0x%08X%08X\n",TITLE_UPPER(TitleID),TITLE_LOWER(TitleID));

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
	//read TMD so we can get the main booting dol
	s32 fd = 0;
	u32 id = 0;
	ret = 0;
	memset(TMD_Path,0,64);
	memset(TMD_Path2,0,64);
	sprintf(TMD_Path, "/title/%08x/%08x/content/title.tmd",TITLE_UPPER(title_id),TITLE_LOWER(title_id));
	sprintf(TMD_Path2, "/title/%08x/%08x/content/title_or.tmd",TITLE_UPPER(title_id),TITLE_LOWER(title_id));
	fd = ES_GetStoredTMDSize(title_id,&tmd_size);
	if (fd < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort("Unable to get stored tmd size");
	}
	mTMD = (signed_blob *)tmd_buf;
	fd = ES_GetStoredTMD(title_id,mTMD,tmd_size);
	if (fd < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort("Unable to get stored tmd");
	}
	rTMD = (tmd*)SIGNATURE_PAYLOAD(mTMD);
	for(u8 i=0; i < rTMD->num_contents; ++i)
	{
		if (rTMD->contents[i].index == rTMD->boot_index)
		{
			id = rTMD->contents[i].cid;
			break;
		}
	}
	if (id == 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort("Unable to retrieve title booting app");
	}

	memset(original_app,0,64);
	memset(copy_app,0,64);
	sprintf(original_app, "/title/%08x/%08x/content/%08x.app",TITLE_UPPER(title_id),TITLE_LOWER(title_id),id);
	sprintf(copy_app, "/title/%08x/%08x/content/%08x.app",TITLE_UPPER(title_id),TITLE_LOWER(title_id),id);
	copy_app[33] = '1';
	gprintf("%s &\n%s \n",original_app,copy_app);

	WPAD_Init();
	PAD_Init();
	sleepx(2);

	printf("\r\t\t\t   Press (+/A) to install or update Priiloader\n\t");
	printf("\tPress (-/Y) to remove Priiloader and restore system menu\n\t");
	if( (ios_patched < 1) && (IOS_GetVersion() != 36 ) )
		printf("  Hold Down (B) with any above options to use IOS36\n\t");
	printf("\tPress (HOME/Start) to chicken out and quit the installer!\n\n\t");
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
    	}

		if (pDown & WPAD_BUTTON_PLUS || GCpDown & PAD_BUTTON_A)
		{
			//install Priiloader
			printf("\x1b[2J");
			fflush(stdout);
			printf("IOS %d rev %d\n\n\n",IOS_GetVersion(),IOS_GetRevision());
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
			bool _Prii_Found = CheckForPriiloader();
			CopyTicket();
			WritePriiloader(_Prii_Found);
			ret = PatchTMD(0);
			if(ret < 0)
			{
				abort("\npatching TMD error %d!\n",ret);
			}
			if(_Prii_Found)
			{
				printf("deleting extra priiloader files...\n");
				Delete_Priiloader_Files(1);
				printf("\n\nUpdate done, exiting to loader... waiting 5s...\n");
			}
			else if(!_Prii_Found)
			{
				printf("Attempting to delete leftover files...\n");
				Delete_Priiloader_Files(0);
				printf("Install done, exiting to loader... waiting 5s...\n");
			}
			sleepx(5);
			exit(0);

		}
		else if (pDown & WPAD_BUTTON_MINUS || GCpDown & PAD_BUTTON_Y )
		{
			printf("\x1b[2J");
			fflush(stdout);
			printf("IOS %d rev %d\n\n\n",IOS_GetVersion(),IOS_GetRevision());
			printf("Checking for Priiloader...\n");
			fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
			if (fd < 0)
			{
				abort("Priiloader not found");
			}
			else
			{
				ISFS_Close(fd);
				printf("Priiloader installation found.\n\n");
				RemovePriiloader();
				PatchTMD(1);
				printf("Deleting extra Priiloader files...\n");
				Delete_Priiloader_Files(2);
				printf("Done!\n\n");
				printf("Removal done, exiting to loader... waiting 5s...\n");
				sleepx(5);
				exit(0);
			}
		}
		if ( GCpDown & PAD_BUTTON_START || pDown & WPAD_BUTTON_HOME) 
		{
			printf("\x1b[5;0H");
			fflush(stdout);
			abort("Installation canceled");
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
