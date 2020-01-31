/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*/

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <algorithm>

#include "gecko.h"
#include "hacks.h"
#include "settings.h"
#include "error.h"
#include "font.h"
#include "mem2_manager.h"

#define BLOCK_SIZE ALIGN32(32)

std::vector<system_hack> system_hacks;
std::vector<u8> states_hash;

u32 file_size = 0;
FILE* sd_file_handler = NULL;
s32 nand_file_handler = -1;

s32 GetMasterHackIndexByID(const std::string& ID )
{
	s32 index = -1;
	u32 sysver = GetSysMenuVersion();
	auto it = std::find_if(system_hacks.begin(), system_hacks.end(), [&](const system_hack& obj) 
		{
			return obj.masterID == ID && 
			obj.max_version >= sysver &&
			obj.min_version <= sysver;
		});
	// if we have an iterator , then we will retrieve the index of the iterator
	if (it != system_hacks.end())  
		index = std::distance(system_hacks.begin(), it);
	return index;
}
void _showError(const char* errorMsg, ...)
{
	char astr[1024];
	memset(astr, 0, 1024);

	va_list ap;
	va_start(ap, errorMsg);

	vsnprintf(astr, 1024, errorMsg, ap);

	va_end(ap);

	PrintString(1, ((640 / 2) - ((strnlen(astr,1024)) * 13 / 2)) >> 1, 208, astr);
	sleep(5);
	return;
}

std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of("\t ");
    if (std::string::npos == first)
        return str;

    size_t last = str.find_last_not_of("\t ");
    return str.substr(first, (last - first + 1));
}

