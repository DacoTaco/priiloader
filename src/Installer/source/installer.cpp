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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <malloc.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include <ogc/machine/processor.h>

#include "gecko.h"
#include "title.h"
#include "installer.h"
#include "nand.h"
#include "priiloader_app.h"

const char* _titleAppFormat = "/title/%08x/%08x/content/%08x.app";
char _originalBootAppPath[ISFS_MAXPATH] = {0};
char _copyBootAppPath[ISFS_MAXPATH] = {0};

// only used for vwii
bool _isvWii = false;
char _vwiiAdditionalApp[ISFS_MAXPATH] = {0};
char _nandloaderAppPath[ISFS_MAXPATH] = {0};

char _tmdPath[ISFS_MAXPATH] = {};
char _tmdBackupPath[ISFS_MAXPATH] = {};

static u32 _tmdSize;
static u8 _tmdBuffer[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(64);
static signed_blob *_tmdBlob = (signed_blob *)_tmdBuffer;
static tmd *_smTmd = NULL; //set later, when TMD is loaded in
static u64 _targetTitleId = 0;

// only used for vwii
u32 _nandloaderTmdSize;
static u8 _nandloaderTmdBuffer[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(64);
static signed_blob *_nandloaderTmdBlob = (signed_blob *)_nandloaderTmdBuffer;
static tmd *_nandloaderTmd = NULL;
static const u64 _nandloaderTitleId = 0x0000000100000200ll;


inline void ProcessDeleteResponse( s32 ret )
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

s32 DeletePriiloaderFile(const char* pathFormat, const char* filename, const char* fileDescription)
{
	char file_path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32) = {0};
	std::string format = std::string(pathFormat);
	if(format.back() != '/')
		format.push_back('/');
	
	format += filename;
	sprintf(file_path, format.c_str(), TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId));
	s32 ret = ISFS_Delete(file_path);

	gprintf("deleting %s : %d", file_path, ret);
	printf("%s: ", fileDescription);
	ProcessDeleteResponse(ret);

	return ret;
}

void DeletePriiloaderFiles(InstallerAction action )
{
	const char* dataFolder = "/title/%08x/%08x/data/";
	const char* contentFolder = "/title/%08x/%08x/content/";
	bool settings = false;
	bool hacks = false;
	bool password = false;
	bool main_bin = false;
	bool ticket = false;
	switch(action)
	{
		case InstallerAction::Remove: //remove
			main_bin = true;
			ticket = true;
		case InstallerAction::Install: //install	
			hacks = true;
		case InstallerAction::Update: //update	
			settings = true;
			password = true;
		default:
			break;
	}

	if(action == InstallerAction::Install)
		printf("Attempting to delete leftover files...\r\n");
	else
		printf("Deleting extra Priiloader files...\r\n");

	if(password)
		DeletePriiloaderFile(dataFolder, "password.txt", "password");

	if(settings)
		DeletePriiloaderFile(dataFolder, "loader.ini", "Settings file");
	
	//its best we delete that ticket but its completely useless and will only get in our 
	//way when installing again later...
	if(ticket)
		DeletePriiloaderFile(contentFolder, "ticket", "Ticket");
	
	if(hacks)
	{
		DeletePriiloaderFile(dataFolder, "hacks_s.ini", "Hacks_s.ini");
		DeletePriiloaderFile(dataFolder, "hacks.ini", "Hacks.ini");
		DeletePriiloaderFile(dataFolder, "hacksh_s.ini", "Hacksh_s.ini");
		DeletePriiloaderFile(dataFolder, "hackshas.ini", "system_hacks");
	}
	if(main_bin)
	{
		DeletePriiloaderFile(dataFolder, "main.nfo", "autoboot binary info");
		DeletePriiloaderFile(dataFolder, "main.bin", "autoboot binary");
	}
	return;
}

bool PriiloaderInstalled(void)
{
	printf("Checking for Priiloader...\r\n");				
	gprintf("Checking for SystemMenu Dol");
	s32 fd = ISFS_Open(_copyBootAppPath, ISFS_OPEN_RW);
	if (fd < 0)
		return false;

	ISFS_Close(fd);
	return true;
}

bool CheckvWiiNandLoader(void)
{
	s32 ret;
	u32 x;

	//check if BC-NAND is installed
	ret = ES_GetTitleContentsCount(_nandloaderTitleId, &x);

	if (ret < 0)
		return false; // title was never installed

	if (x <= 0)
		return false; // title was installed but deleted via Channel Management

	return true;
}

