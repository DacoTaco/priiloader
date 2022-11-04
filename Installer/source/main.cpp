/*
Preloader/Priiloader Installer - An installation utiltiy for preloader (c) 2008-2020 crediar

Copyright (c) 2020 DacoTaco

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
#include <string>
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
#include "gecko.h"
#include "IOS.h"

//rev version
#include "gitrev.h"
#include "version.h"

// Bin Files

// Priiloader Application
#include "priiloader_app.h"
#include "certs_bin.h"
#include "su_tmd.h"
#include "su_tik.h"

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;

char original_app[ISFS_MAXPATH];
char copy_app[ISFS_MAXPATH];

char TMD_Path[ISFS_MAXPATH];
char TMD_Path2[ISFS_MAXPATH];

u32 tmd_size;
static u8 tmd_buf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(32);
static signed_blob *mTMD;
static tmd *rTMD;
static u64 title_id = 0x0000000100000002LL;

typedef struct
{
	u32 owner;
	u16 group;
	u8 attributes;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} Nand_Permissions;

typedef struct init_states {
	s8 AHBPROT;
} init_states;

void sleepx (int seconds)
{
	time_t start;
	time_t end;
	time(&start);
	time(&end);
	//i hate while loops. but its safer then sleep this way
	while(difftime(end, start) < seconds)
	{
		time(&end);
		VIDEO_WaitVSync();
	}
	return;
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
			return false;
		}
	}
	//it should never get here, but to kill that silly warning... :)
	return false;
}
void abort_pre_init(const char* msg, ...)
{
	WPAD_Init();
	PAD_Init();

	va_list args;
	char text[4096];
	va_start( args, msg );
	strcpy( text + vsnprintf( text,4095,msg,args ),""); 
	va_end( args );
	printf("\x1b[%d;%dm", 36, 1);
	gprintf("%s -> aborting mission...", text);
	printf("%s\nPress A to exit back to loader...\r\n",text);
	UserYesNoStop();
	printf("exiting...\r\n");
	printf("\x1b[%d;%dm", 37, 1);
	VIDEO_WaitVSync();
	exit(0);
}
void abort(const char* msg, ...)
{
	va_list args;
	char text[4096];
	va_start( args, msg );
	strcpy( text + vsnprintf( text,4095,msg,args ),""); 
	va_end( args );
	printf("\x1b[%d;%dm", 36, 1);
	gprintf("%s -> aborting mission...", text);
	printf("%s\nPress A to exit back to loader...\r\n",text);
	UserYesNoStop();
	printf("exiting...\r\n");
	printf("\x1b[%d;%dm", 37, 1);
	VIDEO_WaitVSync();
	exit(0);
}

bool CheckvWii (void) {
	s32 ret;
	u32 x;

	//check if the vWii NandLoader is installed ( 0x200 & 0x201)
	ret = ES_GetTitleContentsCount(0x0000000100000200ll, &x);

	if (ret < 0)
		return false; // title was never installed

	if (x <= 0)
		return false; // title was installed but deleted via Channel Management

	return true;
}
bool CompareSha1Hash(u8 *Data1,u32 Data1_Size,u8 *Data2,u32 Data2_Size)
{
	if(Data1 == NULL || Data2 == NULL )
	{
		gprintf("Data1 or Data2 == NULL");
		return false;
	}
	if(Data1_Size <= 0 || Data2_Size <= 0)
	{
		gprintf("Data1 or Data2 size == NULL");
		return false;
	}
	
	// set _D1 and _D2 to different initial values
	unsigned int FileHash_D1[5];
	unsigned int FileHash_D2[5];
	memset(FileHash_D1,0x00,sizeof(unsigned int)*5);
	memset(FileHash_D2,0xFF,sizeof(unsigned int)*5); 
	SHA1 sha; // SHA-1 class
	sha.Reset();
	sha.Input(Data1,Data1_Size);
	if (!sha.Result(FileHash_D1))
	{
		gprintf("could not compute Hash of D1!");
		return false;
	}
	sha.Reset();
	sha.Input(Data2,Data2_Size);
	if (!sha.Result(FileHash_D2))
	{
		gprintf("could not compute Hash of D2!");
		return false;
	}
	sha.Reset();
	/*gprintf("sha1 original(0x%x) : %x %x %x %x %x\nsha1 written(0x%x) : %x %x %x %x %x\r\n",
																		  Data1_Size,FileHash_D1[0],FileHash_D1[1],FileHash_D1[2],FileHash_D1[3],FileHash_D1[4],
																		  Data2_Size,FileHash_D2[0],FileHash_D2[1],FileHash_D2[2],FileHash_D2[3],FileHash_D2[4]);*/
	if (!memcmp(FileHash_D1,FileHash_D2,sizeof(FileHash_D1)))
	{
		return true;
	}
	else
	{		
		gprintf("SHA1 check failed!");
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
	gprintf("owner %d group %d attributes %X perm:%X-%X-%X", src_perm.owner, (u32)src_perm.group, (u32)src_perm.attributes, (u32)src_perm.ownerperm, (u32)src_perm.groupperm, (u32)src_perm.otherperm);
	
	//extract filename from destination
	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = strstr(destination,"/");
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp == NULL)
		return -1;
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}

	//create temp path
	sprintf(temp_dest,"/tmp/%s",ptemp);
	ISFS_Delete(temp_dest);

	//and go for it
	ret = ISFS_CreateFile(temp_dest,src_perm.attributes,src_perm.ownerperm,src_perm.groupperm,src_perm.otherperm);
	if (ret != ISFS_OK) 
	{
		printf("Failed to create file %s. ret = %u\r\n",temp_dest,ret);
		gprintf("Failed to create file %s. ret = %d",temp_dest,ret);
		return ret;
	}
	dest_handler = ISFS_Open(temp_dest,ISFS_OPEN_RW);
    if (dest_handler < 0)
	{
		gprintf("failed to open destination : %s",temp_dest);
		ISFS_Delete(temp_dest);
		return dest_handler;
	}

	ret = ISFS_Write(dest_handler,Buf_To_Write_to_Copy,buf_size);
	if (ret < 0)
	{
		gprintf("failed to write destination : %s",temp_dest);
		ISFS_Close(dest_handler);
		ISFS_Delete(temp_dest);
		return ret;
	}
	ISFS_Close(dest_handler);
	s32 temp = 0;
	u8 *Data2 = NULL;
	STACK_ALIGN(fstats,D2stat,sizeof(fstats),32);
	if (D2stat == NULL)
	{
		temp = -1;
		goto free_and_Return;
	}
	dest_handler = ISFS_Open(temp_dest,ISFS_OPEN_RW);
	if(dest_handler < 0)
	{
		gprintf("temp_dest open error %d",dest_handler);
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
		gprintf("nand_copy(buf) : rename returned %d",ret);
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
		gprintf("temp %d",temp);
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
	//place different data in D2 so that if something goes wrong later on, the comparison will fail
	unsigned int FileHash_D1[5];
	unsigned int FileHash_D2[5];
	memset(FileHash_D1,0x00,sizeof(unsigned int)*5);
	memset(FileHash_D2,0xFF,sizeof(unsigned int)*5); 
	SHA1 sha;
	sha.Reset();

	//variables - temp dir & SHA1 check
	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = strstr(destination,"/");
	u8 temp = 0;

	//get temp filename
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp == NULL)
	{
		gprintf("failed to compile destination path");
		return -1;
	}
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}
	sprintf(temp_dest,"/tmp/%s",ptemp);

	//get data into pointer from original file
	file_handler = ISFS_Open(source,ISFS_OPEN_READ);
    if (file_handler < 0)
	{
		gprintf("failed to open source : %s",source);
		return file_handler;
	}
    
	ret = ISFS_GetFileStats(file_handler,status);
    if (ret < 0)
    {
		printf("\n\nFailed to get information about %s!\r\n",source);
		sleepx(2);
        ISFS_Close(file_handler);
        return ret;
    }

    buffer = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (buffer == NULL)
	{
		gprintf("buffer failed to align");
		sleepx(2);
        ISFS_Close(file_handler);
		return 0;
	}
	memset(buffer,0,status->file_length);
    ret = ISFS_Read(file_handler,buffer,status->file_length);
    if (ret < 0)
    {
		printf("\n\nFailed to Read Data from %s!\r\n",source);
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
		gprintf("could not compute Hash of D1!");
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
		gprintf("failed to open destination : %s",temp_dest);
		ISFS_Delete(temp_dest);
		free(buffer);
		buffer = NULL;
		return file_handler;
	}
	ret = ISFS_Write(file_handler,buffer,status->file_length);
    if (ret < 0)
    {
		gprintf("failed to write destination : %s",destination);
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
		gprintf("buffer failed to align");
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
		gprintf("could not compute Hash of D2!");
		return -80;
	}
	sha.Reset();
	/*gprintf("sha1 original : %x %x %x %x %x\nsha1 written : %x %x %x %x %x\r\n",
																		  FileHash_D1[0],FileHash_D1[1],FileHash_D1[2],FileHash_D1[3],FileHash_D1[4],
																		  FileHash_D2[0],FileHash_D2[1],FileHash_D2[2],FileHash_D2[3],FileHash_D2[4]);*/
	if (!memcmp(FileHash_D1,FileHash_D2,sizeof(FileHash_D1)))
	{
		gprintf("nand_copy : SHA1 hash success");
		ISFS_Delete(destination);
		ret = ISFS_Rename(temp_dest,destination);
		gprintf("ISFS_Rename ret %d",ret);
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
		gprintf("nand_copy temp %d fail o.o;",temp);
		ISFS_Delete(temp_dest);
		return -80;
	}
	return 1;
}
void proccess_delete_ret( s32 ret )
{
	if(ret == -106)
	{
		printf("\x1b[%d;%dm", 32, 1);
		printf("Not found\r\n");
		printf("\x1b[%d;%dm", 37, 1);
	}
	else if(ret == -102)
	{
		printf("\x1b[%d;%dm", 33, 1);
		printf("Error deleting file: access denied\r\n");
		printf("\x1b[%d;%dm", 37, 1);
	}
	else if (ret < 0)
	{
		printf("\x1b[%d;%dm", 33, 1);
		printf("Error deleting file. error %d\r\n",ret);
		printf("\x1b[%d;%dm", 37, 1);
	}
	else
	{
		printf("\x1b[%d;%dm", 32, 1);
		printf("Deleted\r\n");
		printf("\x1b[%d;%dm", 37, 1);
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
		gprintf("password.txt : %d",ret);
		printf("password file : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	if(settings)
	{
		sprintf(file_path, "/title/%08x/%08x/data/loader.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("loader.ini : %d",ret);
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
		gprintf("ticket : %d",ret);
		printf("Ticket : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);
	}
	if(hacks)
	{
		sprintf(file_path, "/title/%08x/%08x/data/hacks_s.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks_s.ini : %d",ret);
		printf("Hacks_s.ini : ");
		proccess_delete_ret(ret);

		sprintf(file_path, "/title/%08x/%08x/data/hacks.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks.ini : %d",ret);
		printf("Hacks.ini : ");
		proccess_delete_ret(ret);

		sprintf(file_path, "/title/%08x/%08x/data/hacksh_s.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacksh_s.ini : %d",ret);
		printf("Hacksh_s.ini : ");
		proccess_delete_ret(ret);

		sprintf(file_path, "/title/%08x/%08x/data/hackshas.ini",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("hacks_hash : %d",ret);
		printf("system_hacks : ");
		proccess_delete_ret(ret);
		memset(file_path,0,ISFS_MAXPATH);

	}
	if(main_bin)
	{
		sprintf(file_path, "/title/%08x/%08x/data/main.nfo",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("main.nfo : %d",ret);
		sprintf(file_path, "/title/%08x/%08x/data/main.bin",(u32)(title_id >> 32),(u32) (title_id & 0xFFFFFFFF));
		ret = ISFS_Delete(file_path);
		gprintf("main.bin : %d",ret);
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
	unsigned int FileHash[5];
	u8 *TMD_chk = NULL;
	s32 fd = 0;
	s32 r = 0;
#ifdef DEBUG
	gprintf("Path : %s",TMD_Path);
#endif
	r = ISFS_GetAttr(TMD_Path, &Perm.owner, &Perm.group, &Perm.attributes, &Perm.ownerperm, &Perm.groupperm, &Perm.otherperm);
	if(r < 0 )
	{
		//attribute getting failed. returning to default
		printf("\x1b[%d;%dm", 33, 1);
		printf("\nWARNING : failed to get file's permissions. using defaults\r\n");
		printf("\x1b[%d;%dm", 37, 1);
		gprintf("permission failure on desination! error %d",r);
		gprintf("writing with max permissions");
		Perm.ownerperm = 3;
		Perm.groupperm = 3;
		Perm.otherperm = 0;
	}
	else
	{
		gprintf("r %d owner %d group %d attributes %X perm:%X-%X-%X", r, Perm.owner, Perm.group, Perm.attributes, Perm.ownerperm, Perm.groupperm, Perm.otherperm);
	}
	if(delete_mode == 0)
	{
		gprintf("patching TMD...");
		printf("Patching TMD...");

	}
	else
	{
		//return 1;
		gprintf("restoring TMD...");
		printf("Restoring System Menu TMD...\r\n");
	}

	fd = ISFS_Open(TMD_Path2,ISFS_OPEN_READ);
	if(fd < 0)
	{
		if(delete_mode)
		{
			printf("TMD backup not found. leaving TMD alone...\r\n");
			return 1;
		}
		else
		{
			//got to make tmd copy :)
			gprintf("Making tmd backup...");
			r = nand_copy(TMD_Path2,tmd_buf,tmd_size,Perm);
			if ( r < 0)
			{
				gprintf("Failure making TMD backup.error %d",r);
				printf("TMD backup/Patching Failure : error %d",r);
				goto _return;
			}
		}
		fd = 0;
	}
	ISFS_Close(fd);
	gprintf("TMD backup found");
	//not so sure why we'd want to delete the tmd modification but ok...
	if(delete_mode)
	{
		if ( nand_copy(TMD_Path2,TMD_Path,Perm) < 0)
		{
			if(r == -80)
			{
				//checksum issues
				printf("\x1b[%d;%dm", 33, 1);
				printf("\nWARNING!!\nInstaller could not calculate the Checksum when restoring the TMD back!\r\n");
				printf("the TMD however was copied...\r\n");
				printf("Do you want to Continue ?\r\n");
				printf("A = Yes       B = No\n  ");
				printf("\x1b[%d;%dm", 37, 1);
				if(!UserYesNoStop())
				{
					nand_copy(TMD_Path,TMD_Path2,Perm);
					abort("TMD restoring failure.");
				}		
			}
			else
			{
				printf("\x1b[%d;%dm", 33, 1);
				printf("UNABLE TO RESTORE THE SYSTEM MENU TMD!!!\n\nTHIS COULD BRICK THE WII SO PLEASE REINSTALL SYSTEM MENU\nWHEN RETURNING TO THE HOMEBREW CHANNEL!!!\n\r\n");
				printf("\x1b[%d;%dm", 37, 1);
				printf("press A to return to the homebrew channel\r\n");
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
			gprintf("TMD bCheck : failed to open source : %s",TMD_Path2);
			goto patch_tmd;
		}
		r = ISFS_GetFileStats(fd,TMDb_status);
		if (r < 0)
		{
			gprintf("TMD bCheck : Failed to get information about %s!",TMD_Path2);
			ISFS_Close(fd);
			goto patch_tmd;
		}
		memset(tmdb_buf,0,MAX_SIGNED_TMD_SIZE);

		r = ISFS_Read(fd,tmdb_buf,TMDb_status->file_length);
		if (r < 0)
		{
			gprintf("TMD bCheck : Failed to Read Data from %s!",TMD_Path2);
			ISFS_Close(fd);
			goto patch_tmd;
		}
		ISFS_Close(fd);
		mbTMD = (signed_blob *)tmdb_buf;
		pbTMD = (tmd*)SIGNATURE_PAYLOAD(mbTMD);
		if (pbTMD->title_version != rTMD->title_version)
		{
			gprintf("TMD bCheck : backup TMD version mismatch: %d & %d",rTMD->title_version,pbTMD->title_version);
			//got to make tmd copy :)
			r = nand_copy(TMD_Path2,tmd_buf,tmd_size,Perm);
			if ( r < 0)
			{
				gprintf("TMD bCheck : Failure making TMD backup.error %d",r);
				printf("TMD backup/Patching Failure : error %d",r);
				goto _return;
			}
		}
		else
			gprintf("TMD bCheck : backup TMD is correct");
		r = 0;
	}
patch_tmd:
	gprintf("detected access rights : 0x%08X",rTMD->access_rights);
	if(rTMD->access_rights == 0x03)
	{
		gprintf("no AHBPROT modification needed");
	}
	else
	{
		rTMD->access_rights = 0x03;
		DCFlushRange(rTMD,sizeof(tmd));
		if(rTMD->access_rights != 0x03)
		{
			gprintf("rights change failure.");
			goto _return;
		}
		SaveTmd++;
	}
	gprintf("checking Boot app SHA1 hash...");
	gprintf("bootapp ( %d ) SHA1 hash = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",rTMD->boot_index
		,rTMD->contents[rTMD->boot_index].hash[0],rTMD->contents[rTMD->boot_index].hash[1],rTMD->contents[rTMD->boot_index].hash[2],rTMD->contents[rTMD->boot_index].hash[3]
		,rTMD->contents[rTMD->boot_index].hash[4],rTMD->contents[rTMD->boot_index].hash[5],rTMD->contents[rTMD->boot_index].hash[6],rTMD->contents[rTMD->boot_index].hash[7]
		,rTMD->contents[rTMD->boot_index].hash[8],rTMD->contents[rTMD->boot_index].hash[9],rTMD->contents[rTMD->boot_index].hash[10],rTMD->contents[rTMD->boot_index].hash[11]
		,rTMD->contents[rTMD->boot_index].hash[12],rTMD->contents[rTMD->boot_index].hash[13],rTMD->contents[rTMD->boot_index].hash[14],rTMD->contents[rTMD->boot_index].hash[15]
		,rTMD->contents[rTMD->boot_index].hash[16],rTMD->contents[rTMD->boot_index].hash[17],rTMD->contents[rTMD->boot_index].hash[18],rTMD->contents[rTMD->boot_index].hash[19]);
	gprintf("generated priiloader SHA1 : ");
	sha.Reset();
	sha.Input(priiloader_app, priiloader_app_size);
	if (!sha.Result(FileHash))
	{
		gprintf("could not compute Hash of Priiloader!");
		r = -1;
		goto _return;
	}
	if (!memcmp(rTMD->contents[rTMD->boot_index].hash, FileHash, sizeof(FileHash) ) )
	{
		gprintf("no SHA hash change needed");
	}
	else
	{
		memcpy(rTMD->contents[rTMD->boot_index].hash,FileHash,sizeof(FileHash));
		gprintf("%08x %08x %08x %08x %08x",FileHash[0],FileHash[1],FileHash[2],FileHash[3],FileHash[4]);
		DCFlushRange(rTMD,sizeof(tmd));
		SaveTmd++;
	}
	if(SaveTmd > 0)
	{
		gprintf("saving TMD");
		r = nand_copy(TMD_Path,tmd_buf,tmd_size,Perm);
		if(r < 0 )	
		{
			gprintf("nand_copy failure. error %d",r);
			if(r == -80)
				goto _checkreturn;
			else
				goto _return;
		}
	}
	else
	{
		gprintf("no TMD mod's needed");
		printf("no TMD modifications needed\r\n");
		goto _return;
	}
	printf("Done\r\n");
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
		printf("\x1b[%d;%dm", 33, 1);
		printf("\nWARNING!!\nInstaller couldn't Patch the system menu TMD.\r\n");
		printf("Priiloader could still end up being installed but could end up working differently\r\n");
		printf("Do you want the Continue ?\r\n");
		printf("A = Yes       B = No\n  ");
		printf("\x1b[%d;%dm", 37, 1);
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
			printf("\nDone!\r\n");
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
	printf("\x1b[%d;%dm", 33, 1);
	printf("\nWARNING!!\n  Installer could not calculate the Checksum for the TMD!");
	printf("\nbut Patch write was successfull.\r\n");
	printf("Do you want the Continue ?\r\n");
	printf("A = Yes       B = No\n  ");
	printf("\x1b[%d;%dm", 37, 1);
	if(!UserYesNoStop())
	{
		printf("reverting changes...\r\n");
		nand_copy(TMD_Path2,TMD_Path,Perm);
		abort("TMD Patch failure\r\n");
	}		
	else
	{
		printf("\nDone!\r\n");
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
	gprintf("Checking for copy ticket...");
	fd = ISFS_Open(TIK_Path_dest,ISFS_OPEN_READ);
	if (fd >= 0)
	{
		ISFS_Close(fd);
		printf("Skipping copy of system menu ticket...\r\n");
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
			printf("Priiloader system menu ticket not found.\n\tTrying to read original ticket...\r\n");
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
		abort("Unable to copy the system menu ticket");

	printf("Done!\r\n");
	return 1;
}
bool CheckForPriiloader( void )
{
	bool ret = false;
	s32 fd = 0;
	printf("Checking for Priiloader...\r\n");				
	gprintf("checking for SystemMenu Dol");
	fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
	if (fd < 0)
	{
		printf("Priiloader not found : Installing Priiloader...\n\r\n");
		ret = false;
	}
	else
	{
		ISFS_Close(fd);
		printf("Priiloader installation found : Updating Priiloader...\n\r\n");
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
				printf("\x1b[%d;%dm", 33, 1);
				printf("\nWARNING!!\n  Installer could not calculate the Checksum for the System menu app");
				printf("\nbut Copy was successfull.\r\n");
				printf("Do you want the Continue ?\r\n");
				printf("A = Yes       B = No\n  ");
				printf("\x1b[%d;%dm", 37, 1);
				if(!UserYesNoStop())
				{
					printf("reverting changes...\r\n");
					ISFS_Delete(copy_app);
					abort("System Menu Copying Failure");
				}		
				else
					printf("\nDone!\r\n");
			}
			else
				abort("\nUnable to move the system menu. error %d",ret);
		}
		else
		{
			gprintf("Moving System Menu Done");
			printf("Done!\r\n");
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
	gprintf("Writing Priiloader");

	char temp_dest[ISFS_MAXPATH];
	memset(temp_dest,0,ISFS_MAXPATH);
	char *ptemp = strstr(original_app,"/");
	while(ptemp != NULL && strstr(ptemp+1,"/") != NULL)
	{
		ptemp = strstr(ptemp+1,"/");
	}
	if(ptemp == NULL)
	{
		gprintf("error WritePriiloader: failed to compile tmp filepath",fd);
		abort("\nFailed to compile tmp filepath");
	}
	if(ptemp[0] == '/')
	{
		ptemp = ptemp+1;
	}
	sprintf(temp_dest,"/tmp/%s",ptemp);
	ISFS_Delete(temp_dest);
	ret = ISFS_CreateFile(temp_dest,SysPerm.attributes,SysPerm.ownerperm,SysPerm.groupperm,SysPerm.otherperm);
    if (ret < 0)
    {
        gprintf("error %d", fd);
        abort("\nFailed to create file for Priiloader");
    }

	fd = ISFS_Open(temp_dest,ISFS_OPEN_RW);
	if (fd < 0)
	{
		gprintf("error %d",fd);
		abort("\nFailed to open file for Priiloader writing");
	}
	ret = ISFS_Write(fd, priiloader_app, priiloader_app_size);
	if (ret < 0 ) //check if the app was writen correctly				
	{
		ISFS_Close(fd);
		ISFS_Delete(copy_app);
		ISFS_Delete(temp_dest);
		gprintf("Write failed. ret %d",ret);
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
			gprintf("Size Check Success");
			printf("Size Check Success!\r\n");
		}
	}
	u8 *AppData = (u8 *)memalign(32,ALIGN32(status->file_length));
	if (AppData)
		ret = ISFS_Read(fd,AppData,status->file_length);
	else
	{
		ISFS_Close(fd);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure! MemAlign Failure of AppData\r\n");
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
		abort("Checksum comparison Failure! read of priiloader app returned %u\r\n",ret);
	}
	if(CompareSha1Hash((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
		printf("Checksum comparison Success!\r\n");
	else
	{
		if (AppData)
		{
			free(AppData);
			AppData = NULL;
		}
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure!\r\n");
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
		gprintf("WritePriiloader : rename returned %d",ret);
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("\nFailed to Write Priiloader : error Ren %d",ret);
	}
	printf("Done!!\r\n");
	gprintf("Wrote Priiloader App.Checking Installation");
	printf("\nChecking Priiloader Installation...\r\n");
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
			gprintf("Size Check Success");
			printf("Size Check Success!\r\n");
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
		abort("Checksum comparison Failure! MemAlign Failure of AppData\r\n");
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
		abort("Checksum comparison Failure! read of priiloader app returned %u\r\n",ret);
	}
	if(CompareSha1Hash((u8*)priiloader_app,priiloader_app_size,AppData,status->file_length))
		printf("Checksum comparison Success!\r\n");
	else
	{
		if (AppData)
		{
			free(AppData);
			AppData = NULL;
		}
		nand_copy(copy_app,original_app,SysPerm);
		ISFS_Delete(copy_app);
		abort("Checksum comparison Failure!\r\n");
	}
	if (AppData)
	{
		free(AppData);
		AppData = NULL;
	}
	gprintf("Priiloader Update Complete");
	printf("Done!\r\n\r\n");
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
			printf("\x1b[%d;%dm", 33, 1);
			printf("\nWARNING!!\n  Installer could not calculate the Checksum when coping the System menu app\r\n");
			printf("back! the app however was copied...\r\n");
			printf("Do you want to Continue ?\r\n");
			printf("A = Yes       B = No\n  ");
			printf("\x1b[%d;%dm", 37, 1);
			if(!UserYesNoStop())
			{
				printf("reverting changes...\r\n");
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
	printf("Done!\r\n");
	return 1;
}
s8 HaveNandPermissions( void )
{
	gprintf("testing permissions...");
	s32 temp = ISFS_Open("/title/00000001/00000002/content/title.tmd",ISFS_OPEN_RW);
	if ( temp < 0 )
	{
		gprintf("no permissions.error %d",temp);
		return false;
	}
	else
	{
		ISFS_Close(temp);
		gprintf("and bingo was his name-O");
		return true;
	}
}
int main(int argc, char **argv)
{
	s8 ios_patched = 0;
	init_states wii_state;
	s32 ret = 0;
	s32 dolphinFd = -1; 
	
	dolphinFd = IOS_Open("/dev/dolphin", IPC_OPEN_NONE); 
	if (dolphinFd >= 0) IOS_Close(dolphinFd);

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
	CON_Init(xfb,x,y,w,h, vmode->fbWidth*VI_DISPLAY_PIX_SZ );
	printf("\r\n\r\n\r\n");
	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);
	gprintf("resolution is %dx%d",vmode->viWidth,vmode->viHeight);

	if( IOS_GetVersion() >= 200 )
	{
		gprintf("F\r\nA\r\nI\r\nL\r\n");
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("\r\n\r\n\r\n\r\nusing a non-valid IOS(%d)",IOS_GetVersion());
	}
	if(ReadRegister32(0x0d800064) != 0xFFFFFFFF)
	{
		ReloadIOS(36, 0);
		if ( ( IOS_GetRevision() < 200 ) || ( IOS_GetRevision() > 0xFF01 ) || ( IOS_GetVersion() != 36 ) )
		{
			gprintf("F\r\nA\r\nI\r\nL\r\n");
			printf("\x1b[2J");
			fflush(stdout);
			abort_pre_init("\r\n\r\n\r\n\r\ncIOSPAGHETTI(Cios Infected) IOS detected");
		}
	}

	printf("\r\nIOS %d rev %d\r\n\r\n",IOS_GetVersion(),IOS_GetRevision());
#if BETAVERSION > 0
	printf("Priiloader v%d.%d.%db%d(r0x%08x) Installation/Removal Tool\n\n\n\n\t", VERSION.major, VERSION.minor, VERSION.patch, VERSION.beta, GIT_REV);
#else
	printf("\t\tPriiloader v%d.%d.%d(r0x%08x) Installation / Removal Tool\n\n\n\n\t", VERSION.major, VERSION.minor, VERSION.patch, GIT_REV);
#endif

	printf("\t\t\t\t\tPLEASE READ THIS CAREFULLY\n\n\t");
	printf("\t\tTHIS PROGRAM/TOOL COMES WITHOUT ANY WARRANTIES!\n\t");
	printf("\t\tYOU ACCEPT THAT YOU INSTALL THIS AT YOUR OWN RISK\n\n\n\t");
	printf("THE AUTHOR(S) CANNOT BE HELD LIABLE FOR ANY DAMAGE IT MIGHT CAUSE\n\n\t");
	printf("\tIF YOU DO NOT AGREE WITH THESE TERMS TURN YOUR WII OFF\n\n\n\n\t");
	printf("\t\t\t\t\tPlease wait while we init...");
		


	if( CheckvWii() ) 
	{
		printf("\x1b[31;1m");
		printf("\n\n\tError: vWii detected. please dont run this on a Wii u!");
		abort_pre_init("\r\n");
	}

	//patch and reload IOS so we don't end up with possible shit from the loading application ( looking at you HBC! >_< )
	//we don't need it in dolphin though
	if (dolphinFd >= 0)
		gprintf("Dolphin detected. We don't need AHBPROT");
	else if(ReadRegister32(0x0d800064) == 0xFFFFFFFF)
	{
		ios_patched = ReloadIOS(IOS_GetVersion(), 1);
		if(ios_patched < 0)
		{
			printf("\x1b[2J");
			fflush(stdout);
			abort_pre_init("\r\n\r\nfailed to do AHBPROT magic: error %d\r\n", ios_patched);
		}
	}

	memset(&wii_state,0,sizeof(init_states));
	wii_state.AHBPROT = ReadRegister32(0x0d800064) == 0xFFFFFFFF || dolphinFd >= 0;

	try
	{
		if (wii_state.AHBPROT && dolphinFd < 0 && PatchIOS({ SetUidPatcher, NandAccessPatcher, FakeSignOldPatch, FakeSignPatch, EsIdentifyPatch }) < 0)
		{
			gprintf("HW_AHBPROT isn't detected");
			printf("\x1b[2J");
			fflush(stdout);
			abort_pre_init("HW_AHBPROT isn't Set.\r\n Please check your priiloader/HBC installation!\r\n");
		}
	}
	catch (const std::string& ex)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init(ex.c_str());
	}
	catch (char const* ex)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init(ex);
	}
	catch (...)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("Error while patching IOS!");
	}	

	u64 TitleID = 0;
	if(wii_state.AHBPROT)
	{
		ret = ES_SetUID(title_id);
		gprintf("ES_SetUID : %d",ret);
		//kinda unstable attempt to indentify as SM. it *seems* to work sometimes. but its nice if it does. dont know what triggers it to work tho :/
		//if it works, ES_Identify says ticket/tmd is invalid but identifes us as SM anyway X'D
		//but it seems to cause a crash in ipc so this is why we dont do it
		/*ret = ES_Identify( (signed_blob *)certs_bin, certs_bin_size, (signed_blob *)su_tmd, su_tmd_size, (signed_blob *)su_tik, su_tik_size, 0);
		gprintf("ES_Identify : %d",ret);*/
		/*ret = ES_GetTitleID(&TitleID);
		gprintf("identified as = 0x%08X%08X",TITLE_UPPER(TitleID),TITLE_LOWER(TitleID));*/
	}
	else
	{
		u32 keyId = 0;
		ret = ES_Identify( (signed_blob*)certs_bin, certs_bin_size, (signed_blob*)su_tmd, su_tmd_size, (signed_blob*)su_tik, su_tik_size, &keyId);
		gprintf("ES_Identify : %d",ret);
		if(ret < 0)
		{
			printf("\x1b[2J");
			fflush(stdout);
			abort_pre_init("\n\n\n\nIOS%d isn't ES_Identify patched : error %d.",IOS_GetVersion(),ret);
		}
	}

	ES_GetTitleID(&TitleID);
	gprintf("identified as = 0x%08X%08X",TITLE_UPPER(TitleID),TITLE_LOWER(TitleID));

	if (ISFS_Initialize() < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("\n\n\n\nFailed to init ISFS");
	}
	gprintf("Got ROOT!");
	if (HaveNandPermissions())
	{
		ios_patched++;
	}
	else if (wii_state.AHBPROT)
	{
		gprintf("attempting ios 36...");
		ReloadIOS(36, 0);
		wii_state.AHBPROT = 0;
		if (HaveNandPermissions())
			ios_patched++;
		else
		{
			printf("\x1b[2J");
			fflush(stdout);
			abort_pre_init("\r\n\r\n\r\n\r\nFailed to retrieve nand permissions from nand!IOS 36 isn't patched!\r\n");
		}
	}
	else
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("\r\n\r\n\r\n\r\nFailed to retrieve nand permissions from nand!IOS 36 isn't patched!\r\n");
	}


	//IOS Shit Done
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
		abort_pre_init("Unable to get stored tmd size");
	}
	mTMD = (signed_blob *)tmd_buf;
	fd = ES_GetStoredTMD(title_id,mTMD,tmd_size);
	if (fd < 0)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("Unable to get stored tmd");
	}
	rTMD = (tmd*)SIGNATURE_PAYLOAD(mTMD);
	if(rTMD->title_version > 0x2000)
	{
		printf("\x1b[2J");
		fflush(stdout);
		abort_pre_init("System Menu version invalid or not vanilla (v%d / 0x%04X)",rTMD->title_version,rTMD->title_version);
	}
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
		abort_pre_init("Unable to retrieve title booting app");
	}

	memset(original_app,0,64);
	memset(copy_app,0,64);
	sprintf(original_app, "/title/%08x/%08x/content/%08x.app",TITLE_UPPER(title_id),TITLE_LOWER(title_id),id);
	sprintf(copy_app, "/title/%08x/%08x/content/%08x.app",TITLE_UPPER(title_id),TITLE_LOWER(title_id),id);
	copy_app[33] = '1';
	gprintf("%s &\r\n%s", original_app, copy_app);

	WPAD_Init();
	PAD_Init();
	sleepx(5);

	printf("\r\t\t\t   Press (+/A) to install or update Priiloader\r\n\t");
	printf("\tPress (-/Y) to remove Priiloader and restore system menu\r\n\t");
	if( (ios_patched < 1) && (IOS_GetVersion() != 36 ) )
		printf("  Hold Down (B) with any above options to use IOS36\r\n\t");
	printf("\tPress (HOME/Start) to chicken out and quit the installer!\r\n\r\n\t");
	printf("\t\t\t\t\tEnjoy! DacoTaco \r\n");
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();
		u16 pDown = WPAD_ButtonsDown(0);
		u16 GCpDown = PAD_ButtonsDown(0);
		u16 pHeld = WPAD_ButtonsHeld(0);
		u16 GCpHeld = PAD_ButtonsHeld(0);
		if ( (pHeld & WPAD_BUTTON_B || GCpHeld & PAD_BUTTON_B) && ( (pDown & WPAD_BUTTON_PLUS || GCpDown & PAD_BUTTON_A) || (pDown & WPAD_BUTTON_MINUS || GCpDown & PAD_BUTTON_Y ) ) )
		{
			if((ios_patched < 1) && (IOS_GetVersion() != 36 ) )
			{
				WPAD_Shutdown();
				ReloadIOS(36, 0);
				WPAD_Init();
				if(HaveNandPermissions())
					ios_patched++;
				else
				{
					printf("\x1b[2J");
					fflush(stdout);
					printf("\r\n\r\nFailed to retrieve nand permissions from nand!ios 36 isn't patched!\r\n");
					sleepx(5);
					exit(0);
				}
			}
    	}

		if (pDown & WPAD_BUTTON_PLUS || GCpDown & PAD_BUTTON_A)
		{
			//install Priiloader
			printf("\x1b[2J");
			fflush(stdout);
			printf("\r\nIOS %d rev %d\r\n\r\n\r\n",IOS_GetVersion(),IOS_GetRevision());
#ifdef BETA
			printf("\x1b[%d;%dm", 33, 1);
			printf("\nWARNING : ");
			printf("\x1b[%d;%dm", 37, 1);
			printf("this is a beta version. are you SURE you want to install this?\nA to confirm, Home/Start to abort\r\n");
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
				abort("\npatching TMD error %d!\r\n",ret);
			}
			if(_Prii_Found)
			{
				printf("Deleting extra priiloader files...\r\n");
				Delete_Priiloader_Files(1);
				abort("\n\nUpdate done!\r\n");
			}
			else if(!_Prii_Found)
			{
				printf("Attempting to delete leftover files...\r\n");
				Delete_Priiloader_Files(0);
				abort("Install done!\r\n");
			}

		}
		else if (pDown & WPAD_BUTTON_MINUS || GCpDown & PAD_BUTTON_Y )
		{
			printf("\x1b[2J");
			fflush(stdout);
			printf("\r\nIOS %d rev %d\n\n\r\n",IOS_GetVersion(),IOS_GetRevision());
			printf("Checking for Priiloader...\r\n");
			fd = ISFS_Open(copy_app,ISFS_OPEN_RW);
			if (fd < 0)
			{
				abort("Priiloader not found");
			}
			else
			{
				ISFS_Close(fd);
				printf("Priiloader installation found.\n\r\n");
				RemovePriiloader();
				PatchTMD(1);
				printf("Deleting extra Priiloader files...\r\n");
				Delete_Priiloader_Files(2);
				printf("Done!\r\n\r\n");
				abort("Removal done.\r\n");
			}
		}
		else if ( GCpDown & PAD_BUTTON_START || pDown & WPAD_BUTTON_HOME) 
		{
			VIDEO_ClearFrameBuffer( vmode, xfb, COLOR_BLACK);
			printf("\x1b[5;0H");
			fflush(stdout);
			VIDEO_WaitVSync();
			abort("Installation canceled");
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
