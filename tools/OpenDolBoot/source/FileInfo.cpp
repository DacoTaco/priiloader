/*
OpenDolBoot - An utiltiy to make a dol binary into a title bootable binary on the Wii/vWii

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
#include <string.h>
#include <vector>
#include "../include/FileInfo.hpp"

FileInfo::FileInfo(const std::string filename)
{
	FileName = filename;
	Data = NULL;
}

FileInfo::~FileInfo()
{
	if(Data != NULL)
	{
		free(Data);
		Data = NULL;
		FileSize = 0;	
	}
}

unsigned int FileInfo::GetFileSize()
{
	return FileSize;
}

const char* FileInfo::GetFilename()
{
	return FileName.c_str();
}

void FileInfo::AllocateMemory(const unsigned int size)
{
	if(Data != NULL)
	{
		free(Data);
		Data = NULL;
	}

	Data = (unsigned char*)malloc(size);
	if(Data == NULL)
		throw "failed to alloc memory";	
	
	memset(Data, 0, size);
	FileSize = size;
}

void FileInfo::ReadFile()
{
	if(FileName.size() == 0)
		throw "invalid filename";
	
	FILE* file = fopen(FileName.c_str(), "rb");
	if(!file)
		throw "failed to open " + FileName;	

	fseek(file, 0, SEEK_END);
	FileSize = ftell(file);
	rewind(file);

	// allocate memory to contain the whole file:
	Data = (unsigned char*)malloc(FileSize);
	if (Data == NULL)
	{
		fclose(file);
		throw "failed to alloc memory";	
	}

	memset(Data, 0, FileSize);
	//copy the file into the buffer:
	if (fread(Data, 1, FileSize, file) != FileSize) 
	{
		fclose(file);
		throw "failed to read file data";	
	}

	fclose(file);	
}

void FileInfo::WriteFile()
{
	FILE* file = fopen(FileName.c_str(), "wb+");
	if(!file)
		throw "failed to open " + FileName;	

	fwrite(Data, 1, FileSize, file);
	fclose(file);
}

