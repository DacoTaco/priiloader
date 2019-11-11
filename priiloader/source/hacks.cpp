/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar

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
#include <fstream>

#include "gecko.h"
#include "hacks.h"
#include "settings.h"
#include "error.h"
#include "font.h"
#include "mem2_manager.h"

std::vector<system_hack> system_hacks;
std::vector<u8> states_hash;

std::ifstream* sd_file_handler = NULL;
s32 nand_file_handler = -1;

void _showError(const char* errorMsg, ...)
{
	char astr[2048];
	memset(astr, 0, 2048);

	va_list ap;
	va_start(ap, errorMsg);

	vsnprintf(astr, 2047, errorMsg, ap);

	va_end(ap);

	PrintString(1, ((640 / 2) - ((strlen(astr)) * 13 / 2)) >> 1, 208, astr);
	sleep(5);
	return;
}

bool GetLine(bool reading_nand, std::string& line)
{
	char* buf = NULL;	
	try
	{
		//Read untill a newline is found		
		if (reading_nand)
		{
			u32 read_count = 0;
			u32 file_pos = 0;
			const u16 max_loop = 1000;
			const u32 block_size = 32;

			//get current file position
			STACK_ALIGN(fstats, status, sizeof(fstats), 32);
			ISFS_GetFileStats(nand_file_handler, status);
			file_pos = status->file_pos;

			//read untill we have a newline or reach EOF
			while (
					file_pos < status->file_length &&
					( 
						buf == NULL || 
						(
							file_pos+strnlen(buf,block_size*max_loop) < status->file_length &&
							strstr(buf, "\n") == NULL && 
							strstr(buf, "\r") == NULL
						)
					)
				)
			{						
				read_count++;
				//are we dealing with a potential overflow?
				if (read_count > max_loop)
				{
					error = ERROR_MALLOC;
					throw "line to long";
				}

				if (buf)
					buf = (char*)mem_realloc(buf, ALIGN32(block_size*read_count));
				else
					buf = (char*)mem_align(32, block_size*read_count);

				if (!buf)
				{
					error = ERROR_MALLOC;
					throw "error allocating buffer";
				}
				
				u32 addr = (u32)buf;
				if (read_count > 1)
					addr += block_size*(read_count - 1);
				else
					memset(buf, 0, block_size*read_count);

				s32 ret = ISFS_Read(nand_file_handler, (void*)addr, block_size);
				if (ret < 0)
				{
					throw "Error reading from NAND";
				}

				if(strlen(buf) > block_size*max_loop)
				{
					throw "buf has overflown its max size during loop";
				}
			}

			//nothing was read
			if(!buf)
			{
				line = "";
				return false;
			}

			std::string read_line(buf);
			mem_free(buf);

			//find the newline and split the string
			std::string cut_string = read_line.substr(0,read_line.find("\n"));
			if(cut_string.length() == 0)
				std::string cut_string = read_line.substr(0,read_line.find("\r"));

			//set the new file position untill after the \r\n, \r or \n
			file_pos += cut_string.length();

			//cut off any carriage returns
			if(cut_string.back() == '\r')
			{
				cut_string = cut_string.substr(0, cut_string.length() - 1);
				file_pos++;
			}
			if(cut_string.front() == '\r')
				cut_string = cut_string.substr(1, cut_string.length() - 1);

			line = cut_string;
			
			//seek file back correctly
			ISFS_Seek(nand_file_handler, file_pos, SEEK_SET);
		}
		else
		{
			//SD is so much easier... xD
			std::string read_line;
			std::getline(*sd_file_handler, read_line);
			if (read_line.back() == '\r')
				read_line = read_line.substr(0, read_line.length() - 1);
			if (read_line.front() == '\r')
				read_line = read_line.substr(0, read_line.length() - 1);

			line = read_line;
		}
		return line.length() > 0;
	}
	catch (char const* ex)
	{
		if (buf)
			mem_free(buf);

		gprintf("GetLine Exception was thrown : %s\r\n",ex);

		line = "";
		return false;
	}
	catch (const std::string& ex)
	{
		if (buf)
			mem_free(buf);

		gprintf("GetLine Exception was thrown : %s\r\n",ex);

		line = "";
		return false;
	}
	catch(...)
	{
		if (buf)
			mem_free(buf);
		
		gprintf("GetLine General Exception thrown\r\n");

		line = "";
		return false;
	}
}
bool _processLine(system_hack& hack, std::string &line)
{
	try
	{
		gprintf("processing line : %s\r\n", line.c_str());
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
		std::vector<std::string> values;
		system_patch* temp_patch;

		if(hack.patches.size() <= 0)
		{
			hack.patches.push_back(system_patch());
		}
		temp_patch = &hack.patches.back();
		gprintf("address of temp_patch : 0x%08X\r\n",(u32)temp_patch);

		while(value.length() > 0)
		{
			std::string newValue;
			if(value.find(",") != std::string::npos)
			{
				newValue = value.substr(0,value.find(","));
				while(newValue.find(" ") != std::string::npos)
					newValue = newValue.replace(newValue.find(" "),1,"");

				value = value.substr(value.find(",")+1);
			}
			else
			{
				newValue = value;
				while(newValue.find(" ") != std::string::npos)
					newValue = newValue.replace(newValue.find(" "),1,"");

				value.clear();
			}			
			values.push_back(newValue);				
		}

		if(type.length() <= 0 || values.size() <= 0)
			throw "failed to split line into type and values";

		if(type == "amount")
		{
			//not needed line, but we used to have it. we do nothing with it though...
			hack.amount = strtoul(values.front().c_str(),NULL, 10);
		}
		else if(type == "maxversion")
		{
			hack.max_version = strtoul(values.front().c_str(),NULL, 10);
			return true;
		}
		else if(type == "minversion")
		{
			hack.min_version = strtoul(values.front().c_str(),NULL, 10);
			return true;
		}
		else if(type == "patch")
		{
			//process patch
			//example : patch=0x38000001,0x2c000000,0x900DA5D8,0x38000032
			for(u32 i = 0;i < values.size();i++)
			{				
				value = values.at(i);
				if(value.length() < 4 || value.length() > 10 || value.find("0x") != 0 )
					throw "Invalid patch value : " + value;

				value = value.substr(2);

				//start new patch if we already have one
				if(i == 0 && temp_patch->patch.size() != 0 && 
					( temp_patch->hash.size() > 0 || temp_patch->offset != 0 ))
				{
					hack.patches.push_back(system_patch());
					temp_patch = &hack.patches.back();
				}
				
				while(value.size() > 0)
				{
					std::string str_patch;
					u8 patch;
					if(value.size() > 2)
					{
						str_patch = "0x" + value.substr(0,2);
						value = value.substr(2);
					}
					else
					{
						str_patch = value;
						value.clear();
					}
					patch = strtoul(str_patch.c_str(),NULL, 16);
					gprintf("converted to 0x%02X\r\n",patch);
					temp_patch->patch.push_back(patch);
					gprintf("pushed\r\n");
				}
			}
		}
		else if(type == "hash")
		{
			//process hash
			//example : hash=0x38000000,0x2c000000,0x40820010,0x38000036,0x900da9c8,0x480017
			//should get in the struct as 0x380000002c0000004082001038000036900da9c8480017

			for(u32 i = 0;i < values.size();i++)
			{				
				value = values.at(i);
				if(value.length() < 4 || value.length() > 10 || value.find("0x") != 0)
					throw "Invalid hash value : " + value;
				value = value.substr(2);

				//start new patch if we already have one
				if(i == 0 && temp_patch->patch.size() > 0 &&
					( temp_patch->hash.size() > 0 || temp_patch->offset != 0 ))
				{
					hack.patches.push_back(system_patch());
					temp_patch = &hack.patches.back();
				}
				
				while(value.size() > 0)
				{
					std::string str_hash;
					u8 hash;
					if(value.size() > 2)
					{
						str_hash = "0x" + value.substr(0,2);
						value = value.substr(2);
					}
					else
					{
						str_hash = value;
						value.clear();
					}
					hash = strtoul(str_hash.c_str(),NULL, 16);
					gprintf("converted to 0x%02X\r\n",hash);
					temp_patch->hash.push_back(hash);
					gprintf("pushed\r\n");
					str_hash.clear();
				}
			}
		}
		else if(type == "offset")
		{
			//process offset
			//example : offset=0x81000000
			if(values.front().size() != 10)
				throw "Invalid offset value : " + value;

			//save the old patch and start a new one.
			if(temp_patch->patch.size() > 0 && ( temp_patch->hash.size() > 0 || temp_patch->offset != 0 ))
			{
				hack.patches.push_back(system_patch());
				temp_patch = &hack.patches.back();
			}

			u32 offset = strtoul(values.front().c_str(),NULL, 16);
			temp_patch->offset = offset;
		}
		else
		{
			throw "unknown line type : " + type;
		}
		return true;
	}
	catch (const std::string& ex)
	{
		gprintf("_processLine Exception -> %s\r\n",ex.c_str());
		if(line.length() > 0)
			gprintf("line : %s\r\n",line.c_str());

		return false;
	}
	catch (char const* ex)
	{
		gprintf("_processLine Exception -> %s\r\n",ex);
		if(line.length() > 0)
			gprintf("line : %s\r\n",line.c_str());

		return false;
	}
	catch(...)
	{
		gprintf("_processLine General Exception : invalid line to process\r\n");		
		if(line.length() > 0)
			gprintf("line : %s\r\n",line.c_str());

		return false;
	}	
}
bool _addOrRejectHack(system_hack& hack)
{
	try
	{
		if(
			hack.min_version != 0 &&
			hack.max_version != 0 &&
			hack.desc.length() > 0 &&
			hack.patches.size() > 0 &&
			( hack.patches.back().hash.size() > 0 || hack.patches.back().offset > 0) && 
			hack.patches.back().patch.size() > 0
			)
		{
			gdprintf("loaded %s (v%d - v%d)\r\n",hack.desc.c_str(),hack.min_version,hack.max_version);
			system_hacks.push_back(hack);
			return true;
		}

		gdprintf("dropping hack %s\r\n",hack.desc.c_str());
		return false;
	}
	catch(...)
	{
		gprintf("General _addOrRejectHack exception thrown\r\n");
		return false;
	}
}
s8 LoadSystemHacks(bool load_nand)
{
	u32 file_size = 0;

	//system_hacks already loaded
	if (system_hacks.size()) 
	{
		system_hacks.clear();
		states_hash.clear();
	}

	//clear any possible open handlers
	if (load_nand && nand_file_handler >= 0)
	{
		ISFS_Close(nand_file_handler);
		nand_file_handler = -1;
	}
	else if (!load_nand && sd_file_handler != NULL)
	{
		if(sd_file_handler->is_open())
			sd_file_handler->close();

		delete sd_file_handler;
		sd_file_handler = NULL;
	}

	//read the hacks file size
	if (!load_nand)
	{
		sd_file_handler = new std::ifstream("fat:/apps/priiloader/hacks_hash.ini");
		if (!sd_file_handler || !sd_file_handler->is_open())
		{
			sd_file_handler = NULL;
			gprintf("fopen error : strerror %s\r\n", strerror(errno));
		}
		else
		{
			sd_file_handler->seekg(0, std::ios::end);
			file_size = sd_file_handler->tellg();

			sd_file_handler->seekg(0, std::ios::beg);
		}		
	}

	//no file opened from FAT device, so lets open the nand file
	if (!sd_file_handler)
	{
		load_nand = true;
		nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1);
		if (nand_file_handler < 0)
		{
			gprintf("LoadHacks : hacks_hash.ini not on FAT or Nand. ISFS_Open error %d\r\n", nand_file_handler);
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
			sd_file_handler->close();
			delete sd_file_handler;
			sd_file_handler = NULL;
		}

		return 0;
	}

	//Process File
	std::string line;
	system_hack new_hack;
	gprintf("going into loop\r\n");

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
			new_hack.amount = 0;
			new_hack.desc.clear();
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
		sd_file_handler->close();
		delete sd_file_handler;
		sd_file_handler = NULL;
	}

	//Load the states_hash
	nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2);

	if(nand_file_handler > 0)
	{
		STACK_ALIGN(fstats, status, sizeof(fstats), 32);
		memset( status, 0, sizeof(fstats) );

		if(ISFS_GetFileStats( nand_file_handler, status)<0)
			return 0;

		u8 *fbuf = (u8 *)mem_align( 32, ALIGN32(status->file_length) );
		if( fbuf == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
		memset( fbuf, 0, status->file_length );

		if(ISFS_Read( nand_file_handler, fbuf, status->file_length )<0)
		{
			mem_free(fbuf);
			return 0;
		}
		ISFS_Close( nand_file_handler );
		nand_file_handler = -1;

		for(u32 i = 0;i < status->file_length;i++)
		{
			states_hash.push_back(fbuf[i]);
		}		

		mem_free(fbuf);
	}

	if (nand_file_handler < 0 || states_hash.size() != system_hacks.size())
	{
		//file not found or not correct size - create a new one
		nand_file_handler = ISFS_CreateFile("/title/00000001/00000002/data/hacksh_s.ini", 0, 3, 3, 3);
		if(nand_file_handler < 0 )
		{		
			gprintf("LoadHacks : hacksh_s(states) could not be created. ISFS_CreateFile error %d\r\n", nand_file_handler);
			return 0;
		}

		nand_file_handler = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );
		if( nand_file_handler < 0 )
		{
			gprintf("LoadHacks : hacksh_s(states) could not be loaded. ISFS_Open error %d\r\n", nand_file_handler);
			return 0;
		}

		states_hash.clear();
		while(states_hash.size() != system_hacks.size())
			states_hash.push_back(0);

		ISFS_Write( nand_file_handler, &states_hash[0], sizeof( u32 ) * system_hacks.size() );

		ISFS_Close(nand_file_handler);
		nand_file_handler = -1;
	}	

	//Set all system_hacks for system menu > max_version || sysver < min_version to 0
	unsigned int sysver = GetSysMenuVersion();
	for( u32 i=0; i < system_hacks.size(); ++i)
	{
		if( system_hacks[i].max_version < sysver || system_hacks[i].min_version > sysver)
		{
			states_hash[i] = 0;
		}
	}

	return 1;
}