void InitializeInstaller(u64 titleId, bool isvWii)
{
	_isvWii = isvWii;
	_targetTitleId = titleId;

	if (_isvWii && !CheckvWiiNandLoader())
		throw "BC-NAND not installed!";

	//read TMD so we can get the main booting dol
	memset(_tmdPath, 0, ISFS_MAXPATH);
	memset(_tmdBackupPath, 0, ISFS_MAXPATH);
	sprintf(_tmdPath, "/title/%08x/%08x/content/title.tmd",TITLE_UPPER(_targetTitleId),TITLE_LOWER(_targetTitleId));
	sprintf(_tmdBackupPath, "/title/%08x/%08x/content/title_or.tmd",TITLE_UPPER(_targetTitleId),TITLE_LOWER(_targetTitleId));

	s32 ret = ES_GetStoredTMDSize(_targetTitleId, &_tmdSize);
	if(ret < 0)
		throw "Unable to get stored tmd size";
	
	ret = ES_GetStoredTMD(_targetTitleId, _tmdBlob, _tmdSize);
	if (ret < 0)
		throw "Unable to get stored tmd";
	
	_smTmd = (tmd*)SIGNATURE_PAYLOAD(_tmdBlob);
	gprintf("sm version : %d", _smTmd->title_version);

	if(_smTmd->title_version > 0x2000)
		throw "System Menu version invalid or not vanilla (v" + std::to_string(_smTmd->title_version) + ")";
	
	// For vWii only the 5.2.0 SM is supported, everything else is untested and probably doesn't work with the builtin hacks
	if (_isvWii && (_smTmd->title_version & 0xFFF0) != 608)
		throw "System Menu version not supported (v" + std::to_string(_smTmd->title_version) + ")";

	u32 fileId = 0;
	for(u16 i=0; i < _smTmd->num_contents; ++i)
	{
		if (_smTmd->contents[i].index == _smTmd->boot_index)
		{
			fileId = _smTmd->contents[i].cid;
			break;
		}
	}

	if (fileId == 0)
		throw "Unable to retrieve title booting app";

	memset(_originalBootAppPath, 0, ISFS_MAXPATH);
	memset(_copyBootAppPath, 0, ISFS_MAXPATH);
	sprintf(_originalBootAppPath, _titleAppFormat, TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId), fileId);
	fileId = 0x10000000 | (fileId & 0x0FFFFFFF);
	sprintf(_copyBootAppPath, _titleAppFormat, TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId), fileId);

	// For vWii use additional content containing priiloader, which replaces content 1
	if (_isvWii)
	{
		// This is where the priiloader app will be written to (2XXXXXXX.app)
		fileId = 0x20000000 | (fileId & 0x0FFFFFFF);
		memset(_vwiiAdditionalApp, 0, ISFS_MAXPATH);
		sprintf(_vwiiAdditionalApp, _titleAppFormat, TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId), fileId);	

		// Verify the BC-NAND tmd
		ret = ES_GetStoredTMDSize(_nandloaderTitleId, &_nandloaderTmdSize);
		if(ret < 0)
			throw "Unable to get stored nandloader tmd size";

		ret = ES_GetStoredTMD(_nandloaderTitleId, _nandloaderTmdBlob, _nandloaderTmdSize);
		if (ret < 0)
			throw "Unable to get stored nandloader tmd";

		_nandloaderTmd = (tmd*)SIGNATURE_PAYLOAD(_nandloaderTmdBlob);
		for(u8 i=0; i < _nandloaderTmd->num_contents; ++i)
		{
			if (_nandloaderTmd->contents[i].index == _nandloaderTmd->boot_index)
			{
				fileId = _nandloaderTmd->contents[i].cid;
				break;
			}
		}
		if (fileId == 0)
			throw "Unable to retrieve nandloader booting app";

		// Write the nandloader app path
		memset(_nandloaderAppPath, 0, ISFS_MAXPATH);
		sprintf(_nandloaderAppPath, _titleAppFormat, TITLE_UPPER(_nandloaderTitleId), TITLE_LOWER(_nandloaderTitleId), fileId);
	}

	gprintf("original file: %s", _originalBootAppPath);
	gprintf("copy file    : %s", _copyBootAppPath);
	gprintf("vwii file    : %s", _vwiiAdditionalApp);
	gprintf("nandloader   : %s", _nandloaderAppPath);
}