//we use our own GetLine instead of ifstream because ifstream would increase priiloader's size with 300KB...
//and eventhough not perfect, this works fine too
bool GetLine(bool reading_nand, std::string& line)
{
	char* buf = NULL;	
	std::string read_line;
	u32 read_cnt = 0;
	u32 file_pos = 0;
	const u16 max_size = 0x2000;

	//Read untill a newline is found
	try
	{
		//get current file position
		if (reading_nand)
		{	
			STACK_ALIGN(fstats, status, sizeof(fstats), 32);
			ISFS_GetFileStats(nand_file_handler, status);
			file_pos = status->file_pos;
		}
		else
		{
			file_pos = ftell(sd_file_handler);
		}

		buf = (char*)mem_align(32,max_size+1);
		if (!buf)
		{
			error = ERROR_MALLOC;
			throw "error allocating buffer";
		}
		memset(buf,0,max_size+1);

		//read untill we have a newline or reach EOF
		while (
				file_pos < file_size &&
				file_pos+strnlen(buf,max_size) < file_size &&
				strstr(buf, "\n") == NULL && 
				strstr(buf, "\r") == NULL
			)
		{	
			//are we dealing with a potential overflow?
			if (read_cnt > max_size)
			{
				error = ERROR_MALLOC;
				throw "line to long";
			}

			u32 addr = read_cnt + (u32)buf;
			s32 ret = 0;

			if (reading_nand)
				ret = ISFS_Read(nand_file_handler, (void*)addr, BLOCK_SIZE);
			else
				ret = fread( (void*)addr, sizeof( char ), BLOCK_SIZE, sd_file_handler );

			if(ret <= 0)
			{
				std::string err = "Error reading from ";
				err.append(reading_nand?"NAND":"SD");
				err.append(" (" + std::to_string(ret) + ")");
				throw err;
			}
			read_cnt += ret;

			if(strnlen(buf,max_size+1) >= max_size)
			{
				throw "buf has overflown";
			}
		}

		//nothing was read
		if(!buf)
		{
			line = "";
			return false;
		}

		read_line = std::string(buf);
		mem_free(buf);

		//find the newline and split the string
		std::string cut_string = read_line.substr(0,read_line.find("\n"));
		if(cut_string.length() == 0)
			cut_string = read_line.substr(0,read_line.find("\r"));

		//set the new file position untill after the \r\n, \r or \n
		file_pos += cut_string.size()+1;

		//cut off any carriage returns
		if(cut_string.back() == '\r')
			cut_string = cut_string.substr(0, cut_string.length() - 1);
		if(cut_string.front() == '\r')
			cut_string = cut_string.substr(1, cut_string.length() - 1);

		line = cut_string;

		//seek file back correctly
		if (reading_nand)
		{	
			ISFS_Seek(nand_file_handler, file_pos, SEEK_SET);
		}
		else
		{
			fseek( sd_file_handler, file_pos, SEEK_SET);
		}
		return line.length() > 0;
	}
	catch (char const* ex)
	{
		gprintf("GetLine Exception : %s",ex);
	}
	catch (const std::string& ex)
	{
		gprintf("GetLine Exception : %s",ex);
	}
	catch(...)
	{	
		gprintf("GetLine General Exception");
	}

	if (buf)
		mem_free(buf);
	line = "";
	return false;
}
bool _processLine(system_hack& hack, std::string &line)
{
	try
	{
		gdprintf("processing '%s'",line.c_str());

		//if the line has a comment we will strip everything after the comment character
		if(line.find("#") != std::string::npos)
		{
			line = line.substr(0,line.find("#"));
		}

		//trim all trailing spaces. if we end up with an empty line we just return
		line = trim(line);
		if(line.size() == 0) 
			return true;

		if(line.front() == '[' && line.back() == ']')
		{
			line = line.substr(1,line.length()-2);
			hack.desc = line;
			return true;
		}

		if(line.find("=") == std::string::npos)
			throw "Invalid line";	

		std::string type = line.substr(0,line.find("="));
		std::string value = line.substr(line.find("=")+1);
		system_patch* temp_patch;

		if(hack.patches.size() <= 0)
		{
			hack.patches.push_back(system_patch());
		}
		temp_patch = &hack.patches.back();

		if(type.length() == 0 || value.size() == 0)
			throw "failed to split line";

		if(type == "amount")
		{
			//not needed line, but we used to have it. we do nothing with it though...
			//hack.amount = strtoul(values.front().c_str(),NULL, 10);
		}
		else if(type == "master")
		{
			// Hacks with a "master" parameter are hidden, and can be enabled
			// by other hacks using the "require" parameter
			hack.masterID = value;
			return true; 
		}
		else if(type == "require")
		{
			hack.requiredID = value;
			return true; 
		}
		else if(type == "maxversion")
		{
			hack.max_version = strtoul(value.c_str(),NULL, 10);
			return true;
		}
		else if(type == "minversion")
		{
			hack.min_version = strtoul(value.c_str(),NULL, 10);
			return true;
		}
		else if(type == "patch" || type == "hash")
		{
			//process patch or hash
			// patch example : patch=0x38000001,0x2c000000,0x900DA5D8,0x38000032
			//  hash example : hash=0x38000000,0x2c000000,0x40820010,0x38000036,0x900da9c8,0x480017
			//should get in the struct as 0x380000002c0000004082001038000036900da9c8480017

			//start new patch if the last patch is already a complete one
			if(	type == "hash" && 
				temp_patch->patch.size() > 0 && 
				(
					temp_patch->hash.size() > 0 || 
					temp_patch->offset != 0 
				)
			  )
			{
				hack.patches.push_back(system_patch());
				temp_patch = &hack.patches.back();
			}

			//split the value string into seperate bits
			while(value.length() > 0)
			{			
				std::string segment;
				if(value.find(",") != std::string::npos)
				{
					segment = value.substr(0,value.find(","));
					value = value.substr(value.find(",")+1);
				}
				else
				{
					segment = value;
					value.clear();
				}			

				while(segment.find(" ") != std::string::npos)
						segment = segment.replace(segment.find(" "),1,"");	

				if(segment.length() < 4 || segment.length() > 10 || segment.compare(0,2,"0x"))
					throw "Invalid " + type + " : " + segment;

				//cut off the '0x'
				segment = segment.substr(2);

				while(segment.size() > 0)
				{
					std::string subSegment;
					if(segment.size() > 2)
					{
						subSegment = segment.substr(0,2);
						segment = segment.substr(2);
					}
					else
					{
						subSegment = segment;
						segment.clear();
					}

					if(subSegment.substr(2) != "0x")
						subSegment.insert(0,"0x");

					if(type == "patch")
						temp_patch->patch.push_back(strtoul(subSegment.c_str(),NULL, 16));
					else if(type == "hash")
						temp_patch->hash.push_back(strtoul(subSegment.c_str(),NULL, 16));					
				}
			}
		}
		else if(type == "offset")
		{
			//process offset
			//example : offset=0x81000000
			if(value.size() != 10)
				throw ("Invalid offset : " + std::to_string(value.size()));

			//save the old patch and start a new one.
			if(temp_patch->patch.size() > 0 && ( temp_patch->hash.size() > 0 || temp_patch->offset != 0 ))
			{
				hack.patches.push_back(system_patch());
				temp_patch = &hack.patches.back();
			}

			u32 offset = strtoul(value.c_str(),NULL, 16);
			temp_patch->offset = offset;
		}
		else
		{
			throw "unknown type : " + type;
		}
		return true;
	}
	catch (const std::string& ex)
	{
		gprintf("_processLine Exception: %s",ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("_processLine Exception: %s",ex);
	}
	catch(...)
	{
		gprintf("_processLine General Exception: invalid line");		
	}	

	if(line.length() > 0)
		gprintf("line : %s",line.c_str());

	return false;
}
bool _addOrRejectHack(system_hack& hack)
{
	try
	{
		if(
			hack.min_version > 0 &&
			hack.max_version > 0 &&
			hack.desc.length() > 0 &&
			hack.patches.size() > 0 &&
			( hack.patches.back().hash.size() > 0 || hack.patches.back().offset > 0) && 
			hack.patches.back().patch.size() > 0 &&
			( hack.requiredID.size() == 0 || GetMasterHackIndexByID(hack.requiredID) >= 0 )
			)
		{			
			gdprintf("loaded %s (v%d - v%d)",hack.desc.c_str(),hack.min_version,hack.max_version);
			system_hacks.push_back(hack);
			return true;
		}

		gdprintf("dropping invalid hack %s",hack.desc.c_str());
		if( hack.requiredID.size() != 0 && GetMasterHackIndexByID(hack.requiredID) < 0 )
			gdprintf("did not find ID");
		return false;
	}
	catch(...)
	{
		gprintf("General _addOrRejectHack exception thrown");
		return false;
	}
}
s8 LoadSystemHacks(bool load_nand)
{
	//system_hacks already loaded
	if (system_hacks.size() || states_hash.size()) 
	{
		system_hacks.clear();
		states_hash.clear();
	}

	//clear any possible open handlers
	if (nand_file_handler >= 0)
	{
		ISFS_Close(nand_file_handler);
		nand_file_handler = -1;
	}
	if (sd_file_handler != NULL)
	{
		fclose(sd_file_handler);
		sd_file_handler = NULL;
	}

	//read the hacks file size
	if (!load_nand)
	{
		sd_file_handler = fopen("fat:/apps/priiloader/hacks_hash.ini","rb");
		if(!sd_file_handler)
		{
			gprintf("fopen error : %s", strerror(errno));
		}
		else
		{
			fseek( sd_file_handler, 0, SEEK_END );
			file_size = ftell(sd_file_handler);
			fseek( sd_file_handler, 0, SEEK_SET);
		}
	}

	//no file opened from FAT device, so lets open the nand file
	if (!sd_file_handler)
	{
		load_nand = true;
		nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1);
		if (nand_file_handler < 0)
		{
			gprintf("LoadHacks : hacks_hash.ini not on FAT or Nand. ISFS_Open error %d", nand_file_handler);
			return 0;
		}

		STACK_ALIGN(fstats, status, sizeof(fstats), 32);
		ISFS_GetFileStats(nand_file_handler, status);
		file_size = status->file_length;
	}

	if (file_size == 0)
	{
		if (!load_nand)
			_showError("Error \"hacks_hash.ini\" is 0 byte!");

		if (load_nand)
		{
			ISFS_Close(nand_file_handler);
			nand_file_handler = -1;
		}			
		else
		{
			if(sd_file_handler)
				fclose(sd_file_handler);
			sd_file_handler = NULL;
		}

		return 0;
	}

	//Process File
	std::string line;
	system_hack new_hack;

	while (GetLine(load_nand,line))
	{
		//Specs of this loop/function : 
		// - read the line and put it in the correct part of the hack
		// - if we read a description while we already have one, we will check if the hack is complete, and push it to the list if it is
		//																								-> if it isn't complete, drop it
		// - the same will be done with patches
		// - if hash or offset is found -> we don't have a patch yet		-> drop patch and start anew
		//								-> we have a patch					-> we start a new one
		// - if patch is found			-> and we don't have a hash/offset	-> ignore
		//								-> we have a hash/offset			-> append
		// - amount will be ignored. its not really needed...
		//_processLine will place everything in the correct spot in the struct/class & add new patches if needed

		//new hack? process previous one and add it
		if(line.front() == '[' && line.back() == ']')
		{
			_addOrRejectHack(new_hack);
			//new_hack.amount = 0;
			new_hack.desc.clear();
			new_hack.masterID.clear(); 
			new_hack.requiredID.clear(); 
			new_hack.max_version = 0;
			new_hack.min_version = 0;
			new_hack.patches.clear();
		}

		_processLine(new_hack,line);
	}

	//add or reject the last hack we were working on
	_addOrRejectHack(new_hack);

	//cleanup on aisle 4
	if (load_nand)
	{
		ISFS_Close(nand_file_handler);
		nand_file_handler = -1;
	}
	else
	{
		if(sd_file_handler)
			fclose(sd_file_handler);
		sd_file_handler = NULL;
	}

	//Load the states_hash
	nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2);

	if(nand_file_handler > 0)
	{
		STACK_ALIGN(fstats, status, sizeof(fstats), 32);
		memset( status, 0, sizeof(fstats) );

		if(ISFS_GetFileStats( nand_file_handler, status)<0)
		{
			ISFS_Close( nand_file_handler );
			return 0;
		}

		//if the file is not the same size as the loaded hacks, we ignore the file
		if(status->file_length == system_hacks.size())
		{
			u8 *fbuf = (u8 *)mem_align( 32, ALIGN32(status->file_length) );
			if( fbuf == NULL )
			{
				error = ERROR_MALLOC;
				ISFS_Close( nand_file_handler );
				return 0;
			}
			memset( fbuf, 0, status->file_length );

			if(ISFS_Read( nand_file_handler, fbuf, status->file_length )< (s32)status->file_length )
			{
				ISFS_Close( nand_file_handler );
				mem_free(fbuf);
				return 0;
			}

			for(u32 i = 0;i < status->file_length;i++)
			{
				states_hash.push_back(fbuf[i]);
			}		

			mem_free(fbuf);

			/*	Disable all hacks that are either not for this version 
				-OR- 
				master hacks (as they will be re-enabled later if needed)*/
			u32 sysver = GetSysMenuVersion();
			for( u32 i=0; i < system_hacks.size(); ++i)
			{
				
				if (system_hacks[i].masterID.length() > 0 || 
					system_hacks[i].max_version < sysver || 
					system_hacks[i].min_version > sysver)	
				{
					states_hash[i] = 0; 
				}			
			}
		}
	}

	//the hack sizes don't match. reset the states
	if(states_hash.size() != system_hacks.size())
	{
		states_hash.clear();
		while(states_hash.size() != system_hacks.size())
		{
			states_hash.push_back(0);
		}
	}
	
	//if an actual ISFS error was given -> recreate
	if (nand_file_handler < -1)
	{
		//delete file if it exists
		ISFS_Delete("/title/00000001/00000002/data/hacksh_s.ini");

		//file not found or not correct size - create a new one
		nand_file_handler = ISFS_CreateFile("/title/00000001/00000002/data/hacksh_s.ini", 0, 3, 3, 3);
		if(nand_file_handler < 0 )
		{		
			gprintf("LoadHacks : hacksh_s(states) could not be created. ISFS_CreateFile error %d", nand_file_handler);
			return 0;
		}

		nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );
		if( nand_file_handler < 0 )
		{
			gprintf("LoadHacks : hacksh_s(states) could not be loaded. ISFS_Open error %d", nand_file_handler);
			return 0;
		}

		ISFS_Write( nand_file_handler, &states_hash[0], sizeof( u8 ) * system_hacks.size() );
	}	

	if(nand_file_handler >= 0)
		ISFS_Close( nand_file_handler );
	nand_file_handler = -1;

	return 1;
}
