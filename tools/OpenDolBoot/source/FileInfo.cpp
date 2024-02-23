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
#include <memory>
#include "../include/FileInfo.hpp"

unsigned int FileInfo::GetFileSize()
{
	return Data.size();
}

const char* FileInfo::GetFilename()
{
	return FileName.c_str();
}

FileInfo::FileInfo(const std::string& filename, bool readData)
{
	if(filename.size() == 0)
		throw "invalid filename";
	
	FileName = filename;
	if(!readData)
		return;

	//open a pointer that is managed by c++, that does fopen to create it, and fclose when it goes out of scope
	auto file = std::unique_ptr<std::FILE, int(*)(std::FILE*)>(std::fopen(FileName.c_str(), "rb"), &std::fclose);
	if(file == nullptr)
		throw "failed to open " + FileName;

	std::fseek(file.get(), 0, SEEK_END);
    auto fileSize = std::ftell(file.get());
    std::rewind(file.get());

	//copy the file into the buffer:
	Data.resize(fileSize);
    if (fread(&Data[0], 1, fileSize, file.get()) != fileSize) 
        throw "failed to read file data";    
}

FileInfo::FileInfo(const std::string& filename, const unsigned char* data, const unsigned int size)
{
	if(filename.size() == 0)
		throw "invalid filename";
	
	FileName = filename;
	Data = std::vector<unsigned char>((unsigned char*)data, ((unsigned char*)data) + size);
}

void FileInfo::WriteFile()
{
	//open a pointer that is managed by c++, that does fopen to create it, and fclose when it goes out of scope
	auto file = std::unique_ptr<std::FILE, int(*)(std::FILE*)>(std::fopen(FileName.c_str(), "wb"), &std::fclose);
	if(file == nullptr)
		throw "failed to open " + FileName + " for writing";
	
	std::fwrite(&Data[0], 1, Data.size(), file.get());
}