s32 RemovePriiloader(void)
{
	std::string errorMsg = "";
	try
	{
		NandPermissions appPermissions = {
			.ownerperm = 3,
			.groupperm = 3,
			.otherperm = 3,
		};
		
		printf("Restoring System menu app...");
		s32 ret = NandCopy(_copyBootAppPath, _originalBootAppPath, appPermissions);
		if(ret < 0)
			throw "Failed to restore System Menu";

		ISFS_Delete(_copyBootAppPath);
		printf("Done!\r\n");

		if (_isvWii)
		{
			// Delete the actual Priiloader content on vWii
			printf("Deleting Priiloader app...");
			ISFS_Delete(_vwiiAdditionalApp);
			printf("Done!\r\n");
		}
	}
	catch (const std::string& ex)
	{
		errorMsg = ex;
	}
	catch (char const* ex)
	{
		errorMsg = ex;
	}
	catch(...)
	{
		errorMsg = "Unknown Error Occurred";
	}

	if(!errorMsg.empty())
	{
		ISFS_CreateFile(_originalBootAppPath, 0, 3, 3, 3);
		s32 fd = ISFS_Open(_originalBootAppPath, ISFS_OPEN_RW);
		ISFS_Write(fd, priiloader_app, priiloader_app_size);
		ISFS_Close(fd);
		throw errorMsg;
	}

	return 1;
}

s32 WritePriiloader(InstallerAction action)
{
	if(action != InstallerAction::Install && action != InstallerAction::Update)
		return -1;

	s32 ret;
	NandPermissions appPermissions = {
		.ownerperm = 3,
		.groupperm = 3,
		.otherperm = 3,
	};
	if(action == Install)
	{
		//system menu coping
		printf("Moving System Menu app...");
		ret = NandCopy(_originalBootAppPath, _copyBootAppPath, appPermissions);
		if(ret < 0)
		{
			ISFS_Delete(_copyBootAppPath);
			throw "Failed to create System menu app copy";
		}

		gprintf("Moved System Menu App");
		printf("Done!\r\n");
	}

	//sys_menu app moved. lets write priiloader
	ret = 0;
	printf("Writing Priiloader app...");
	gprintf("Writing Priiloader");

	if (_isvWii)
	{
		// Write priiloader to an additional content file for vWii (2XXXXXXX.app)
		ret = NandWrite(_vwiiAdditionalApp, priiloader_app, priiloader_app_size, appPermissions);
		if (ret < 0)
		{
			ISFS_Delete(_vwiiAdditionalApp);
			throw "Unable to write Priiloader app. error " + std::to_string(ret);
		}
		
		gprintf("Priiloader app written.");
		printf("Done!\r\n");

		// On vWii we replace the SM boot content with BC-NAND
		printf("Replacing System Menu app...");
		ret = NandCopy(_nandloaderAppPath, _originalBootAppPath, appPermissions);
		if (ret < 0)
		{
			try{NandCopy(_copyBootAppPath, _originalBootAppPath, appPermissions);}catch(...){}
			throw "Unable to replace the system menu. error " + std::to_string(ret) + ", changes reverted.";
		}
		
		gprintf("System Menu replaced.");
		printf("Done!\r\n");
	}
	else
	{
		ret = NandWrite(_originalBootAppPath, priiloader_app, priiloader_app_size, appPermissions);
		if(ret < 0)
		{
			try{NandCopy(_copyBootAppPath, _originalBootAppPath, appPermissions);}catch(...){}
			ISFS_Delete(_copyBootAppPath);
			throw "Failed to write Priiloader : error " + std::to_string(ret);
		}
	}

	gprintf("Priiloader Update Complete");
	printf("Done!\r\n\r\n");
	return 1;
}

