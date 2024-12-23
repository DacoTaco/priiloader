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
#include <string.h>
#include <string>
#include <unistd.h>
#include <gccore.h>
#include <ogc/machine/processor.h>

#include "gecko.h"
#include "title.h"
#include "installer.h"
#include "nand.h"

s32 NandWrite(const std::string destination, const void* data, u32 dataSize, NandPermissions destPermissions)
{
	if(destination.empty() || data == NULL || dataSize < 1)
		return NandActionErrors::InvalidArgument;

	std::string filename = destination.substr(destination.find_last_of("/\\") + 1);
	if(filename.empty())
		return NandActionErrors::InvalidArgument;

	char pathArray[ISFS_MAXPATH] = {0};
	sprintf(pathArray,"/tmp/%s", filename.c_str());
	std::string tempDestinationPath = std::string(pathArray);
	s32 fileDescriptor = 0;
	s32 ret = 0;
	std::string msg = "";
	void* buffer = NULL;

	try
	{
		//generate SHA1 hash of the data
		u32 sourceHash[5] ATTRIBUTE_ALIGN(32) = {0};
		ret = SHA_Calculate(data, dataSize, sourceHash);
		if(ret < 0)
		{
			ret = NandActionErrors::HashCalculation;
			throw "Failed to compute source hash";
		}

		//done, lets create temp file and write :')
		ISFS_Delete(tempDestinationPath.c_str());
		ret = ISFS_CreateFile(tempDestinationPath.c_str(), destPermissions.attributes, destPermissions.ownerperm, destPermissions.groupperm, destPermissions.otherperm);
		if(ret < 0)
			throw "Failed to create temp file " + tempDestinationPath;

		//created. opening it...
		ret = ISFS_Open(tempDestinationPath.c_str(), ISFS_OPEN_RW);
		if(ret < 0)
			throw "Failed to open temp file " + tempDestinationPath;
		
		fileDescriptor = ret;
		ret = ISFS_Write(fileDescriptor, data, dataSize);
		ISFS_Close(fileDescriptor);
		if (ret < 0)
			throw "Failed to write temp file " + tempDestinationPath;
		
		//write done. open file and calculate hash
		STACK_ALIGN(fstats, fileStatus, sizeof(fstats), 32);
		memset(fileStatus, 0, sizeof(fstats));
		ret = ISFS_Open(tempDestinationPath.c_str(), ISFS_OPEN_READ);
		if(ret < 0)
			throw "Failed to reopen temp file " + tempDestinationPath;
		
		fileDescriptor = ret;
		ret = ISFS_GetFileStats(fileDescriptor, fileStatus);
		if (ret < 0)
			throw "Failed to get stats of temp file " + tempDestinationPath;

		if(fileStatus->file_length != dataSize)
			throw "File size differs from input";

		buffer = memalign(32, ALIGN32(dataSize));
		if (buffer == NULL)
		{
			ret = NandActionErrors::MemoryAllocation;
			throw "failed to allocate destination buffer";	
		}

		memset(buffer, 0, fileStatus->file_length);
		ret = ISFS_Read(fileDescriptor, buffer, fileStatus->file_length);
		if(ret < 0)
			throw "Failed to read temp file";
		
		ISFS_Close(fileDescriptor);
		u32 destinationHash[5] ATTRIBUTE_ALIGN(32) = {0};
		ret = SHA_Calculate(buffer, fileStatus->file_length, destinationHash);
		if(ret < 0)
		{
			ret = NandActionErrors::HashCalculation;
			throw "Failed to compute source hash";
		}

		//gprintf("sha1 original : %x %x %x %x %x\n, sourceHash[0], sourceHash[1], sourceHash[2], sourceHash[3], sourceHash[4]);
		//gprintf("sha1 written : %x %x %x %x %x\r\n", destinationHash[0], destinationHash[1], destinationHash[2], destinationHash[3], destinationHash[4]);

		if (memcmp(sourceHash, destinationHash, sizeof(sourceHash)) != 0)
		{
			ret = NandActionErrors::HashComparison;
			throw "Data difference detected in " + tempDestinationPath;
		}

		gprintf("NandWrite : SHA1 hash success");

		//the actual dangerous part
		ISFS_Delete(destination.c_str());
		ret = ISFS_Rename(tempDestinationPath.c_str(), destination.c_str());
		if(ret < 0)
		{
			ret = NandActionErrors::RenameFailure;
			throw "Failed to rename " + tempDestinationPath + " to " + destination;
		}
		gprintf("NandWrite : %s written", destination.c_str());
	}
	catch (const std::string& ex)
	{
		msg = ex;
	}
	catch (char const* ex)
	{
		msg = std::string(ex);
	}
	catch(...)
	{
		msg = "Unknown Error Occurred";
	}
	
	//did an error occur
	if(!msg.empty())
	{
		printf("\n\n%s\r\n", msg.c_str());
		sleep(2);
	}

	if(buffer)
	{
		free(buffer);
		buffer = NULL;
	}

	ISFS_Close(fileDescriptor);
	ISFS_Delete(tempDestinationPath.c_str());
	return ret;
}

s32 NandCopy(const std::string source, const std::string destination, NandPermissions srcPermissions)
{
	if(source.empty() || destination.empty())
		return NandActionErrors::InvalidArgument;
	
	std::string filename = destination.substr(destination.find_last_of("/\\") + 1);
	if(filename.empty())
		return NandActionErrors::InvalidArgument;

	s32 fileDescriptor = 0;
	s32 ret = 0;
	std::string msg = "";
	void* buffer = NULL;

	try
	{
		//get data into pointer from original file
		ret = ISFS_Open(source.c_str(), ISFS_OPEN_READ);
		if (ret < 0)
			throw "Failed to open source " + source;

		fileDescriptor = ret;
		STACK_ALIGN(fstats, fileStatus, sizeof(fstats), 32);
		ret = ISFS_GetFileStats(fileDescriptor, fileStatus);
		if(ret < 0)
			throw "Failed to get information about " + source;

		buffer = memalign(32, ALIGN32(fileStatus->file_length));
		if (buffer == NULL)
		{
			ret = NandActionErrors::MemoryAllocation;
			throw "failed to allocate source buffer";	
		}

		memset(buffer, 0, fileStatus->file_length);
		ret = ISFS_Read(fileDescriptor, buffer, fileStatus->file_length);
		if (ret < 0)
			throw "Failed to Read Data from " + source;

		ISFS_Close(fileDescriptor);
		ret = NandWrite(destination, buffer, fileStatus->file_length, srcPermissions);		
	}
	catch (const std::string& ex)
	{
		msg = ex;
	}
	catch (char const* ex)
	{
		msg = std::string(ex);
	}
	catch(...)
	{
		msg = "Unknown Error Occurred";
	}
	
	//did an error occur
	if(!msg.empty())
	{
		gprintf("NandCopy error : %s", msg.c_str());
		printf("\n\n%s\r\n", msg.c_str());
		sleep(2);
	}

	if(buffer)
	{
		free(buffer);
		buffer = NULL;
	}
	ISFS_Close(fileDescriptor);
	return ret;
}