s32 CopyTicket(void)
{
	s32 ret = 0;
	char ticketDestinationPath[ISFS_MAXPATH];
	memset(ticketDestinationPath, 0, ISFS_MAXPATH);
	sprintf(ticketDestinationPath, "/title/%08x/%08x/content/ticket", TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId));
	gprintf("Checking for copy ticket...");

	ret = ISFS_Open(ticketDestinationPath, ISFS_OPEN_READ);
	if (ret >= 0)
	{
		ISFS_Close(ret);
		printf("Skipping copy of system menu ticket...\r\n");
		gprintf("ticket copy exists.");
		return 1;
	}

	gprintf("failed to open ticket, creating copy");
	char ticketSourcePath[ISFS_MAXPATH];
	NandPermissions appPermissions = {
		.ownerperm = 3,
		.groupperm = 3,
		.otherperm = 3,
	};

	memset(ticketSourcePath, 0, ISFS_MAXPATH);
	sprintf(ticketSourcePath, "/ticket/%08x/%08x.tik", TITLE_UPPER(_targetTitleId), TITLE_LOWER(_targetTitleId));
	ISFS_Delete(ticketDestinationPath);
	ret = NandCopy(ticketSourcePath, ticketDestinationPath, appPermissions);
	if(ret < 0)
		throw "Failed to make ticket copy!";
	
	printf("Done!\r\n");
	return 1;
}

s32 PatchTMD( InstallerAction action )
{
	NandPermissions filePermissions = { 0 };
	s32 ret = ISFS_GetAttr(_tmdPath, &filePermissions.owner, &filePermissions.group, &filePermissions.attributes, &filePermissions.ownerperm, &filePermissions.groupperm, &filePermissions.otherperm);
	if(ret < 0 )
	{
		//attribute getting failed. returning to default
		printf("\x1b[%d;%dm", 33, 1);
		printf("\nWARNING : failed to get file's permissions. using defaults\r\n");
		printf("\x1b[%d;%dm", 37, 1);
		gprintf("permission failure on desination! error %d", ret);
		gprintf("writing with max permissions");
		filePermissions = {
			.ownerperm = 3,
			.groupperm = 3,
			.otherperm = 0
		};
	}

	gprintf("ret %d owner %d group %d attributes %X perm:%X-%X-%X", ret, filePermissions.owner, filePermissions.group, filePermissions.attributes, filePermissions.ownerperm, filePermissions.groupperm, filePermissions.otherperm);

	s32 fd = ISFS_Open(_tmdBackupPath, ISFS_OPEN_READ);
	if(fd < 0 && action == InstallerAction::Remove)
	{
		printf("TMD backup not found. leaving TMD alone...\r\n");
		return 1;
	}
	ISFS_Close(fd);

	switch(action)
	{
		//not so sure why we'd want to delete the tmd modification but just to remove all traces of priiloader... :)
		case InstallerAction::Remove:
			gprintf("restoring TMD...");
			printf("Restoring System Menu TMD...\r\n");

			ret = NandCopy(_tmdBackupPath, _tmdPath, filePermissions);
			if ( ret >= 0)
			{
				ISFS_Delete(_tmdBackupPath);
				return 1;
			}
			else
			{
				printf("\x1b[%d;%dm", 33, 1);
				printf("UNABLE TO RESTORE THE SYSTEM MENU TMD!!!\n\nTHIS COULD BRICK THE WII SO PLEASE REINSTALL SYSTEM MENU (0x%04X)\nWHEN RETURNING TO THE HOMEBREW CHANNEL!!!\n\r\n", _smTmd->title_version);
				printf("\x1b[%d;%dm", 37, 1);
				throw "TMD Restoration failed";
			}

			return 1;
		default:
			gprintf("patching TMD...");
			printf("Patching TMD...");
			break;
	}

	bool createBackup = fd < 0;
	void* backupTmdBuffer = NULL;
	void* applicationBuffer = NULL;
	std::string errorMsg = "";
	try
	{
		//read tmd
		ret = ISFS_Open(_tmdPath, ISFS_OPEN_READ);
		if (ret < 0)
			throw "failed to open TMD";

		fd = ret;
		STACK_ALIGN(fstats, fileStatus, sizeof(fstats), 32);
		ret = ISFS_GetFileStats(fd, fileStatus);
		if (ret < 0)
			throw "failed to get TMD information";
		
		memset(_tmdBuffer, 0, MAX_SIGNED_TMD_SIZE);
		_tmdSize = fileStatus->file_length;
		ret = ISFS_Read(fd, _tmdBuffer, _tmdSize);
		if(ret < 0)
			throw "Failed to read TMD data";
		
		ISFS_Close(fd);

		//read tmd backup, if it exists ofcourse
		backupTmdBuffer = memalign(32, MAX_SIGNED_TMD_SIZE);
		if(backupTmdBuffer == NULL)
			throw "Failed to allocate backup TMD buffer";
		
		fd = ISFS_Open(_tmdBackupPath, ISFS_OPEN_READ);
		if(fd >= 0)
		{
			ret = ISFS_GetFileStats(fd, fileStatus);
			if (ret < 0)
				throw "failed to get TMD backup information";
			
			ret = ISFS_Read(fd, backupTmdBuffer, fileStatus->file_length);
			if(ret < 0)
				throw "Failed to read TMD Backup data";

			ISFS_Close(fd);
		}

		signed_blob * backupTmdBlob = (signed_blob *)backupTmdBuffer;
		tmd* backupTmd = (tmd*)SIGNATURE_PAYLOAD(backupTmdBlob);
		//create TMD backup if needed
		if(createBackup || backupTmd->title_version != _smTmd->title_version)
		{
			gprintf("Making tmd backup...");
			ISFS_Delete(_tmdBackupPath);
			ret = NandWrite(_tmdBackupPath, _tmdBuffer, _tmdSize, filePermissions);
			if(ret < 0)
				throw "Failed to make TMD Backup";
		}

		//copy tmd content over so that we will be comparing with our state before patching tmd later on.
		memcpy(backupTmdBuffer, _tmdBuffer, _tmdSize);
		gprintf("TMD Check : backup TMD is correct.");
		gprintf("detected access rights : 0x%08X", _smTmd->access_rights);
		if(_smTmd->access_rights == 0x03)
			gprintf("no AHBPROT modification needed");
		else
			_smTmd->access_rights = 0x03;

		u32 appHash[5] ATTRIBUTE_ALIGN(32) = {0};
		if (_isvWii)
		{
			// First read the original app (which is now BC-NAND)
			memset(fileStatus, 0, sizeof(fstats));
			ret = ISFS_Open(_originalBootAppPath, ISFS_OPEN_READ);
			if(ret < 0)
				throw "Failed to open original app";
			
			fd = ret;
			ret = ISFS_GetFileStats(fd, fileStatus);
			if(ret < 0)
				throw "Failed to get original app stats";
			
			applicationBuffer = memalign(32, ALIGN32(fileStatus->file_length));
			if(applicationBuffer == NULL)
				throw "Failed to allocate memory for original app";
			
			ret = ISFS_Read(fd, applicationBuffer, fileStatus->file_length);
			if(ret < 0)
				throw "Failed to read original app";

			ISFS_Close(fd);

			//Get Hashes and do some changes
			ret = SHA_Calculate(applicationBuffer, fileStatus->file_length, appHash);
			if(ret < 0)
				throw "Failed to calculate application app hash";

			if (!memcmp(_smTmd->contents[_smTmd->boot_index].hash, appHash, sizeof(appHash) ) )
			{
				gprintf("no SHA hash change needed");
			}
			else
			{
				gprintf("altering app sha to %08x %08x %08x %08x %08x", appHash[0], appHash[1], appHash[2], appHash[3], appHash[4]);
				memcpy(_smTmd->contents[_smTmd->boot_index].hash, appHash, sizeof(appHash));
			}

			// Check if we already have additional contents
			if (_smTmd->num_contents == 9)
			{
				// Move content 1 to the end
				memcpy(&_smTmd->contents[_smTmd->num_contents], &_smTmd->contents[1], sizeof(tmd_content));
				_smTmd->contents[_smTmd->num_contents].index = _smTmd->num_contents;
				_smTmd->num_contents++;
				_tmdSize += sizeof(tmd_content);

				// Now replace content 1 with priiloader
				_smTmd->contents[1].cid = 0x20000000 | (_smTmd->contents[_smTmd->boot_index].cid & 0x0FFFFFFF);
				_smTmd->contents[1].index = 1;
				_smTmd->contents[1].type = 1;
			}
			// If we have more than 9 contents already make sure content 1 is priiloader
			else if (_smTmd->contents[1].cid >> 28 != 2)
				throw "Invalid SM TMD";

			// Update content 1 size
			if (_smTmd->contents[1].size != priiloader_app_size)
				_smTmd->contents[1].size = priiloader_app_size;
			
			// Update content 1 hash
			memset(appHash, 0, sizeof(appHash));
			ret = SHA_Calculate(priiloader_app, priiloader_app_size, appHash);
			if(ret < 0)
				throw "Failed to compute Priiloader hash";
			
			if (!memcmp(_smTmd->contents[1].hash, appHash, sizeof(appHash))) 
				gprintf("no SHA hash change needed");
			else
			{
				memcpy(_smTmd->contents[1].hash, appHash, sizeof(appHash));
				gprintf("new hash: %08x %08x %08x %08x %08x", appHash[0], appHash[1], appHash[2], appHash[3], appHash[4]);
				DCFlushRange(_smTmd,sizeof(tmd));
			}			
		}
		else
		{
			gprintf("checking Boot app SHA1 hash...");
			gprintf("bootapp ( %d ) SHA1 hash = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",_smTmd->boot_index
				,_smTmd->contents[_smTmd->boot_index].hash[0],_smTmd->contents[_smTmd->boot_index].hash[1],_smTmd->contents[_smTmd->boot_index].hash[2],_smTmd->contents[_smTmd->boot_index].hash[3]
				,_smTmd->contents[_smTmd->boot_index].hash[4],_smTmd->contents[_smTmd->boot_index].hash[5],_smTmd->contents[_smTmd->boot_index].hash[6],_smTmd->contents[_smTmd->boot_index].hash[7]
				,_smTmd->contents[_smTmd->boot_index].hash[8],_smTmd->contents[_smTmd->boot_index].hash[9],_smTmd->contents[_smTmd->boot_index].hash[10],_smTmd->contents[_smTmd->boot_index].hash[11]
				,_smTmd->contents[_smTmd->boot_index].hash[12],_smTmd->contents[_smTmd->boot_index].hash[13],_smTmd->contents[_smTmd->boot_index].hash[14],_smTmd->contents[_smTmd->boot_index].hash[15]
				,_smTmd->contents[_smTmd->boot_index].hash[16],_smTmd->contents[_smTmd->boot_index].hash[17],_smTmd->contents[_smTmd->boot_index].hash[18],_smTmd->contents[_smTmd->boot_index].hash[19]);
			gprintf("generated priiloader SHA1 : ");

			ret = SHA_Calculate(priiloader_app, priiloader_app_size, appHash);
			if(ret < 0)
				throw "Failed to compute Priiloader hash";
			
			if (!memcmp(_smTmd->contents[_smTmd->boot_index].hash, appHash, sizeof(appHash) ) )
			{
				gprintf("no SHA hash change needed");
			}
			else
			{
				memcpy(_smTmd->contents[_smTmd->boot_index].hash, appHash, sizeof(appHash));
				gprintf("new hash: %08x %08x %08x %08x %08x", appHash[0], appHash[1], appHash[2], appHash[3], appHash[4]);
				DCFlushRange(_smTmd,sizeof(tmd));
			}
		}

		//check if we need to write the tmd changes
		if (memcmp(_tmdBuffer, backupTmdBuffer, _tmdSize) != 0)
		{
			gprintf("saving TMD");
			ret = NandWrite(_tmdPath, _tmdBuffer, _tmdSize, filePermissions);
			if(ret < 0)
				throw "Failed to write TMD patch";
			printf("done!\r\n");
		}
		else
		{
			gprintf("no TMD mods needed");
			printf("no TMD modifications needed\r\n");
		}
	}
	catch (const std::string& ex)
	{
		errorMsg = ex;
	}
	catch (char const* ex)
	{
		errorMsg = ex;
	}
	catch(...)
	{
		errorMsg = "Unknown Error Occurred";
	}

	if(fd)
		ISFS_Close(fd);
	
	if(applicationBuffer)
	{
		free(applicationBuffer);
		applicationBuffer = NULL;
	}

	if(backupTmdBuffer)
	{
		free(backupTmdBuffer);
		backupTmdBuffer = NULL;
	}

	if(!errorMsg.empty())
	{
		printf("\x1b[%d;%dm", 33, 1);
		printf("UNABLE TO PATCH THE SYSTEM MENU TMD!!!\n\nTHIS COULD BRICK THE WII SO PLEASE REINSTALL SYSTEM MENU (v%d)\nWHEN RETURNING TO THE HOMEBREW CHANNEL!!!\n\r\n", _smTmd->title_version);
		printf("\x1b[%d;%dm", 37, 1);
		fd = ISFS_Open(_tmdBackupPath, ISFS_OPEN_RW);
		if(fd >= 0)
		{
			//the backup is there. as safety lets copy it back.
			ISFS_Close(fd);
			try{ NandCopy(_tmdBackupPath, _tmdPath, filePermissions); } catch(...) {}
		}
		ISFS_Delete(_tmdBackupPath);
		throw errorMsg;
	}

	return ret;
}